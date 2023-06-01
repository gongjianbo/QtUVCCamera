#pragma once
#include <QObject>
#include <QObject>
#include <QUuid>
#include <QSharedPointer>
#include "CameraInfo.h"
class CameraHotplugPrivate;

// 设备热插拔检测
class CameraHotplug : public QObject
{
    Q_OBJECT
public:
    explicit CameraHotplug(QObject *parent = nullptr);
    ~CameraHotplug();

    // RegisterDeviceNotification 注册对应的 GUID 消息通知
    // 暂未考虑重复注册和注册失败的处理
    void init(const QVector<QUuid> &uuids);

    // UnregisterDeviceNotification
    // 会在析构中自动调用一次
    void free();

signals:
    // 设备插入
    void deviceAttached(quint16 vid, quint16 pid);
    // 设备拔出
    void deviceDetached(quint16 vid, quint16 pid);

private:
    QSharedPointer<CameraHotplugPrivate> dptr;
};
