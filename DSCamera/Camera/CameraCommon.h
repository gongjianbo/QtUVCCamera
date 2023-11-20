#pragma once
#include <QVector>
#include <QList>
#include <QUuid>
#include <QImage>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory.h>
#include <string.h>
#include "SampleGrabber.h"
#include "ImageConverter.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(ptr) { if (ptr) ptr->Release(); ptr = NULL; }
#endif

// 释放AM_MEDIA_TYPE
inline void FreeMediaType(AM_MEDIA_TYPE &type) {
    if (type.cbFormat > 0)
        ::CoTaskMemFree(type.pbFormat);

    if (type.pUnk)
        type.pUnk->Release();

    ::SecureZeroMemory(&type, sizeof(type));
}

// 释放AM_MEDIA_TYPE*
inline void DeleteMediaType(AM_MEDIA_TYPE *type) {
    if (!type)
        return;
    FreeMediaType(*type);
    ::CoTaskMemFree(type);
}

// 视频回调
class CameraCallback : public ISampleGrabberCB
{
public:
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE SampleCB(double time, IMediaSample *sample) override;
    HRESULT STDMETHODCALLTYPE BufferCB(double time, BYTE *buffer, long len) override;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;

    // 设置running状态，=true准备好接收图像，提前设置好参数再start
    void start();
    // 设置running状态，=false时不处理图像
    void stop();
    // 回调函数设置
    void setCallback(const std::function<void(const QImage &image)> &callback);
    // 图像类型设置
    void setSubtype(GUID subtype);
    // 图像尺寸设置
    void setSize(int width, int height);

private:
    // 给running加锁，防止正在接收图像时用到的参数被修改，强制同步
    std::mutex mMutex;
    // =false时不处理图像
    std::atomic<bool> mRunning{false};
    // 处理好图像后通过回调传出
    std::function<void(const QImage &image)> mCallback;
    // 图像数据类型
    GUID mSubtype{MEDIASUBTYPE_NULL};
    // 数据类型对应的处理函数
    ImageConverter mConverter{convertEmpty};
    // 图像尺寸
    int mWidth{0};
    int mHeight{0};
};
