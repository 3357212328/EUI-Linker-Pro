import QtQuick

Rectangle {
    id: root
    required property var theme
    property string label: ""
    property int tone: 0 // 0=neutral, 1=success, 2=warning, 3=danger, 4=info

    implicitHeight: 24
    implicitWidth: txt.implicitWidth + 16
    radius: 6
    border.width: 1

    color: tone === 1 ? theme.successBg
         : tone === 2 ? theme.warningBg
         : tone === 3 ? theme.dangerBg
         : tone === 4 ? theme.infoBg
         : theme.bgSubtle
    border.color: tone === 1 ? theme.successBd
                : tone === 2 ? theme.warningBd
                : tone === 3 ? theme.dangerBd
                : tone === 4 ? theme.infoBd
                : theme.border

    Behavior on color { ColorAnimation { duration: 180 } }
    Behavior on border.color { ColorAnimation { duration: 180 } }

    Text {
        id: txt
        anchors.centerIn: parent
        text: root.label
        font.family: "Microsoft YaHei UI"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        color: root.tone === 1 ? theme.successFg
             : root.tone === 2 ? theme.warningFg
             : root.tone === 3 ? theme.dangerFg
             : root.tone === 4 ? theme.infoFg
             : theme.textSecondary
        Behavior on color { ColorAnimation { duration: 180 } }
    }
}
