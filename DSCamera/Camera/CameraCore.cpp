#include "CameraCore.h"
#include <QDateTime>
#include <QDebug>

CameraCore::CameraCore(QObject *parent)
    : QObject{parent}
{
    // 视频预览回调
    auto preview_callback = [this](const QImage &img){
        // static int i = 0;
        // qDebug()<<"grabber"<<i++<<QTime::currentTime()<<img.isNull()<<img.size()<<img.bytesPerLine()<<img.format();
        QMetaObject::invokeMethod(this, "imageComing", Qt::QueuedConnection,
                                  Q_ARG(QImage, img));
        probe.previewUpdate(img);
    };
    // 物理按键拍图回调
    auto still_callback = [this](const QImage &img){
        static int i = 0;
        qDebug()<<"still"<<i++<<QTime::currentTime()<<img.isNull()<<img.size();
        probe.stillUpdate(img);
    };
    setCallback(preview_callback, still_callback);

    // 检测摄像头热插拔
    hotplug.init(QVector<QUuid>() << QUuid(0x65E8773DL, 0x8F56, 0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96));
    // 插入设备刷新列表
    connect(&hotplug, &CameraHotplug::deviceAttached, this, [this](){
            auto device_list = info.getDeviceList();
            int index = getDeviceIndex();
            if (index < 0 || index >= device_list.size()) {
                info.updateDeviceList();
                if (info.getDeviceList().size() > 0) {
                    selectDevice(0);
                }
                return;
            }
            CameraDevice device = device_list.at(index);
            // 更新设备列表
            info.updateDeviceList();
            device_list = info.getDeviceList();
            for (int i = 0; i < device_list.size(); i++)
            {
                if (device_list.at(i).displayName == device.displayName) {
                    // TODO 检测之前是否拔出，不然每插一个都重新加载
                    selectDevice(i);
                    break;
                }
            }
            emit deviceIndexChanged();
        }, Qt::QueuedConnection);
    // 移除设备暂不处理
    // connect(&hotplug, &CameraHotplug::deviceDetached, this, [this](){}, Qt::QueuedConnection);

    // 初始化设备信息，默认选中一个设备
    info.updateDeviceList();
    if (info.getDeviceList().size() > 0) {
        selectDevice(0);
    }
}

CameraCore::~CameraCore()
{
    stop();
    releaseGraph();
}

CameraInfo *CameraCore::getInfo()
{
    return &info;
}

CameraProbe *CameraCore::getProbe()
{
    return &probe;
}

CameraHotplug *CameraCore::getHotplug()
{
    return &hotplug;
}

int CameraCore::getState() const
{
    return state;
}

void CameraCore::setState(int newState)
{
    if (state != newState) {
        state = (CameraCore::CameraState)newState;
        emit stateChanged(state);
    }
}

int CameraCore::getDeviceIndex() const
{
    return mSetting.deviceIndex;
}

void CameraCore::setDeviceIndex(int index)
{
    if (mSetting.deviceIndex != index) {
        mSetting.deviceIndex = index;
        emit deviceIndexChanged();
    }
}

QSize CameraCore::getResolution() const
{
    return QSize(mSetting.width, mSetting.height);
}

void CameraCore::attachView(CameraView *view)
{
    if (!view) {
        return;
    }
    connect(this, &CameraCore::imageComing, view, &CameraView::updateImage, Qt::QueuedConnection);
}

void CameraCore::setCallback(const std::function<void (const QImage &)> &previewCB,
                             const std::function<void (const QImage &)> &stillCB)
{
    mPreviewCallback.callback = previewCB;
    mStillCallback.callback = stillCB;
}

