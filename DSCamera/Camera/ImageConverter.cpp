#include "ImageConverter.h"
#include <QtMath>
#include <QDebug>

inline unsigned char rgbClip(int value)
{
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

QImage convertRGB32(const unsigned char *data, long len, int width, int height)
{
    if (width * height * 4 != len) {
        return convertEmpty(data, len, width, height);
    }
    // TODO yuy2转rgb后，stillpin的回调也触发了，待处理
    // 参考 QCamera 代码，rgb 时 bmiHeader.biHeight < 0 为上到下，否则下到上需要翻转
    QImage img(data, width, height, QImage::Format_RGB32);
    if (height > 0) {
        img = img.mirrored(false, true);
    } else {
        img = img.copy();
    }
    return img;
}

QImage convertMJPG(const unsigned char *data, long len, int width, int height)
{
    QByteArray bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(data), len);
    QImage img;
    img.loadFromData(bytes, "JPG");
    if (img.isNull()) {
        return convertEmpty(data, len, width, height);
    }
    if (img.format() != QImage::Format_RGB32) {
        img = img.convertToFormat(QImage::Format_RGB32);
    } else {
        img = img.copy();
    }
    return img;
}

QImage convertYUY2(const unsigned char *data, long len, int width, int height)
{
    if (width * height * 2 != len) {
        return convertEmpty(data, len, width, height);
    }
    QImage img(width, height, QImage::Format_RGB32);
    const unsigned char *p_yuv = data;
    int y1, u, y2, v;
    int r, g, b;
    for (int i = 0; i < height; i++)
    {
        unsigned char *p_rgb = img.scanLine(i);
        for (int j = 0; j < width; j += 2)
        {
            y1 = *p_yuv++ - 16;
            u = *p_yuv++ - 128;
            y2 = *p_yuv++ - 16;
            v = *p_yuv++ - 128;

            r = (298 * y1 + 516 * u + 128) >> 8;
            g = (298 * y1 - 100 * u - 208 * v + 128) >> 8;
            b = (298 * y1 + 409 * v + 128) >> 8;
            *p_rgb++ = rgbClip(r);
            *p_rgb++ = rgbClip(g);
            *p_rgb++ = rgbClip(b);
            *p_rgb++ = 0xFF;

            r = (298 * y2 + 516 * u + 128) >> 8;
            g = (298 * y2 - 100 * u - 208 * v + 128) >> 8;
            b = (298 * y2 + 409 * v + 128) >> 8;
            *p_rgb++ = rgbClip(r);
            *p_rgb++ = rgbClip(g);
            *p_rgb++ = rgbClip(b);
            *p_rgb++ = 0xFF;
        }
    }
    return img;
}

QImage convertEmpty(const unsigned char *data, long len, int width, int height)
{
    Q_UNUSED(data)
    Q_UNUSED(len)
    Q_UNUSED(width)
    Q_UNUSED(height)
    return QImage();
}

struct ConverterRow
{
    GUID subtype;
    ImageConverter converter;
};

static ConverterRow converterTable[] = {
    {MEDIASUBTYPE_RGB32, convertRGB32},
    {MEDIASUBTYPE_MJPG, convertMJPG},
    {MEDIASUBTYPE_YUY2, convertYUY2}
};

bool isValidSubtype(GUID subtype)
{
    for (auto &row : converterTable)
    {
        if (row.subtype == subtype)
            return true;
    }
    return false;
}

ImageConverter subtypeConverter(GUID subtype)
{
    for (auto &row : converterTable)
    {
        if (row.subtype == subtype)
            return row.converter;
    }
    return convertEmpty;
}
