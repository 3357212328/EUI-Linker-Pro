import QtQuick

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

    // Pure QtQuick custom slim scrollbar (avoids pulling QtQuick.Controls2 DLLs)
    Item {
        id: vTrack
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 2
        width: 10
        visible: root.contentHeight > root.height + 1

        readonly property real maxContentY: Math.max(0, root.contentHeight - root.height)
        readonly property real usableHeight: Math.max(1, height - 8)
        readonly property real thumbH: Math.max(28, Math.min(usableHeight, root.visibleArea.heightRatio * usableHeight))
        readonly property real maxThumbY: Math.max(0, usableHeight - thumbH)

        HoverHandler { id: trackHover }

        Rectangle {
            id: thumb
            anchors.right: parent.right
            anchors.rightMargin: 2
            y: 4 + (vTrack.maxContentY > 0 ? (root.contentY / vTrack.maxContentY) * vTrack.maxThumbY : 0)
            width: dragMa.pressed || trackHover.hovered ? 6 : 4
            height: vTrack.thumbH
            radius: width / 2
            color: dragMa.pressed ? root.theme.accent
                 : trackHover.hovered ? root.theme.borderStrong
                 : root.theme.border
            opacity: dragMa.pressed || trackHover.hovered || root.moving ? 0.95 : 0.55

            Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
            Behavior on opacity { NumberAnimation { duration: 150 } }
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        MouseArea {
            id: dragMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            property real grabOffsetY: 0

            function scrollToTrackY(trackY) {
                if (vTrack.maxThumbY <= 0 || vTrack.maxContentY <= 0) return
                var ty = Math.max(0, Math.min(vTrack.maxThumbY, trackY - 4 - grabOffsetY))
                root.contentY = (ty / vTrack.maxThumbY) * vTrack.maxContentY
            }

            onPressed: function(mouse) {
                if (mouse.y >= thumb.y && mouse.y <= thumb.y + thumb.height) {
                    grabOffsetY = mouse.y - thumb.y
                } else {
                    grabOffsetY = vTrack.thumbH * 0.5
                    scrollToTrackY(mouse.y)
                }
            }

            onPositionChanged: function(mouse) {
                if (pressed) scrollToTrackY(mouse.y)
            }
        }
    }
}
