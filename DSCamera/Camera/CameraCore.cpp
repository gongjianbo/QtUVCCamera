#include "CameraCore.h"
#include <QDateTime>
#include <QDebug>

CameraCore::CameraCore()
{
}

CameraCore::~CameraCore()
{
    stop();
    releaseGraph();
}

int CameraCore::getCurWidth() const
{
    return mSetting.width;
}

int CameraCore::getCurHeight() const
{
    return mSetting.height;
}

void CameraCore::setCallback(const std::function<void (const QImage &)> &previewCB,
                             const std::function<void (const QImage &)> &stillCB)
{
    mPreviewCallback.setCallback(previewCB);
    mStillCallback.setCallback(stillCB);
}

bool CameraCore::openDevice(const CameraDevice &device)
{
    // 释放当前
    releaseGraph();
    mDevice = device;

    HRESULT hr = S_FALSE;
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

    // 创建用于预览的Sample Grabber Filter.
    hr = ::CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void **>(&mPreviewFilter));
    if (FAILED(hr))
        return false;

    // 获取ISampleGrabber接口，用于设置回调等相关信息
    hr = mPreviewFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void **>(&mPreviewGrabber));
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

    GUID sub_type = MEDIASUBTYPE_NULL;
    if (mSetting.valid) {
        sub_type = mSetting.type;
    } else {
        getType(sub_type);
    }
    if (!isValidSubtype(sub_type)) {
        sub_type = MEDIASUBTYPE_RGB32;
    }
    // 先设置一下后面才能生效
    AM_MEDIA_TYPE amt = {0};
    amt.majortype = MEDIATYPE_Video;
    amt.subtype = sub_type;
    amt.formattype = FORMAT_VideoInfo;
    hr = mPreviewGrabber->SetMediaType(&amt);
    if (FAILED(hr))
        return false;

    // 设置分辨率
    if (mSetting.valid) {
        setFormat(mSetting.width, mSetting.height, mSetting.avgTime, mSetting.type);
        mSetting.valid = false;
    } else {
        // 初始化格式
        setFormat(0, 0, 0, sub_type);
    }

    if (mState.recording) {
        IBaseFilter *mux = NULL;
        // 设置输出视频文件位置
        wchar_t path[MAX_PATH] = {0};
        mState.recordPath.toWCharArray(path);
        hr = mBuilder->SetOutputFileName(&MEDIASUBTYPE_Avi, path, &mux, NULL);
        if (FAILED(hr))
            return false;
        // RenderStream最后一个参数为空会弹出activemovie窗口显示预览视频
        hr = mBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                    mSourceFilter, mPreviewFilter, mux);
        if (FAILED(hr))
            return false;
        // 参考别人的代码，用完直接release
        mux->Release();
    } else {
        // RenderStream最后一个参数为空会弹出activemovie窗口显示预览视频
        hr = mBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                    mSourceFilter, NULL, mPreviewFilter);
        if (FAILED(hr))
            return false;
    }

    mPreviewGrabber->SetOneShot(false);
    mPreviewGrabber->SetBufferSamples(false);
    // 回调函数 0-调用SampleCB 1-BufferCB
    hr = mPreviewGrabber->SetCallback(&mPreviewCallback, 1);
    if (FAILED(hr))
        return false;
    // 设置格式
    // 其他的原生格式解析可以参考：
    // https://github.com/GoodRon/QtWebcam
    // 衍生版本：https://gitee.com/fsfzp888/UVCCapture
    // amt.subtype = MEDIASUBTYPE_MJPG;
    hr = mPreviewGrabber->SetMediaType(&amt);
    if (FAILED(hr))
        return false;

    mStillGrabber->SetOneShot(false);
    mStillGrabber->SetBufferSamples(true);
    // 回调函数 0-调用SampleCB 1-BufferCB
    hr = mStillGrabber->SetCallback(&mStillCallback, 1);
    if (FAILED(hr))
        return false;
    hr = mStillGrabber->SetMediaType(&amt);
    if (FAILED(hr))
        return false;

    // still render可能失败，但是不return false
    hr = mBuilder->RenderStream(&PIN_CATEGORY_STILL, &MEDIATYPE_Video,
                                mSourceFilter, NULL, mStillFilter);
    if (FAILED(hr)) {
        // 失败后，stillpin可能持续触发，比如yuy2转rgb时
        // 所以这里取消掉still的回调
        mStillGrabber->SetCallback(NULL, 1);
    }

    return true;
}

bool CameraCore::getType(GUID &type)
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

    AM_MEDIA_TYPE *pamt = NULL;
    hr = stream_config->GetFormat(&pamt);
    bool ret = false;
    // 设置分辨率，如果无效的话似乎会使用默认值
    if (SUCCEEDED(hr) && pamt && pamt->pbFormat) {
        type = pamt->subtype;
        ret = true;
    }
    DeleteMediaType(pamt);
    SAFE_RELEASE(stream_config);
    return ret;
}