bool CameraCore::selectDevice(int index)
{
    auto device_list = info.getDeviceList();
    if (index < 0 || index >= device_list.size())
        return false;
    CameraDevice device = device_list.at(index);
    quint16 vid, pid;
    if (!CameraInfo::getVidPid(device.displayName, vid, pid)) {
        return false;
    }
    // 根据 vid pid 区分设备，公司老的设备不支持jpg
    // 目前我只需要转 jpg 和 rgb，其他的原生格式解析可以参考：
    // https://github.com/GoodRon/QtWebcam
    // 衍生版本：https://gitee.com/fsfzp888/UVCCapture
    mSetting.isJpg = !(pid == 0x5000 || pid == 0x7585);
    setDeviceIndex(index);
    // 释放当前
    releaseGraph();

    HRESULT hr;
    // 创建Filter Graph Manager.
    hr = ::CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, reinterpret_cast<void **>(&mGraph));
    if (FAILED(hr))
        return false;

    // 创建Capture Graph Builder.
    hr = ::CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, IID_ICaptureGraphBuilder2, reinterpret_cast<void **>(&mBuilder));
    if (FAILED(hr))
        return false;
    mBuilder->SetFiltergraph(mGraph);

    // IMediaControl接口，用来控制流媒体在Filter Graph中的流动，例如流媒体的启动和停止；
    hr = mGraph->QueryInterface(IID_IMediaControl, reinterpret_cast<void **>(&mMediaControl));
    if (FAILED(hr))
        return false;

    // MFC版用到，暂时移除
    // hr = mGraph->QueryInterface(IID_IVideoWindow, reinterpret_cast<LPVOID *>(&mVideoWindow));
    // hr = mGraph->QueryInterface(IID_IMediaEvent, reinterpret_cast<LPVOID *>(&gME));

    // 创建用于预览的Sample Grabber Filter.
    hr = ::CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void **>(&mPreviewFilter));
    if (FAILED(hr))
        return false;

    // 获取ISampleGrabber接口，用于设置回调等相关信息
    hr = mPreviewFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void **>(&mPreviewGrabber));
    if (FAILED(hr))
        return false;

    // 创建Null Filter
    hr = ::CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void **>(&mPreviewNull));
    if (FAILED(hr))
        return false;

    // 创建用于抓拍的Sample Grabber Filter.
    hr = ::CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void **>(&mStillFilter));
    if (FAILED(hr))
        return false;

    // 获取ISampleGrabber接口，用于设置回调等相关信息
    hr = mStillFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void **>(&mStillGrabber));
    if (FAILED(hr))
        return false;

    // 创建Null Filter
    hr = ::CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void **>(&mStillNull));
    if (FAILED(hr))
        return false;

    // MFC版用到，暂时移除
    // hr = ::CoCreateInstance(CLSID_EZrgb24, 0, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void **>(&mTransformFilter));
    if (!bindFilter(device.displayName))
        return false;

    hr = mGraph->AddFilter(mSourceFilter, L"Source Filter");
    if (FAILED(hr))
        return false;
    hr = mGraph->AddFilter(mPreviewFilter, L"Preview Filter");
    if (FAILED(hr))
        return false;
    hr = mGraph->AddFilter(mStillFilter, L"Still Filter");
    if (FAILED(hr))
        return false;
    hr = mGraph->AddFilter(mPreviewNull, L"Preview Null");
    if (FAILED(hr))
        return false;
    hr = mGraph->AddFilter(mStillNull, L"Still Null");
    if (FAILED(hr))
        return false;

    // 设置格式
    AM_MEDIA_TYPE amt = {0};
    amt.majortype = MEDIATYPE_Video;
    amt.subtype = mSetting.isJpg ? MEDIASUBTYPE_MJPG : MEDIASUBTYPE_RGB32;
    amt.formattype = FORMAT_VideoInfo;
    hr = mPreviewGrabber->SetMediaType(&amt);
    if (FAILED(hr)) {
        return false;
    }
    // 回调函数 0-调用SampleCB 1-BufferCB
    mPreviewGrabber->SetCallback(&mPreviewCallback, 1);
    if (FAILED(hr))
        return false;

    hr = mStillGrabber->SetMediaType(&amt);
    if (FAILED(hr))
        return false;
    mStillGrabber->SetOneShot(false);
    mStillGrabber->SetBufferSamples(true);
    // 回调函数 0-调用SampleCB 1-BufferCB
    mStillGrabber->SetCallback(&mStillCallback, 1);
    if (FAILED(hr))
        return false;

    // 设置分辨率
    if (mSetting.valid) {
        setFormat(mSetting.width, mSetting.height, mSetting.avgTime);
        mSetting.valid = false;
    }
    // RenderStream最后一个参数为空会弹出activemovie窗口显示预览视频
    // 此处设置PIN_CATEGORY_PREVIEW只能预览，没法在格式设置界面设置
    hr = mBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                mSourceFilter, mPreviewFilter, mPreviewNull);
    if (FAILED(hr))
        return false;

    hr = mBuilder->RenderStream(&PIN_CATEGORY_STILL, &MEDIATYPE_Video,
                                mSourceFilter, mStillFilter, mStillNull);
    if (FAILED(hr))
        return false;

    return play();
}

