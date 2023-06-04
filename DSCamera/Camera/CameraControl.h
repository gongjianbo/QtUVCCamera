#pragma once
#include <QObject>
#include <QQuickWindow>
#include "CameraCore.h"
#include "CameraInfo.h"
#include "CameraProbe.h"
#include "CameraHotplug.h"
#include "CameraView.h"

// 相机操作
class CameraControl : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CameraInfo* info READ getInfo CONSTANT)
    Q_PROPERTY(CameraProbe* probe READ getProbe CONSTANT)
    Q_PROPERTY(CameraHotplug* hotplug READ getHotplug CONSTANT)
    Q_PROPERTY(int state READ getState NOTIFY stateChanged)
    Q_PROPERTY(int deviceIndex READ getDeviceIndex NOTIFY deviceIndexChanged)
    Q_PROPERTY(QSize resolution READ getResolution NOTIFY formatChanged)
public:
    // 相机工作状态
    enum CameraState
    {
        // 默认停止状态
        Stopped,
        // 正在播放
        Playing,
        // 已暂停
        Paused
    };
    Q_ENUM(CameraState)
public:
    explicit CameraControl(QObject *parent = nullptr);
    ~CameraControl();

    // 设备信息
    CameraInfo *getInfo();
    // 拍图和录制
    CameraProbe *getProbe();
    // 热插拔
    CameraHotplug *getHotplug();

    // 相机工作状态
    int getState() const;
    void setState(int newState);

    // 当前设备选择
    int getDeviceIndex() const;
    void setDeviceIndex(int index);

    // 分辨率
    QSize getResolution() const;

    // 关联视频显示
    Q_INVOKABLE void attachView(CameraView *view);
    // 根据setting的设备列表下标打开某个设备，从0开始
    Q_INVOKABLE bool selectDevice(int index);
    // 设置分辨率等
    Q_INVOKABLE bool setFormat(int width, int height);
    // 播放
    Q_INVOKABLE bool play();
    // 暂停
    Q_INVOKABLE bool pause();
    // 停止
    Q_INVOKABLE bool stop();
    // 设备设置，指定父窗口时模态显示
    Q_INVOKABLE void popDeviceSetting(QQuickWindow *window = nullptr);
    // 格式设置，指定父窗口时模态显示
    Q_INVOKABLE void popFormatSetting(QQuickWindow *window = nullptr);

private:
    // 弹出directshow的设备设置，指定父窗口时模态显示
    Q_INVOKABLE void deviceSetting(HWND winId);
    // 弹出directshow的格式设置，指定父窗口时模态显示
    Q_INVOKABLE void formatSetting(HWND winId);

signals:
    // 新的图像到来
    void imageComing(const QImage &img);
    // 播放/停止状态变化
    void stateChanged(int newState);
    // 设备选择
    void deviceIndexChanged();
    // 格式设置
    void formatChanged();

private:
    // 底层接口操作
    CameraCore core;
    // 设备信息
    CameraInfo info;
    // 拍图和录制
    CameraProbe probe;
    // 热插拔检测
    CameraHotplug hotplug;
    // 当前播放状态
    CameraState state{Stopped};
    // 设备选择序号
    int deviceIndex{-1};
};
