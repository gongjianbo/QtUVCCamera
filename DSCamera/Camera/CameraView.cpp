#include "CameraView.h"
#include <QCoreApplication>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHoverEvent>
#include <QtMath>
#include <QDateTime>
#include <QDir>
#include <QDebug>

CameraView::CameraView(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);

    connect(&fpsTimer, &QTimer::timeout, this, [this](){
        setFps(fpsCount);
        fpsCount = 0;
    });
    fpsTimer.start(1000);
}

bool CameraView::getIsEmpty() const
{
    return viewImage.isNull();
}

int CameraView::getFps() const
{
    return fps;
}

void CameraView::setFps(int value)
{
    if (fps != value) {
        fps = value;
        emit fpsChanged();
    }
}

void CameraView::setHorFlipIschecked(bool check)
{
    horFlipIschecked = check;
    update();
}

void CameraView::setVerFlipIschecked(bool check)
{
    verFlipIschecked = check;
    update();
}

void CameraView::updateImage(const QImage &img)
{
    if (viewImage.isNull() != img.isNull()) {
        emit isEmptyChanged();
    }
    viewImage = img;
    fpsCount++;
    update();
}

void CameraView::save()
{

}

QSGNode *CameraView::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (getIsEmpty())
        return oldNode;

    QRect node_rect = boundingRect().toRect();
    // 计算居中的位置
    const double image_ratio = viewImage.width() / (double)viewImage.height();
    const double rect_ratio = node_rect.width() / (double)node_rect.height();
    if (image_ratio > rect_ratio) {
        const int new_height = node_rect.width() / image_ratio;
        node_rect.setY(node_rect.y() + (node_rect.height() - new_height) / 2);
        node_rect.setHeight(new_height);
    } else {
        const int new_width = image_ratio * node_rect.height();
        node_rect.setX(node_rect.x() + (node_rect.width() - new_width) / 2);
        node_rect.setWidth(new_width);
    }

    if (!node_rect.isValid())
        return oldNode;
    // 图片缩放比例
    const double scale = node_rect.width() / (double)viewImage.width() * 100;

    QSGSimpleTextureNode *node = dynamic_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
    }
    QImage img = viewImage;
    // img = viewImage.mirrored(horFlipIschecked, verFlipIschecked);
    // 缩小时平滑缩放
    if (scale < 100)
    {
        img = img.scaled(node_rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QSGTexture *m_texture = window()->createTextureFromImage(img);
    node->setTexture(m_texture);
    node->setOwnsTexture(true);
    node->setRect(node_rect);
    node->markDirty(QSGNode::DirtyGeometry);
    node->markDirty(QSGNode::DirtyMaterial);
    QSGSimpleTextureNode::TextureCoordinatesTransformMode trans_flag;
    if (horFlipIschecked) {
        trans_flag |= QSGSimpleTextureNode::MirrorHorizontally;
    }
    if (verFlipIschecked) {
        trans_flag |= QSGSimpleTextureNode::MirrorVertically;
    }
    node->setTextureCoordinatesTransform(trans_flag);

    return node;
}