bool CameraCore::setFormat(int width, int height, int avgTime)
{
    if (!mSourceFilter || !mBuilder) {
        return false;
    }

    HRESULT hr = S_FALSE;
    IAMStreamConfig *stream_config = NULL;
    hr = mBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, mSourceFilter,
                                 IID_IAMStreamConfig, reinterpret_cast<void **>(&stream_config));
    if (FAILED(hr) || !stream_config) {
        hr = mBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, mSourceFilter,
                                     IID_IAMStreamConfig, reinterpret_cast<void **>(&stream_config));
    }
    if (FAILED(hr) || !stream_config) {
        return false;
    }

    AM_MEDIA_TYPE *amt = NULL;
    hr = stream_config->GetFormat(&amt);
    bool ret = false;
    // 设置分辨率，如果无效的话似乎会使用默认值
    if (SUCCEEDED(hr) && amt && amt->pbFormat) {
        VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(amt->pbFormat);
        vih->bmiHeader.biWidth = width;
        vih->bmiHeader.biHeight = height;
        // int fps = qRound(10000000.0 / pvi->AvgTimePerFrame);
        vih->AvgTimePerFrame = avgTime;
        hr = stream_config->SetFormat(amt);
        if (SUCCEEDED(hr)) {
            ret = true;
        }
        qDebug()<<__FUNCTION__<<hr<<width<<height<<avgTime;
        DeleteMediaType(amt);
    }
    SAFE_RELEASE(stream_config);
    return ret;
}

bool CameraCore::play()
{
    if (mGraph && mMediaControl && mPreviewGrabber) {
        if (SUCCEEDED(mMediaControl->Run())) {
            AM_MEDIA_TYPE amt = {0};
            mPreviewGrabber->GetConnectedMediaType(&amt);
            VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(amt.pbFormat);
            if (vih != NULL)
            {
                int width = vih->bmiHeader.biWidth;
                int height = vih->bmiHeader.biHeight;
                int avg_time = vih->AvgTimePerFrame;
                qDebug()<<__FUNCTION__<<width<<height<<(mSetting.isJpg ? "jpg" : "rgb")<<avg_time;

                mPreviewCallback.width = width;
                mPreviewCallback.height = height;
                // 如果是非固定的格式，可以在这里设置回调函数的解析方式
                mPreviewCallback.isJpg = mSetting.isJpg;
                mPreviewCallback.running = true;

                mStillCallback.width = width;
                mStillCallback.height = height;
                mStillCallback.isJpg = mSetting.isJpg;
                mStillCallback.running = true;

                probe.reset();
                setState(Playing);

                mSetting.width = width;
                mSetting.height = height;
                mSetting.avgTime = avg_time;
                emit formatChanged();
                return true;
            }
        }
    }
    return false;
}

bool CameraCore::pause()
{
    if (mGraph && mMediaControl) {
        if (SUCCEEDED(mMediaControl->Pause())) {
            setState(Paused);
            return true;
        }
    }
    return false;
}

bool CameraCore::stop()
{
    if (mGraph && mMediaControl) {
        if (SUCCEEDED(mMediaControl->Stop())) {
            setState(Stopped);
            return true;
        }
    }
    return false;
}

void CameraCore::popDeviceSetting(QQuickWindow *window)
{
    if (!window) {
        return;
    }
    winHandle = (HWND)window->winId();
    QMetaObject::invokeMethod(this, "deviceSetting", Qt::QueuedConnection);
}

void CameraCore::popFormatSetting(QQuickWindow *window)
{
    if (!window) {
        return;
    }
    winHandle = (HWND)window->winId();
    QMetaObject::invokeMethod(this, "formatSetting", Qt::QueuedConnection);
}

