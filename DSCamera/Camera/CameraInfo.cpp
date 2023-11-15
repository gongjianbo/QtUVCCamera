#include "CameraInfo.h"
#include <QDebug>
#include "Windows.h"
#include "dshow.h"

CameraInfo::CameraInfo(QObject *parent)
    : QObject{parent}
{

}

QStringList CameraInfo::getDeviceNames() const
{
    QStringList names;
    for (const CameraDevice &device : qAsConst(deviceList))
        names << device.displayName;
    return names;
}

QStringList CameraInfo::getFriendlyNames() const
{
    QStringList names;
    for (const CameraDevice &device : qAsConst(deviceList))
        names << device.friendlyName;
    return names;
}

void CameraInfo::updateDeviceList()
{
    deviceList = enumDeviceList();
    emit deviceListChanged();
}

QList<CameraDevice> CameraInfo::getDeviceList() const
{
    return deviceList;
}

bool CameraInfo::getVidPid(const QString &name, quint16 &vid, quint16 &pid)
{
    // 从设备描述中获取 vid 和 pid
    quint16 vid_temp = 0;
    quint16 pid_temp = 0;
    QString desc_temp = name.toUpper();
    int offset = -1;
#if defined(Q_OS_WIN32)
    offset = desc_temp.indexOf("VID_");
    if (offset > 0 && offset + 8 <= desc_temp.size()) {
        vid_temp = desc_temp.mid(offset + 4, 4).toUShort(nullptr, 16);
    } else {
        return false;
    }
    offset = desc_temp.indexOf("PID_");
    if (offset > 0 && offset + 8 <= desc_temp.size()) {
        pid_temp = desc_temp.mid(offset + 4, 4).toUShort(nullptr, 16);
    } else {
        return false;
    }
#elif defined(Q_OS_MACOS)
    if (desc_temp.size() != 18)
        return false;
    offset = 10;
    vid_temp = desc_temp.mid(offset, 4).toUShort(nullptr, 16);
    offset = 14;
    pid_temp = desc_temp.mid(offset, 4).toUShort(nullptr, 16);
#endif
    vid = vid_temp;
    pid = pid_temp;
    return true;
}

bool enumResolutions(IMoniker *moniker)
{
    // 枚举该设备支持的格式和分辨率
    IBaseFilter *source_filter = NULL;
    HRESULT hr = moniker->BindToObject(NULL, NULL, IID_IBaseFilter,
                                       reinterpret_cast<void **>(&source_filter));
    if (FAILED(hr) || !source_filter)
        return false;

    IEnumPins *enum_pin = NULL;
    if (FAILED(source_filter->EnumPins(&enum_pin)) || !enum_pin) {
        SAFE_RELEASE(source_filter);
        return false;
    }
    enum_pin->Reset();
    IPin *pin = NULL;
    while (SUCCEEDED(enum_pin->Next(1, &pin, NULL)) && pin)
    {
        PIN_INFO pin_info;
        if (FAILED(pin->QueryPinInfo(&pin_info)) || pin_info.dir != PINDIR_OUTPUT) {
            SAFE_RELEASE(pin);
            continue;
        }

        IEnumMediaTypes *enum_type = NULL;
        if (FAILED(pin->EnumMediaTypes(&enum_type)) || !enum_type) {
            SAFE_RELEASE(pin);
            continue;
        }
        enum_type->Reset();
        AM_MEDIA_TYPE *pamt = NULL;
        while (SUCCEEDED(enum_type->Next(1, &pamt, NULL)) && pamt)
        {
            if (pamt->formattype == FORMAT_VideoInfo && pamt->majortype == MEDIATYPE_Video)
            {
                VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(pamt->pbFormat);
                int width = vih->bmiHeader.biWidth;
                int height = vih->bmiHeader.biHeight;
                int avg_time = vih->AvgTimePerFrame;
                // 分辨率和格式是混在一起的，需要自己分开
                qDebug() << pamt->subtype << width << height << avg_time;
            }
            DeleteMediaType(pamt);
            pamt = NULL;
        }
        SAFE_RELEASE(enum_type);
        SAFE_RELEASE(pin);
    }
    SAFE_RELEASE(enum_pin);
    SAFE_RELEASE(source_filter);
    return true;
}

