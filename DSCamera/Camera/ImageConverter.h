#pragma once
#include <QImage>
#include <functional>
#include <map>
#include <memory.h>
#include <string.h>
#include "SampleGrabber.h"

// 图像格式转换
typedef std::function<QImage(const unsigned char *data, long len, int width, int height)> ImageConverter;

// MEDIASUBTYPE_RGB32
QImage convertRGB32(const unsigned char *data, long len, int width, int height);

// MEDIASUBTYPE_MJPG
QImage convertMJPG(const unsigned char *data, long len, int width, int height);

// MEDIASUBTYPE_YUY2
QImage convertYUY2(const unsigned char *data, long len, int width, int height);

// 不识别的转为空
QImage convertEmpty(const unsigned char *data, long len, int width, int height);

// 根据ID判断是否是支持的类型
bool isValidSubtype(GUID subtype);

// 根据GUID选择转换函数
ImageConverter subtypeConverter(GUID subtype);
