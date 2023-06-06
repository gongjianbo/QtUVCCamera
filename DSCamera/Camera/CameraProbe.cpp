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
        QString dir_path = getCacheDir();
        QDir dir(dir_path);
        if (dir.exists() || dir.mkdir(dir_path)) {
            QString file_path = dir_path + QString("/%1_拍图.jpg").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hhmmss"));
            QFile::remove(file_path);
            img.save(file_path);
        }
    });
}

bool CameraProbe::getRecording() const
{
    return recording;
}

void CameraProbe::setRecording(bool record)
{
    if (recording != record) {
        recording = record;
        emit recordingChanged();
    }
}

void CameraProbe::capture()
{
    saveNext = true;
}

void CameraProbe::startRecord()
{
    if (!corePtr || getRecording())
        return;
    QString dir_path = getCacheDir();
    QDir dir(dir_path);
    if (dir.exists() || dir.mkdir(dir_path)) {
        QString file_path = dir_path + QString("/%1_录制.avi").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hhmmss"));
        QFile::remove(file_path);
        if (corePtr->startRecord(file_path)) {
            saveFirst = true;
            setRecording(true);
        }
    }
}

void CameraProbe::stopRecord()
{
    if (!corePtr || !getRecording())
        return;
    corePtr->stopRecord();
    setRecording(false);
    if (!saveFirst) {
        QString dir_path = getCacheDir();
        QDir dir(dir_path);
        if (dir.exists() || dir.mkdir(dir_path)) {
            QString file_path = dir_path + QString("/%1_录制.jpg").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hhmmss"));
            QFile::remove(file_path);
            recordFirst.save(file_path);
        }
    }
}

void CameraProbe::openCacheDir()
{
    QString dir_path = getCacheDir();
    QDir dir(dir_path);
    if (dir.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir_path));
    }
}

void CameraProbe::attachCore(CameraCore *core)
{
    corePtr = core;
}

void CameraProbe::reset()
{
    saveNext = false;
    saveFirst = false;
    stopRecord();
}

void CameraProbe::previewUpdate(const QImage &img)
{
    if (saveNext) {
        qDebug()<<"capture finished"<<img.size();
        saveNext = false;
        QMetaObject::invokeMethod(this, "captureFinished", Qt::QueuedConnection,
                                  Q_ARG(QImage, img));
    }
    if (saveFirst) {
        saveFirst = false;
        recordFirst = img;
    }
}

void CameraProbe::stillUpdate(const QImage &img)
{
    Q_UNUSED(img)
    // 响应硬件拍图信号，但是为了逻辑统一，实际还是调用capture
    saveNext = true;
}

QString CameraProbe::getCacheDir()
{
    return QCoreApplication::applicationDirPath() + "/Capture";
}
