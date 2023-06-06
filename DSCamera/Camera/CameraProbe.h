#pragma once
#include <QObject>
#include "CameraCommon.h"
#include "CameraCore.h"

// 数据探针
class CameraProbe : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ getRecording NOTIFY recordingChanged)
public:
    explicit CameraProbe(QObject *parent = nullptr);

    // 录制状态
    bool getRecording() const;
    void setRecording(bool record);

    // 拍图
    Q_INVOKABLE void capture();
    // 开始录制
    Q_INVOKABLE void startRecord();
    // 结束录制
    Q_INVOKABLE void stopRecord();
    // 打开保存图片文件夹
    Q_INVOKABLE void openCacheDir();

    // 关联core进行操作
    void attachCore(CameraCore *core);
    // 重置状态
    void reset();
    // 在preview数据线程回调
    void previewUpdate(const QImage &img);
    // 在still数据线程回调
    void stillUpdate(const QImage &img);
    // 获取缓存路径
    static QString getCacheDir();

signals:
    // 捕获到图片
    void captureFinished(const QImage &img);
    // 录制状态变化
    void recordingChanged();

private:
    // 底层操作
    CameraCore *corePtr{nullptr};
    // 是否在录制
    bool recording{false};
    // 录制时保存第一张
    std::atomic_bool saveFirst{false};
    QImage recordFirst;
    // 下一帧保存
    std::atomic_bool saveNext{false};
};
