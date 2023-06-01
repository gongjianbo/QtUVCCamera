#include "CameraRegister.h"
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "Windows.h"
#include "dshow.h"
#include "CameraCore.h"
#include "CameraInfo.h"
#include "CameraProbe.h"
#include "CameraHotplug.h"
#include "CameraView.h"

// strmiids: DirectShow导出类标识符（CLSID）和接口标识符（IID）
#pragma comment(lib, "strmiids.lib")
// strmbase: DirectShow基类
#pragma comment(lib, "strmbase.lib")
// ole32: CoCreateInstance.CoInitialize
#pragma comment(lib, "ole32.lib")
// oleaut32: SysStringLen.VariantInit.VariantClear
#pragma comment(lib, "oleaut32.lib")

struct CameraGuard {
    CameraGuard() {
        ::CoInitialize(NULL);
        // ...
    }
    ~CameraGuard() {
        // ...
        ::CoUninitialize();
    }
};

CameraGuard guard;

void Camera::registerType(QQmlApplicationEngine *engine){
    qmlRegisterType<CameraCore>("Camera", 1, 0, "CameraCore");
    qmlRegisterType<CameraInfo>("Camera", 1, 0, "CameraInfo");
    qmlRegisterType<CameraProbe>("Camera", 1, 0, "CameraProbe");
    qmlRegisterType<CameraHotplug>("Camera", 1, 0, "CameraHotplug");
    qmlRegisterType<CameraView>("Camera", 1, 0, "CameraView");
    CameraCore *core = new CameraCore(qApp);
    engine->rootContext()->setContextProperty("cameraCore", core);
}
