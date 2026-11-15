import QtQuick

Rectangle {
    id: root
    required property var theme
    property var labels: []
    property int currentIndex: 0
    property real barHeight: 34
    property bool fullWidthTabs: false
    signal activated(int index)

    implicitHeight: barHeight + 6
    implicitWidth: rowItems.implicitWidth + 6
    radius: 9
    color: theme.bgSubtle
    border.width: 1
    border.color: theme.border

    Behavior on color { ColorAnimation { duration: 180 } }
    Behavior on border.color { ColorAnimation { duration: 180 } }

    readonly property var activeItem: (tabRepeater.count > currentIndex && currentIndex >= 0)
                                      ? tabRepeater.itemAt(currentIndex)
                                      : null

    Rectangle {
        id: pill
        visible: root.activeItem !== null
        x: root.activeItem ? (3 + root.activeItem.x) : 3
        y: 3
        width: root.activeItem ? root.activeItem.width : 0
        height: root.barHeight
        radius: 7
        color: theme.bgCard
        border.width: 1
        border.color: theme.accentBorder

        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 180 } }
    }

    Row {
        id: rowItems
        anchors.fill: parent
        anchors.margins: 3
        spacing: 2

        Repeater {
            id: tabRepeater
            model: root.labels

            Item {
                required property int index
                required property string modelData
                readonly property bool isCurrent: index === root.currentIndex
                readonly property bool hovered: itemMa.containsMouse

                height: root.barHeight
                width: root.fullWidthTabs && root.labels.length > 0
                       ? Math.max(64, (rowItems.width - (root.labels.length - 1) * rowItems.spacing) / root.labels.length)
                       : (tabLabel.implicitWidth + 24)

                Text {
                    id: tabLabel
                    anchors.centerIn: parent
                    text: modelData
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    font.weight: isCurrent ? Font.DemiBold : Font.Medium
                    color: isCurrent ? root.theme.accent
                         : hovered ? root.theme.textPrimary
                         : root.theme.textSecondary
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                MouseArea {
                    id: itemMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activated(index)
                }
            }
        }
    }
}
