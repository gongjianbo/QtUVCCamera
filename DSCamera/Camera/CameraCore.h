#pragma once
#include <QObject>
#include "CameraCommon.h"

// 封装DirectShow操作
class CameraCore
{
public:
    explicit CameraCore(QObject *parent = nullptr);
    ~CameraCore();

    // 获取当前图像宽度
    int getCurWidth() const;
    // 获取当前图像高度
    int getCurHeight() const;

    // 设置回调
    void setCallback(const std::function<void(const QImage &image)> &previewCB,
                     const std::function<void(const QImage &image)> &stillCB);

    // 打开设备
    bool openDevice(const CameraDevice &device);
    // 查询格式
    bool getType(GUID &type);
    // 设置分辨率、帧率等格式，编码目前固定
    bool setFormat(int width, int height, int avgTime = 30);

    // 播放
    bool play();
    // 暂停
    bool pause();
    // 停止
    bool stop();

    // 弹出directshow的设备设置，指定父窗口时模态显示
    bool deviceSetting(HWND winId);
    // 弹出directshow的格式设置，指定父窗口时模态显示
    bool formatSetting(HWND winId);

private:
    void releaseGraph();
    bool bindFilter(const QString &deviceName);
    // 删除与该Filter连接的下游的所有Filter
    void freePin(IGraphBuilder *inGraph, IBaseFilter *inFilter) const;

private:
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
        // 尺寸
        int width{0};
        int height{0};
        // 多久一帧，单位100ns纳秒，如果1秒30帧，就是0.0333333秒一帧
        // 换算成100ns单位就是0.0333333 * 1000 * 1000 * 10 = 333333
        int avgTime = 333333;
    } mSetting;
};
