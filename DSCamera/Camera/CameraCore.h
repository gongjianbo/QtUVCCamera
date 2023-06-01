#pragma once
#include <QObject>
#include <QQuickWindow>
#include "CameraInfo.h"
#include "CameraProbe.h"
#include "CameraView.h"

// Camera 具体操作
class CameraCore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CameraInfo* info READ getInfo CONSTANT)
    Q_PROPERTY(CameraProbe* probe READ getProbe CONSTANT)
    Q_PROPERTY(int state READ getState NOTIFY stateChanged)
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
    explicit CameraCore(QObject *parent = nullptr);
    ~CameraCore();

    // 设备信息
    CameraInfo *getInfo();
    // 拍图和录制
    CameraProbe *getProbe();

    // 相机工作状态
    int getState() const;
    void setState(int newState);

    // 分辨率
    QSize getResolution() const;

    // 关联视频显示
    Q_INVOKABLE void attachView(CameraView *view);
    // 设置回调
    void setCallback(const std::function<void(const QImage &image)> &previewCB,
                     const std::function<void(const QImage &image)> &stillCB);
    // 根据setting的设备列表下标打开某个设备，从0开始
    Q_INVOKABLE bool selectDevice(int index);
    // 设置分辨率、帧率等格式，编码目前固定
    Q_INVOKABLE bool setFormat(int width, int height, int avgTime = 30);
    // 播放
    Q_INVOKABLE bool play();
    // 暂停
    Q_INVOKABLE bool pause();
    // 停止
    Q_INVOKABLE bool stop();
    // 设备设置
    Q_INVOKABLE void popDeviceSetting(QQuickWindow *window);
    // 格式设置
    Q_INVOKABLE void popFormatSetting(QQuickWindow *window);

signals:
    // 新的图像到来
    void imageComing(const QImage &img);
    void stateChanged(int newState);
    void formatChanged();

private:
    void releaseGraph();
    bool bindFilter(const QString &deviceName);
    // 删除与该Filter连接的下游的所有Filter
    void freePin(IGraphBuilder *inGraph, IBaseFilter *inFilter) const;
    // 弹出directshow的设备设置
    Q_INVOKABLE void deviceSetting();
    // 弹出directshow的格式设置
    Q_INVOKABLE void formatSetting();

private:
    // 设备信息
    CameraInfo info;
    // 拍图和录制
    CameraProbe probe;
    // 当前播放状态
    CameraState state{Stopped};
    // 弹出设置界面时绑定当前窗口模态显示
    HWND winHandle{NULL};

    // DirectShow
    ICaptureGraphBuilder2 *mBuilder{NULL};
    IGraphBuilder *mGraph{NULL};
    IMediaControl *mMediaControl{NULL};

    // 视频流
    IBaseFilter *mSourceFilter{NULL};
    ISampleGrabber *mPreviewGrabber{NULL};
    IBaseFilter *mPreviewFilter{NULL};
    IBaseFilter *mPreviewNull{NULL};
    CameraCallback mPreviewCallback;

    // still pin 拍照
    ISampleGrabber *mStillGrabber{NULL};
    IBaseFilter *mStillFilter{NULL};
    IBaseFilter *mStillNull{NULL};
    CameraCallback mStillCallback;

    // selectDevice 时应用之前的设置
    struct {
        // valid=true保存了参数设置，打开设备时进行设置
        bool valid{false};
        // 设备选择序号
        int deviceIndex{0};
        // 尺寸
        int width{0};
        int height{0};
        // 多久一帧，单位100ns纳秒，如果1秒30帧，就是0.0333333秒一帧
        // 换算成100ns单位就是0.0333333 * 1000 * 1000 * 10 = 333333
        int avgTime = 333333;
        // 目前只用到了jpg和rgb32两种格式
        bool isJpg{false};
    } mSetting;
};
