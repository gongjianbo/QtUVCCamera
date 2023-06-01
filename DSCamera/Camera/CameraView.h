#pragma once
#include <QQuickItem>
#include <QImage>
#include <QTimer>
#include <QPainter>

// 视频显示及鼠标交互
class CameraView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool isEmpty READ getIsEmpty NOTIFY isEmptyChanged)
    Q_PROPERTY(int fps READ getFps NOTIFY fpsChanged)
public:
    explicit CameraView(QQuickItem *parent = nullptr);

    // 初始状态没有数据，会显示一个按钮
    bool getIsEmpty() const;

    // 帧率
    int getFps() const;
    void setFps(int value);

    // 横向翻转
    Q_INVOKABLE void setHorFlipIschecked(bool check);
    // 竖向翻转
    Q_INVOKABLE void setVerFlipIschecked(bool check);
    // 更新图像
    Q_INVOKABLE void updateImage(const QImage &img);
    // 保存图像
    Q_INVOKABLE void save();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

signals:
    void isEmptyChanged();
    void fpsChanged();

private:
    // 预览的图片
    QImage viewImage;

    // 横向翻转
    bool horFlipIschecked{false};
    // 竖向翻转
    bool verFlipIschecked{false};

    // 帧率
    int fps{0};
    // 每秒接收帧数
    int fpsCount{0};
    // 定时器统计每秒帧数
    QTimer fpsTimer;
};
