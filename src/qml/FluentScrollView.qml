import QtQuick
import QtQuick.Controls.Basic

Flickable {
    id: root
    required property var theme
    default property alias pageContent: contentHost.data

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    contentWidth: width
    contentHeight: contentHost.implicitHeight

    Item {
        id: contentHost
        width: root.width - 10
        implicitHeight: childrenRect.height
    }

    ScrollBar.vertical: ScrollBar {
        id: vbar
        width: 8
        policy: root.contentHeight > root.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        background: Rectangle {
            color: "transparent"
        }
        contentItem: Rectangle {
            implicitWidth: 6
            radius: 3
            color: vbar.pressed ? root.theme.accent
                 : vbar.hovered ? root.theme.borderStrong
                 : root.theme.border
            opacity: vbar.active || vbar.hovered ? 0.95 : 0.55
            Behavior on opacity { NumberAnimation { duration: 150 } }
            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }
}