QList<CameraDevice> CameraInfo::enumDeviceList() const
{
    // https://learn.microsoft.com/zh-cn/windows/win32/directshow/selecting-a-capture-device
    QList<CameraDevice> device_list;
    HRESULT hr = S_FALSE;

    // 1.调用 CoCreateInstance 以创建系统设备枚举器的实例。
    ICreateDevEnum *device_enum = NULL;
    hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                            IID_ICreateDevEnum, reinterpret_cast<void **>(&device_enum));
    if (FAILED(hr) || !device_enum) {
        return device_list;
    }

    // 2.调用 ICreateDevEnum::CreateClassEnumerator，并将设备类别指定为 GUID。
    IEnumMoniker *enum_moniker = NULL;
    hr = device_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enum_moniker, 0);
    if (FAILED(hr) || !enum_moniker) {
        SAFE_RELEASE(device_enum);
        return device_list;
    }
    enum_moniker->Reset();

    // 创建一个builder等下用来枚举格式信息
    ICaptureGraphBuilder2 *graph_builder = NULL;
    hr = ::CoCreateInstance(CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC,
                            IID_ICaptureGraphBuilder2, reinterpret_cast<void **>(&graph_builder));
    if (FAILED(hr) || !graph_builder){
        SAFE_RELEASE(device_enum);
        SAFE_RELEASE(enum_moniker);
        return device_list;
    }

    // 3.CreateClassEnumerator 方法返回指向 IEnumMoniker 接口的指针。
    // 若要枚举名字对象，请调用 IEnumMoniker::Next。
    IMoniker *moniker = NULL;
    IMalloc *malloc_interface = NULL;
    ::CoGetMalloc(1, reinterpret_cast<LPMALLOC *>(&malloc_interface));
    qDebug()<<__FUNCTION__;
    int counter = 0;
    while (SUCCEEDED(enum_moniker->Next(1, &moniker, NULL)) && moniker)
    {
        counter++;
        qDebug()<<"device"<<counter;
        CameraDevice device;
        BSTR name_str = NULL;
        hr = moniker->GetDisplayName(NULL, NULL, &name_str);
        if (FAILED(hr)) {
            SAFE_RELEASE(moniker);
            continue;
        }
        // 如"@device:pnp:\\\\?\\usb#vid_04ca&pid_7070&mi_00#6&16c57194&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global"
        device.displayName = QString::fromWCharArray(name_str);
        malloc_interface->Free(name_str);
        qDebug()<<"display name"<<device.displayName;

        IPropertyBag *prop_bag = NULL;
        hr = moniker->BindToStorage(NULL, NULL, IID_IPropertyBag,
                                    reinterpret_cast<void **>(&prop_bag));
        if (FAILED(hr) || !prop_bag) {
            SAFE_RELEASE(moniker);
            continue;
        }

        VARIANT var;
        var.vt = VT_BSTR;
        hr = prop_bag->Read(L"FriendlyName", &var, NULL);
        if (FAILED(hr)) {
            SAFE_RELEASE(prop_bag);
            SAFE_RELEASE(moniker);
            continue;
        }
        // 如"Integrated Camera"
        device.friendlyName = QString::fromWCharArray(var.bstrVal);
        qDebug()<<"friendly name"<<device.friendlyName;

        // displayname字符串里已经包含了devicepath信息
        // hr = prop_bag->Read(L"DevicePath", &var, NULL);
        // if (FAILED(hr)) {
        //     SAFE_RELEASE(prop_bag);
        //     SAFE_RELEASE(moniker);
        //     continue;
        // }
        // QString devicepath = QString::fromWCharArray(var.bstrVal);
        // qDebug()<<"devicepath"<<devicepath;

        // 枚举该设备支持的格式和分辨率
        if (enumResolutions(moniker)) {
            device_list.push_back(device);
        }

        SAFE_RELEASE(prop_bag);
        SAFE_RELEASE(moniker);
    }

    SAFE_RELEASE(malloc_interface);
    SAFE_RELEASE(graph_builder);
    SAFE_RELEASE(moniker);
    SAFE_RELEASE(enum_moniker);
    SAFE_RELEASE(device_enum);
    return device_list;
}

