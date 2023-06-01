import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "./Content"
import "./Component"

Window {
    width: 900
    height: 600
    visible: true
    title: qsTr("DirectShow Camera")
    color: "white"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12
        ShadowRect {
            Layout.fillHeight: true
            width: 240
            color: "#F0F0F0"
            SettingArea {
                anchors.fill: parent
                anchors.margins: 12
            }
        }
        ShadowRect {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#F0F0F0"
            VideoArea {
                anchors.fill: parent
                anchors.margins: 12
            }
        }
    }
}