bool CameraCore::setFormat(int width, int height, LONGLONG avgTime, GUID type)
{
    qDebug()<<__FUNCTION__<<"call"<<width<<height<<avgTime<<type;
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
    int cap_count;
    int cap_size;
    VIDEO_STREAM_CONFIG_CAPS caps;
    hr = stream_config->GetNumberOfCapabilities(&cap_count, &cap_size);
    if (FAILED(hr) || sizeof(caps) != cap_size)
    {
        SAFE_RELEASE(stream_config);
        return false;
    }
    bool ret = false;
    for (int i = 0; i < cap_count && !ret; i++)
    {
        AM_MEDIA_TYPE *pamt = NULL;
        hr = stream_config->GetStreamCaps(i, &pamt, (BYTE*)(&caps));
        if (FAILED(hr) || !pamt) {
            continue;
        }
        // 没设置类型时就用第一个类型
        if (type == MEDIASUBTYPE_NULL) {
            type = pamt->subtype;
        }
        if (pamt->formattype == FORMAT_VideoInfo && pamt->subtype == type) {
            VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(pamt->pbFormat);
            if (width > 0 && height > 0) {
                vih->bmiHeader.biWidth = width;
                vih->bmiHeader.biHeight = height;
            } else {
                width = vih->bmiHeader.biWidth;
                height = vih->bmiHeader.biHeight;
            }
            if (avgTime > 0) {
                // int fps = qRound(10000000.0 / pvi->AvgTimePerFrame);
                vih->AvgTimePerFrame = avgTime;
            } else {
                avgTime = vih->AvgTimePerFrame;
            }
            hr = stream_config->SetFormat(pamt);
            if (SUCCEEDED(hr)) {
                ret = true;
                qDebug()<<__FUNCTION__<<"result"<<hr<<width<<height<<avgTime<<type;
            }
        }
        DeleteMediaType(pamt);
        if (ret)
            break;
    }
    // 上面设置失败了，可能是没有jpg或者rgb，此处设置默认格式的分辨率
    if (!ret) {
        AM_MEDIA_TYPE *pamt = NULL;
        hr = stream_config->GetFormat(&pamt);
        // 设置分辨率，如果无效会使用默认值
        if (SUCCEEDED(hr) && pamt && pamt->pbFormat) {
            VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(pamt->pbFormat);
            if (width > 0 && height > 0) {
                vih->bmiHeader.biWidth = width;
                vih->bmiHeader.biHeight = height;
            } else {
                width = vih->bmiHeader.biWidth;
                height = vih->bmiHeader.biHeight;
            }
            if (avgTime > 0) {
                // int fps = qRound(10000000.0 / pvi->AvgTimePerFrame);
                vih->AvgTimePerFrame = avgTime;
            } else {
                avgTime = vih->AvgTimePerFrame;
            }
            // 这里可以判断原格式，不能设置
            hr = stream_config->SetFormat(pamt);
            if (SUCCEEDED(hr)) {
                ret = true;
            }
            qDebug()<<__FUNCTION__<<"other"<<hr<<width<<height<<avgTime<<pamt->subtype;
        }
        DeleteMediaType(pamt);
    }
    SAFE_RELEASE(stream_config);
    if (ret) {
        mSetting.width = width;
        mSetting.height = height; 
    }
    return ret;
}

bool CameraCore::play()
{
    if (!mGraph || !mMediaControl || !mPreviewGrabber)
        return false;

    if (FAILED(mMediaControl->Run()))
        return false;

    AM_MEDIA_TYPE amt = {0};
    HRESULT hr = mPreviewGrabber->GetConnectedMediaType(&amt);
    if (FAILED(hr))
        return false;
    VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(amt.pbFormat);
    if (!vih)
        return false;

    int width = vih->bmiHeader.biWidth;
    int height = vih->bmiHeader.biHeight;
    LONGLONG avg_time = vih->AvgTimePerFrame;
    GUID sub_type = amt.subtype;

    mPreviewCallback.setSize(width, height);
    mPreviewCallback.setSubtype(sub_type);
    mPreviewCallback.start();
    qDebug()<<__FUNCTION__<<"preview"<<width<<height<<avg_time<<sub_type;

    mStillCallback.setSize(width, height);
    mStillCallback.setSubtype(sub_type);
    if (mStillGrabber && SUCCEEDED(mStillGrabber->GetConnectedMediaType(&amt))) {
        // 可能 StillPin 的格式没有被设置成功
        VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(amt.pbFormat);
        if (vih) {
            mStillCallback.setSize(vih->bmiHeader.biWidth, vih->bmiHeader.biHeight);
            mStillCallback.setSubtype(amt.subtype);
            qDebug()<<__FUNCTION__<<"still"<<vih->bmiHeader.biWidth<<vih->bmiHeader.biHeight<<amt.subtype;
        }
    }
    mStillCallback.start();

    mSetting.width = width;
    mSetting.height = height;
    mSetting.avgTime = avg_time;
    mSetting.type = sub_type;

    mState.running = true;
    return true;
}