void CameraCore::releaseGraph()
{
    mPreviewCallback.running = false;
    mStillCallback.running = false;
    probe.reset();
    if (mMediaControl) {
        mMediaControl->Stop();
    }
    SAFE_RELEASE(mMediaControl);
    if (mGraph) {
        if (mSourceFilter) {
            freePin(mGraph, mSourceFilter);
            mGraph->RemoveFilter(mSourceFilter);
        }
        if (mPreviewFilter) {
            freePin(mGraph, mPreviewFilter);
            mGraph->RemoveFilter(mPreviewFilter);
        }
        if (mPreviewNull) {
            freePin(mGraph, mPreviewNull);
            mGraph->RemoveFilter(mPreviewNull);
        }
        if (mStillFilter) {
            freePin(mGraph, mStillFilter);
            mGraph->RemoveFilter(mStillFilter);
        }
        if (mStillNull) {
            freePin(mGraph, mStillNull);
            mGraph->RemoveFilter(mStillNull);
        }
    }
    SAFE_RELEASE(mSourceFilter);
    SAFE_RELEASE(mStillNull);
    SAFE_RELEASE(mPreviewNull);
    SAFE_RELEASE(mStillFilter);
    SAFE_RELEASE(mPreviewFilter);
    SAFE_RELEASE(mStillGrabber);
    SAFE_RELEASE(mPreviewGrabber);
    SAFE_RELEASE(mBuilder);
    SAFE_RELEASE(mGraph);
    setState(Stopped);
}

bool CameraCore::bindFilter(const QString &deviceName)
{
    HRESULT hr;

    // 调用 CoCreateInstance 以创建系统设备枚举器的实例
    ICreateDevEnum *devce_enum = NULL;
    hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                            IID_ICreateDevEnum, reinterpret_cast<void **>(&devce_enum));
    if (FAILED(hr)) {
        return false;
    }

    // 2.调用 ICreateDevEnum::CreateClassEnumerator，并将设备类别指定为 GUID
    IEnumMoniker *enum_moniker = NULL;
    hr = devce_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enum_moniker, 0);
    if (FAILED(hr)) {
        SAFE_RELEASE(devce_enum);
        return false;
    }
    enum_moniker->Reset();

    // CreateClassEnumerator 方法返回指向 IEnumMoniker 接口的指针
    // 若要枚举名字对象，请调用 IEnumMoniker::Next。
    IMoniker *moniker = NULL;
    IMalloc *malloc_interface = NULL;
    ::CoGetMalloc(1, reinterpret_cast<LPMALLOC *>(&malloc_interface));
    while (enum_moniker->Next(1, &moniker, NULL) == S_OK)
    {
        BSTR name_str = NULL;
        hr = moniker->GetDisplayName(NULL, NULL, &name_str);
        if (FAILED(hr)) {
            SAFE_RELEASE(moniker);
            continue;
        }
        // "@device:pnp:\\\\?\\usb#vid_04ca&pid_7070&mi_00#6&16c57194&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global"
        QString display_name = QString::fromWCharArray(name_str);
        malloc_interface->Free(name_str);
        // qDebug()<<"displayname"<<displayName;
        // 匹配到选择的设备，参考Qt5.15DSCameraSession源码用的contains，我也不知道为啥不比较相等
        if (deviceName.contains(display_name)) {
            // BindToObject将某个设备标识绑定到一个DirectShow Filter，
            // 然后调用IFilterGraph::AddFilter加入到Filter Graph中，这个设备就可以参与工作了
            // 调用IMoniker::BindToObject建立一个和选择的device联合的filter，
            // 并且装载filter的属性(CLSID,FriendlyName, and DevicePath)。
            hr = moniker->BindToObject(NULL, NULL, IID_IBaseFilter,
                                       reinterpret_cast<void **>(&mSourceFilter));
            if (SUCCEEDED(hr)) {
                SAFE_RELEASE(moniker);
                break;
            }
        }
        SAFE_RELEASE(moniker);
    }

    SAFE_RELEASE(malloc_interface);
    SAFE_RELEASE(moniker);
    SAFE_RELEASE(enum_moniker);
    SAFE_RELEASE(devce_enum);
    return !!mSourceFilter;
}

