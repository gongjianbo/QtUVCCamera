#pragma once
#include <QObject>
#include "CameraCommon.h"

// 设备信息
struct CameraDevice
{
    // 设备信息字符串，包含devicePath等信息
    QString displayName;
    // 用于ui显示的设备名
    QString friendlyName;
};

// 设备信息
class CameraInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList deviceNames READ getDeviceNames NOTIFY deviceListChanged)
    Q_PROPERTY(QStringList friendlyNames READ getFriendlyNames NOTIFY deviceListChanged)
public:
    explicit CameraInfo(QObject *parent = nullptr);

    // 设备名列表-displayName
    QStringList getDeviceNames() const;
    // 设备名对应的显示名称-friendlyName
    QStringList getFriendlyNames() const;

    // 更新设备列表
    void updateDeviceList();
    // 设备列表
    QList<CameraDevice> getDeviceList() const;
    // 获取设备名中的 vid pid
    static bool getVidPid(const QString &name, quint16 &vid, quint16 &pid);

signals:
    void deviceListChanged();

private:
    // 枚举设备信息
    QList<CameraDevice> enumDeviceList() const;

private:
    QList<CameraDevice> deviceList;
};