bool CameraCore::pause()
{
    if (mGraph && mMediaControl) {
        return SUCCEEDED(mMediaControl->Pause());
    }
    return false;
}

bool CameraCore::stop()
{
    if (mGraph && mMediaControl) {
        mPreviewCallback.stop();
        mStillCallback.stop();
        mState.running = false;
        return SUCCEEDED(mMediaControl->Stop());
    }
    return false;
}

bool CameraCore::startRecord(const QString &savePath)
{
    if (!mState.running) {
        return false;
    }
    // 暂时在core中调用
    // TODO 目前录制会卡顿，即便不解析图片数据
    stop();
    mState.recording = true;
    mState.recordPath = savePath;
    // 打开时保持原来的size
    mSetting.valid = true;
    openDevice(mDevice);
    play();
    return true;
}

bool CameraCore::stopRecord()
{
    if (!mState.running) {
        return false;
    }
    mState.recording = false;
    mSetting.valid = true;
    openDevice(mDevice);
    play();
    return true;
}

void CameraCore::releaseGraph()
{
    mPreviewCallback.stop();
    mStillCallback.stop();
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
        if (mStillFilter) {
            freePin(mGraph, mStillFilter);
            mGraph->RemoveFilter(mStillFilter);
        }
    }
    SAFE_RELEASE(mSourceFilter);
    SAFE_RELEASE(mStillFilter);
    SAFE_RELEASE(mPreviewFilter);
    SAFE_RELEASE(mStillGrabber);
    SAFE_RELEASE(mPreviewGrabber);
    SAFE_RELEASE(mBuilder);
    SAFE_RELEASE(mGraph);
}

bool CameraCore::bindFilter(const QString &deviceName)
{
    HRESULT hr = S_FALSE;

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

bool CameraCore::deviceSetting(HWND winId)
{
    if (!mSourceFilter)
        return false;

    ISpecifyPropertyPages *prop_pages = NULL;
    if (S_OK == mSourceFilter->QueryInterface(IID_ISpecifyPropertyPages, reinterpret_cast<void **>(&prop_pages)))
    {
        CAUUID cauuid;
        prop_pages->GetPages(&cauuid);
        ::OleCreatePropertyFrame(winId, 30, 30, NULL, 1,
                                 reinterpret_cast<IUnknown **>(&mSourceFilter),
                                 cauuid.cElems,
                                 reinterpret_cast<GUID *>(cauuid.pElems),
                                 0, 0, NULL);
        ::CoTaskMemFree(cauuid.pElems);
        prop_pages->Release();
        return true;
    }
    return false;
}

bool CameraCore::formatSetting(HWND winId)
{
    if (!mGraph || !mSourceFilter)
        return false;;

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

    ISpecifyPropertyPages *prop_pages = NULL;
    CAUUID cauuid;
    bool ret = false;
    if (S_OK == stream_config->QueryInterface(IID_ISpecifyPropertyPages, reinterpret_cast<void**>(&prop_pages)))
    {
        // 先停止，这一步放到了调用CameraCore::formatSetting前，重新打开放到了调用后
        // 这里不断开filter有的设备没法设置
        freePin(mGraph, mSourceFilter);

        prop_pages->GetPages(&cauuid);
        ::OleCreatePropertyFrame(winId, 30, 30, NULL, 1,
                                 reinterpret_cast<IUnknown **>(&stream_config),
                                 cauuid.cElems,
                                 reinterpret_cast<GUID *>(cauuid.pElems),
                                 0, 0, NULL);
        ::CoTaskMemFree(cauuid.pElems);
        prop_pages->Release();

        AM_MEDIA_TYPE *pamt = NULL;
        if (NOERROR == stream_config->GetFormat(&pamt))
        {
            if (pamt->formattype == FORMAT_VideoInfo && pamt->majortype == MEDIATYPE_Video)
            {
                VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(pamt->pbFormat);
                int width = vih->bmiHeader.biWidth;
                int height = vih->bmiHeader.biHeight;
                LONGLONG avg_time = vih->AvgTimePerFrame;
                GUID sub_type = pamt->subtype;
                // 稍后会调用 openDevice 和 setFormat 函数
                if (!isValidSubtype(sub_type)) {
                    sub_type = MEDIASUBTYPE_RGB32;
                }

                mSetting.width = width;
                mSetting.height = height;
                mSetting.avgTime = avg_time;
                mSetting.type = sub_type;
                mSetting.valid = true;
                qDebug()<<__FUNCTION__<<width<<height<<avg_time<<sub_type;
                ret = true;
            }
        }
        DeleteMediaType(pamt);
    }
    stream_config->Release();
    return ret;
}
