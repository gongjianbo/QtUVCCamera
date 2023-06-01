import QtQuick 2.15
import QtGraphicalEffects 1.15
// import Qt5Compat.GraphicalEffects

// 带阴影的 Rectangle
Item {
    id: control

    default property alias items: rect_area.children
    property alias color: rect_area.color
    property alias border: rect_area.border

    Rectangle {
        id: rect_area
        anchors.fill: parent
        clip: true
    }

    DropShadow {
        anchors.fill: rect_area
        horizontalOffset: 1
        verticalOffset: 1
        radius: 8
        samples: 16
        color: "#AAAAAA"
        source: rect_area
    }
}
