import QtQuick
import QtQuick.Effects

Item {
    id: root
    required property var theme
    property bool hoverLift: false
    property bool hovered: false
    property real cardRadius: 11
    property real cardPadding: 0
    property color cardColor: hovered && hoverLift ? theme.bgCardHover : theme.bgCard
    property color borderColor: theme.border
    property real cardBorderWidth: 1

    default property alias content: innerContainer.data

    implicitWidth: innerContainer.implicitWidth + cardPadding * 2
    implicitHeight: innerContainer.implicitHeight + cardPadding * 2

    Rectangle {
        id: bgShape
        anchors.fill: parent
        radius: root.cardRadius
        color: root.cardColor
        border.width: root.cardBorderWidth
        border.color: root.borderColor
        visible: false
    }

    MultiEffect {
        source: bgShape
        anchors.fill: bgShape
        shadowEnabled: true
        shadowColor: root.theme.shadowColor
        shadowBlur: root.hoverLift && root.hovered ? 0.65 : 0.35
        shadowVerticalOffset: root.hoverLift && root.hovered ? 4 : 2
        shadowHorizontalOffset: 0
        Behavior on shadowBlur { NumberAnimation { duration: 160 } }
        Behavior on shadowVerticalOffset { NumberAnimation { duration: 160 } }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cardRadius
        color: root.cardColor
        border.width: root.cardBorderWidth
        border.color: root.borderColor
        Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutQuad } }
        Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutQuad } }
    }

    Item {
        id: innerContainer
        anchors.fill: parent
        anchors.margins: root.cardPadding
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }
}
