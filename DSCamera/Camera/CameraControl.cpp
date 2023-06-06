#include "CameraControl.h"
#include <QDateTime>
#include <QDebug>

CameraControl::CameraControl(QObject *parent)
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
    core.setCallback(preview_callback, still_callback);
    probe.attachCore(&core);

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
            if (device_list.isEmpty())
                return;
            int select = 0;
            for (int i = 0; i < device_list.size(); i++)
            {
                if (device.displayName.compare(device_list.at(i).displayName) == 0) {
                    select = i;
                    break;
                }
            }
            // TODO 检测之前是否拔出，不然每插一个都重新加载
            selectDevice(select);
        }, Qt::QueuedConnection);
    // 移除设备暂不处理
    // connect(&hotplug, &CameraHotplug::deviceDetached, this, [this](){}, Qt::QueuedConnection);

    // 初始化设备信息，默认选中一个设备
    info.updateDeviceList();
    if (info.getDeviceList().size() > 0) {
        selectDevice(0);
    }
}

CameraControl::~CameraControl()
{

}

CameraInfo *CameraControl::getInfo()
{
    return &info;
}

CameraProbe *CameraControl::getProbe()
{
    return &probe;
}

CameraHotplug *CameraControl::getHotplug()
{
    return &hotplug;
}

int CameraControl::getState() const
{
    return state;
}

void CameraControl::setState(int newState)
{
    if (state != newState) {
        state = (CameraControl::CameraState)newState;
        emit stateChanged(state);
    }
}

int CameraControl::getDeviceIndex() const
{
    return deviceIndex;
}

void CameraControl::setDeviceIndex(int index)
{
    if (deviceIndex != index) {
        deviceIndex = index;
        emit deviceIndexChanged();
    }
}

QSize CameraControl::getResolution() const
{
    return QSize(core.getCurWidth(), core.getCurHeight());
}

void CameraControl::attachView(CameraView *view)
{
    if (!view) {
        return;
    }
    connect(this, &CameraControl::imageComing, view, &CameraView::updateImage, Qt::QueuedConnection);
}

bool CameraControl::selectDevice(int index)
{
    auto device_list = info.getDeviceList();
    if (index < 0 || index >= device_list.size())
        return false;
    CameraDevice device = device_list.at(index);
    setDeviceIndex(index);
    probe.reset();
    if (!core.openDevice(device)) {
        setState(Stopped);
        return false;
    }
    return play();
}

bool CameraControl::setFormat(int width, int height)
{
    return core.setFormat(width, height);
}

bool CameraControl::play()
{
    if (!core.play()) {
        return false;
    }
    probe.reset();
    setState(Playing);
    emit formatChanged();
    return true;
}

bool CameraControl::pause()
{
    if (!core.pause()) {
        return false;
    }
    setState(Paused);
    return true;
}

bool CameraControl::stop()
{
    if (!core.stop()) {
        return false;
    }
    setState(Stopped);
    return true;
}

void CameraControl::popDeviceSetting(QQuickWindow *window)
{
    if (getState() == Stopped)
        return;
    HWND winId = NULL;
    if (window) {
        winId = (HWND)window->winId();
    }
    QMetaObject::invokeMethod(this, "deviceSetting", Qt::QueuedConnection, Q_ARG(HWND, winId));
}

void CameraControl::popFormatSetting(QQuickWindow *window)
{
    if (getState() == Stopped)
        return;
    HWND winId = NULL;
    if (window) {
        winId = (HWND)window->winId();
    }
    QMetaObject::invokeMethod(this, "formatSetting", Qt::QueuedConnection, Q_ARG(HWND, winId));
}

void CameraControl::deviceSetting(HWND winId)
{
    core.deviceSetting(winId);
}

void CameraControl::formatSetting(HWND winId)
{
    stop();
    core.formatSetting(winId);
    selectDevice(getDeviceIndex());
}
