#pragma once
#include <QVector>
#include <QList>
#include <QImage>
#include <memory>
#include <atomic>
#include <functional>
#include <memory.h>
#include <string.h>
#include "SampleGrabber.h"

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

// 设备信息
struct CameraDevice
{
    // 设备信息字符串，包含devicePath等信息
    QString displayName;
    // 用于ui显示的设备名
    QString friendlyName;

    // 是否有效，主要区分返回值默认构造
    bool isValid() const {
        return !displayName.isEmpty();
    }
};

// 视频回调
class CameraCallback : public ISampleGrabberCB
{
public:
    ULONG STDMETHODCALLTYPE AddRef() override {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return 1;
    }
    HRESULT STDMETHODCALLTYPE SampleCB(double time, IMediaSample *sample) override {
        Q_UNUSED(time)
        Q_UNUSED(sample)
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE BufferCB(double time, BYTE *buffer, long len) override {
        Q_UNUSED(time)
        if (running && buffer && len > 0) {
            QImage img;
            // 目前只简单的处理 jpg 和 rgb
            // 在当前线程 copy 一次，避免跨线程后操作原来的内存
            if (isJpg) {
                QByteArray bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(buffer), len);
                img.loadFromData(bytes, "JPG");
                img = img.copy();
            } else if (len == width * height * 4) {
                // 如果是RGB32，小端模式[col * 4 + 3]Alpha通道没置零，要在此处置零
                // RGB24可以用Qt5.14新增的BGR888
                img = QImage(buffer, width, height, width * 4, QImage::Format_RGB32);
                for (int row = 0; row < height; row++)
                {
                    uchar *row_ptr = img.scanLine(row);
                    for (int col = 0; col < width; col++)
                    {
                        row_ptr[col * 4 + 3] = 0xFF;
                    }
                }
                // 默认是yuv，directshow转成rgb就上下翻转了
                img = img.mirrored(false, true);
            } else {
                return S_OK;
            }
            callback(img);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override {
        if(iid == IID_ISampleGrabberCB || iid == IID_IUnknown) {
            *ppv = reinterpret_cast<LPVOID *>(this);
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    // 提前设置好回调
    std::function<void(const QImage &image)> callback;
    std::atomic_bool running{false};
    bool isJpg{false};
    int width{0};
    int height{0};
};
