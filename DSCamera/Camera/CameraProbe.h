#pragma once
#include <QObject>
#include "CameraCommon.h"

// 数据探针
class CameraProbe : public QObject
{
    Q_OBJECT
public:
    explicit CameraProbe(QObject *parent = nullptr);

    // 拍图
    Q_INVOKABLE void capture();
    // 打开保存图片文件夹
    Q_INVOKABLE void openCacheDir();

    // 重置状态
    void reset();
    // 在preview数据线程回调
    void previewUpdate(const QImage &img);
    // 在still数据线程回调
    void stillUpdate(const QImage &img);

signals:
    // 捕获到图片
    void captureFinished(const QImage &img);

private:
    // 下一帧保存
    std::atomic_bool saveNext{false};
};
