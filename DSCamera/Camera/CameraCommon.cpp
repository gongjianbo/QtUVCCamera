#include "CameraCommon.h"
#include <QDebug>

ULONG CameraCallback::AddRef()
{
    return 1;
}

ULONG CameraCallback::Release()
{
    return 1;
}

HRESULT CameraCallback::SampleCB(double time, IMediaSample *sample)
{
    Q_UNUSED(time)
    Q_UNUSED(sample)
    return S_OK;
}

HRESULT CameraCallback::BufferCB(double time, BYTE *buffer, long len)
{
    Q_UNUSED(time)
    if (!mRunning || !buffer || len <= 0)
        return S_OK;
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
    //qDebug()<<len<<mWidth<<mHeight<<this;
    QImage img = mConverter(buffer, len, mWidth, mHeight);
    if (mRunning && !img.isNull()) {
        mCallback(img);
        return S_OK;
    }
    return S_FALSE;
}

HRESULT CameraCallback::QueryInterface(const IID &iid, LPVOID *ppv)
{
    if(iid == IID_ISampleGrabberCB || iid == IID_IUnknown) {
        *ppv = reinterpret_cast<LPVOID *>(this);
        return S_OK;
    }
    return E_NOINTERFACE;
}

void CameraCallback::start()
{
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
    mRunning = true;
}

void CameraCallback::stop()
{
    mRunning = false;
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
}

void CameraCallback::setCallback(const std::function<void (const QImage &)> &callback)
{
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
    mCallback = callback;
}

void CameraCallback::setSubtype(GUID subtype)
{
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
    mSubtype = subtype;
    mConverter = subtypeConverter(subtype);
}

void CameraCallback::setSize(int width, int height)
{
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
    mWidth = width;
    mHeight = height;
}
