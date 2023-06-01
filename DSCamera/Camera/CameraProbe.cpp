#include "CameraProbe.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QDebug>

CameraProbe::CameraProbe(QObject *parent)
    : QObject{parent}
{
    // 拍图保存
    connect(this, &CameraProbe::captureFinished, this, [this](const QImage &img){
        QString dir_path = QCoreApplication::applicationDirPath() + "/Capture";
        QDir dir(dir_path);
        if (dir.exists() || dir.mkdir(dir_path)) {
            QString file_path = dir_path + QString("/%1.jpg").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hhmmss"));
            img.save(file_path);
        }
    });
}

void CameraProbe::capture()
{
    saveNext = true;
}

void CameraProbe::openCacheDir()
{
    QString dir_path = QCoreApplication::applicationDirPath() + "/Capture";
    QDir dir(dir_path);
    if (dir.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir_path));
    }
}

void CameraProbe::reset()
{
    saveNext = false;
}

void CameraProbe::previewUpdate(const QImage &img)
{
    if (saveNext) {
        saveNext = false;
        QMetaObject::invokeMethod(this, "captureFinished", Qt::QueuedConnection,
                                  Q_ARG(QImage, img));
    }
}

void CameraProbe::stillUpdate(const QImage &img)
{
    Q_UNUSED(img)
    // 响应硬件拍图信号，但是为了逻辑统一，实际还是调用capture
    saveNext = true;
}
