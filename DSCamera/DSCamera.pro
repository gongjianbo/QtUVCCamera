QT += core
QT += gui
QT += widgets
QT += quick
QT += quickcontrols2
# QT += core5compat

CONFIG += c++11
CONFIG += utf8_source

RC_ICONS = Image/camera.ico
DESTDIR = $$PWD/../bin

INCLUDEPATH += $$PWD/Camera
include($$PWD/Camera/Camera.pri)

SOURCES += \
        main.cpp

RESOURCES += QML/qml.qrc \
    Image/img.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