void CameraCore::freePin(IGraphBuilder *inGraph, IBaseFilter *inFilter) const
{
    if (!inGraph || !inFilter)
        return;

    IEnumPins *pin_enum = NULL;

    // 创建一个Pin的枚举器
    if (SUCCEEDED(inFilter->EnumPins(&pin_enum)))
    {
        pin_enum->Reset();

        IPin *pin = NULL;
        ULONG fetched = 0;
        // 枚举该Filter上所有的Pin
        while (SUCCEEDED(pin_enum->Next(1, &pin, &fetched)) && fetched)
        {
            if (pin)
            {
                // 得到当前Pin连接对象的Pin指针
                IPin *connected_pin = NULL;
                pin->ConnectedTo(&connected_pin);
                if (connected_pin)
                {
                    // 查询Pin信息（获取Pin的方向)
                    PIN_INFO pin_info;
                    if (SUCCEEDED(connected_pin->QueryPinInfo(&pin_info)))
                    {
                        pin_info.pFilter->Release();
                        if (pin_info.dir == PINDIR_INPUT)
                        {
                            // 如果连接对方是输入Pin(说明当前枚举得到的是输出Pin)
                            // 则递归调用NukeDownstream函数，首先将下一级（乃至再下一级）
                            // 的所有Filter删除
                            freePin(inGraph, pin_info.pFilter);
                            inGraph->Disconnect(connected_pin);
                            inGraph->Disconnect(pin);
                            inGraph->RemoveFilter(pin_info.pFilter);
                        }
                    }
                    connected_pin->Release();
                }
                pin->Release();
            }
        }
        pin_enum->Release();
    }
}

void CameraCore::deviceSetting()
{
    if (getState() == Stopped || !mSourceFilter || !winHandle)
        return;
    ISpecifyPropertyPages *prop_pages = NULL;
    if (S_OK == mSourceFilter->QueryInterface(IID_ISpecifyPropertyPages, reinterpret_cast<void **>(&prop_pages)))
    {
        CAUUID cauuid;
        prop_pages->GetPages(&cauuid);
        ::OleCreatePropertyFrame(winHandle, 30, 30, NULL, 1,
                                 reinterpret_cast<IUnknown **>(&mSourceFilter),
                                 cauuid.cElems,
                                 reinterpret_cast<GUID *>(cauuid.pElems),
                                 0, 0, NULL);
        ::CoTaskMemFree(cauuid.pElems);
        prop_pages->Release();
    }
}

void CameraCore::formatSetting()
{
    if (getState() == Stopped || !mGraph || !mSourceFilter || !winHandle)
        return;

    HRESULT hr = S_FALSE;
    IAMStreamConfig *stream_config = NULL;
    hr = mBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, mSourceFilter,
                                 IID_IAMStreamConfig, reinterpret_cast<void **>(&stream_config));
    if (FAILED(hr) || !stream_config) {
        hr = mBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, mSourceFilter,
                                     IID_IAMStreamConfig, reinterpret_cast<void **>(&stream_config));
    }
    if (FAILED(hr) || !stream_config) {
        return;
    }

    ISpecifyPropertyPages *prop_pages = NULL;
    CAUUID cauuid;
    if (S_OK == stream_config->QueryInterface(IID_ISpecifyPropertyPages, reinterpret_cast<void**>(&prop_pages)))
    {
        stop();
        // 这里不断开filter有的设备没法设置
        freePin(mGraph, mSourceFilter);

        prop_pages->GetPages(&cauuid);
        ::OleCreatePropertyFrame(winHandle, 30, 30, NULL, 1,
                                 reinterpret_cast<IUnknown **>(&stream_config),
                                 cauuid.cElems,
                                 reinterpret_cast<GUID *>(cauuid.pElems),
                                 0, 0, NULL);
        ::CoTaskMemFree(cauuid.pElems);
        prop_pages->Release();

        AM_MEDIA_TYPE *pmt = NULL;
        if (NOERROR == stream_config->GetFormat(&pmt))
        {
            if (pmt->formattype == FORMAT_VideoInfo)
            {
                VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(pmt->pbFormat);
                int width = vih->bmiHeader.biWidth;
                int height = vih->bmiHeader.biHeight;
                int avg_time = vih->AvgTimePerFrame;
                mSetting.width = width;
                mSetting.height = height;
                mSetting.avgTime = avg_time;
                mSetting.valid = true;
                qDebug()<<__FUNCTION__<<width<<height<<avg_time;
            }
        }

        DeleteMediaType(pmt);
        selectDevice(getDeviceIndex());
    }
    stream_config->Release();
}
