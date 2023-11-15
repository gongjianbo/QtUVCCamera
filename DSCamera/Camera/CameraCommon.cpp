#include "CameraCommon.h"

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

    bool is_ok = false;
    QImage img;
    // 在当前线程 copy 一次，避免跨线程后操作原来的内存
    // TODO 根据类型切换处理函数的指针，不在这里 switch
    if (MEDIASUBTYPE_MJPG == mType) {
        QByteArray bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(buffer), len);
        img.loadFromData(bytes, "JPG");
        img = img.copy();
        is_ok = true;
    } else if (MEDIASUBTYPE_RGB32 == mType && len == mWidth * mHeight * 4) {
        // 如果是RGB32，小端模式[col * 4 + 3]Alpha通道没置零，要在此处置零
        // RGB24可以用Qt5.14新增的BGR888
        img = QImage(buffer, mWidth, mHeight, mWidth * 4, QImage::Format_RGB32);
        for (int row = 0; row < mHeight; row++)
        {
            uchar *row_ptr = img.scanLine(row);
            for (int col = 0; col < mWidth; col++)
            {
                row_ptr[col * 4 + 3] = 0xFF;
            }
        }
        // 默认是yuv，directshow转成rgb就上下翻转了
        img = img.mirrored(false, true);
        is_ok = true;
    } else {
        // 有的设置转成rgb会一直触发stillpin
        return S_FALSE;
    }
    if (mRunning && is_ok) {
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

void CameraCallback::setType(GUID type)
{
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
    mType = type;
}

void CameraCallback::setSize(int width, int height)
{
    std::lock_guard<std::mutex> guard(mMutex);
    Q_UNUSED(guard)
    mWidth = width;
    mHeight = height;
}
