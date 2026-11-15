import QtQuick

Rectangle {
    id: root
    required property var theme
    property string text: ""
    property string iconPath: ""
    property bool primary: false
    property bool danger: false
    property bool busy: false
    property bool buttonEnabled: true
    property real minBtnWidth: 0
    property real btnHeight: 35

    signal clicked()

    implicitHeight: btnHeight
    implicitWidth: Math.max(minBtnWidth, rowContent.implicitWidth + 28)
    radius: 8
    border.width: primary ? 0 : 1

    readonly property bool interactive: buttonEnabled && !busy
    readonly property bool hovered: ma.containsMouse && interactive
    readonly property bool pressed: ma.pressed && interactive

    opacity: interactive ? 1.0 : 0.48
    scale: pressed ? 0.972 : (hovered ? 1.015 : 1.0)
    Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 150 } }

    color: {
        if (primary) {
            return pressed ? theme.accentPressed : (hovered ? theme.accentHover : theme.accent)
        }
        if (danger) {
            return hovered ? theme.dangerBg : theme.bgCard
        }
        return hovered ? theme.bgCardHover : theme.bgCard
    }
    border.color: {
        if (primary) return "transparent"
        if (danger) return theme.dangerBd
        return hovered ? theme.borderStrong : theme.border
    }

    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutQuad } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    Row {
        id: rowContent
        anchors.centerIn: parent
        spacing: 7

        // Animated spinner ring when busy
        Item {
            visible: root.busy
            width: 15
            height: 15
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                width: 15
                height: 15
                radius: 7.5
                color: "transparent"
                border.width: 2
                border.color: root.primary ? "#55ffffff" : theme.border

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    x: 4.5
                    y: -1.5
                    color: root.primary ? "#ffffff" : theme.accent
                }

                RotationAnimator on rotation {
                    running: root.busy
                    from: 0
                    to: 360
                    duration: 850
                    loops: Animation.Infinite
                }
            }
        }

        VecIcon {
            visible: !root.busy && root.iconPath.length > 0
            iconSize: 15
            pathData: root.iconPath
            iconColor: root.primary ? theme.accentOn
                     : root.danger ? theme.dangerFg
                     : theme.textPrimary
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.text
            font.family: "Microsoft YaHei UI"
            font.pixelSize: 13
            font.weight: root.primary ? Font.DemiBold : Font.Medium
            color: root.primary ? theme.accentOn
                 : root.danger ? theme.dangerFg
                 : theme.textPrimary
            Behavior on color { ColorAnimation { duration: 150 } }
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.interactive
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
