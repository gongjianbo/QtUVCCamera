import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Camera 1.0
import "../Component"

Item {
    id: control

    Item {
        anchors.fill: parent
        clip: true
        CameraView {
            id: camera_view
            anchors.fill: parent
            Component.onCompleted: {
                cameraCore.attachView(camera_view)
            }
        }
    }


    Row {
        spacing: 20
        ShadowRect {
            width: 160
            height: 30
            color: "#D7D7D7"
            Label {
                anchors.centerIn: parent
                text: "Size: " + cameraCore.resolution.width + "×" + cameraCore.resolution.height
            }
        }
        ShadowRect {
            width: 100
            height: 30
            color: "#D7D7D7"
            Label {
                anchors.centerIn: parent
                text:  "FPS: " + camera_view.fps
            }
        }
    }
}
