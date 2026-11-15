import QtQuick

Rectangle {
    id: root
    required property var theme
    property string text: ""
    property string placeholderText: ""
    property string fontFamily: "Microsoft YaHei UI"
    property int fontSize: 13
    signal textEdited(string newText)

    implicitHeight: 36
    radius: 8
    color: theme.bgInput
    border.width: inputField.activeFocus ? 1.5 : 1
    border.color: inputField.activeFocus ? theme.accent
                : hoverArea.hovered ? theme.borderStrong : theme.border

    Behavior on color { ColorAnimation { duration: 160 } }
    Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutQuad } }

    // Bottom Fluent accent focus indicator bar
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        height: 2
        radius: 1
        width: inputField.activeFocus ? Math.max(0, parent.width - 14) : 0
        color: theme.accent
        opacity: inputField.activeFocus ? 1.0 : 0.0
        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 160 } }
    }

    HoverHandler {
        id: hoverArea
    }

    TextInput {
        id: inputField
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        verticalAlignment: Text.AlignVCenter
        clip: true
        selectByMouse: true
        font.family: root.fontFamily
        font.pixelSize: root.fontSize
        color: root.theme.textPrimary
        selectionColor: root.theme.accentSoft
        selectedTextColor: root.theme.textPrimary
        text: root.text

        onTextChanged: {
            if (root.text !== text) {
                root.textEdited(text)
            }
        }

        Connections {
            target: root
            function onTextChanged() {
                if (inputField.text !== root.text) {
                    var pos = inputField.cursorPosition
                    inputField.text = root.text
                    inputField.cursorPosition = Math.min(pos, inputField.text.length)
                }
            }
        }

        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            text: root.placeholderText
            font: inputField.font
            color: root.theme.textMuted
            visible: inputField.text.length === 0
            elide: Text.ElideRight
        }
    }
}
