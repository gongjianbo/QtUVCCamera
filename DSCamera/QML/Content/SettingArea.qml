import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Component"

// 左侧操作及设置面板
ColumnLayout {
    id: control

    spacing: 12

    ComboBox {
        id: device_box
        height: 32
        Layout.fillWidth: true
        model: cameraCtrl.info.friendlyNames
        onActivated: {
            var ret = cameraCtrl.selectDevice(device_box.currentIndex)
            console.log("select", device_box.currentText, ret)
        }
        Connections {
            target: cameraCtrl
            // 如插入设备后顺序变化
            function onDeviceIndexChanged() {
                if (cameraCtrl.deviceIndex != device_box.currentIndex) {
                    device_box.currentIndex = cameraCtrl.deviceIndex
                }
            }
        }
    }

    Button {
        text: "设备切换测试 " + (switch_timer.running ? "on" : "off")
        onClicked: {
            switch_timer.running = !switch_timer.running
        }
        Timer {
            id: switch_timer
            interval: 2000
            repeat: true
            running: false
            onTriggered: {
                var count = device_box.count
                var index = device_box.currentIndex + 1
                if (index >= count) {
                    index = 0
                }

                device_box.currentIndex = index
                var ret = cameraCtrl.selectDevice(device_box.currentIndex)
                console.log("select", device_box.currentText, ret)
            }
        }
    }

    Button {
        text: "设置设备"
        onClicked: {
            cameraCtrl.popDeviceSetting(Window.window)
        }
    }

    Button {
        text: "设置格式"
        onClicked: {
            cameraCtrl.popFormatSetting(Window.window)
        }
    }

    Row {
        spacing: 12
        Button {
            text: "播放"
            onClicked: {
                cameraCtrl.play()
            }
        }
        Button {
            text: "暂停"
            onClicked: {
                cameraCtrl.pause()
            }
        }
        Button {
            text: "停止"
            onClicked: {
                cameraCtrl.stop()
            }
        }
    }

    Row {
        spacing: 12
        Button {
            text: "拍图"
            onClicked: {
                cameraCtrl.probe.capture()
            }
        }
        Button {
            text: cameraCtrl.probe.recording ? "结束" : "录制"
            onClicked: {
                // 录制的时候应该把其他操作禁用，不然会引发一系列问题
                if (cameraCtrl.probe.recording) {
                    cameraCtrl.probe.stopRecord()
                } else {
                    cameraCtrl.probe.startRecord()
                }
            }
        }
    }

    Button {
        text: "打开缓存文件夹"
        onClicked: {
            cameraCtrl.probe.openCacheDir()
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
