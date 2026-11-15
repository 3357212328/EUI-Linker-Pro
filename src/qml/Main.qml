import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Effects

Window {
    id: win
    width: 1180
    height: 820
    minimumWidth: 980
    minimumHeight: 680
    visible: true
    title: "NTFS 符号链接工作台"
    color: appTheme.bgCanvas

    Behavior on color { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }

    readonly property var appBackend: backend

    AppTheme {
        id: appTheme
        dark: win.appBackend.darkMode
    }

    // Keyboard shortcuts
    Shortcut { sequence: "Ctrl+1"; onActivated: backend.activeTab = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: backend.activeTab = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: backend.activeTab = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: backend.activeTab = 3 }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (confirmDialog.openState) {
                confirmDialog.openState = false
                backend.confirmCanceled()
            } else if (toastBox.visibleState) {
                toastBox.visibleState = false
            }
        }
    }

    // Backend Confirm & Toast Connections
    Connections {
        target: backend
        function onConfirmRequested(title, message, primaryLabel) {
            confirmDialog.titleText = title
            confirmDialog.bodyText = message
            confirmDialog.acceptText = primaryLabel
            confirmDialog.openState = true
        }
        function onToastRequested(title, message, tone) {
            toastBox.showToast(title, message, tone)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ================= LEFT SIDEBAR =================
        Rectangle {
            id: sidebar
            Layout.preferredWidth: 238
            Layout.fillHeight: true
            color: appTheme.bgSidebar
            Behavior on color { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }

            // Right hairline divider
            Rectangle {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 1
                color: appTheme.border
                Behavior on color { ColorAnimation { duration: 220 } }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                // Brand Identity Header
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 11

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: appTheme.bgCard
                        border.width: 1
                        border.color: appTheme.border

                        Image {
                            anchors.fill: parent
                            anchors.margins: 4
                            source: "qrc:/qt/qml/FluentLinker/assets/icon.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            text: "Fluent Linker"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            color: appTheme.textPrimary
                        }
                        Text {
                            text: "Pro · NTFS 工作台"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 12
                            color: appTheme.textSecondary
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: appTheme.border
                }

                // Navigation Rail with Spring Sliding Indicator
                Item {
                    id: navContainer
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4 * 46

                    readonly property var navItems: [
                        { label: "创建链接", sub: "符号 / 联接 / 搬家", icon: appTheme.icAddLink },
                        { label: "链接检测", sub: "诊断与安全拆解", icon: appTheme.icSearch },
                        { label: "历史记录", sub: "回填与追溯", icon: appTheme.icHistory },
                        { label: "权限与指南", sub: "环境检查与对照", icon: appTheme.icShield }
                    ]

                    // Sliding Active Background Capsule
                    Rectangle {
                        x: 0
                        y: backend.activeTab * 46
                        width: navContainer.width
                        height: 42
                        radius: 9
                        color: appTheme.accentSoft
                        border.width: 1
                        border.color: appTheme.accentBorder

                        Behavior on y {
                            SpringAnimation {
                                spring: 5.5
                                damping: 0.34
                                epsilon: 0.25
                            }
                        }
                        Behavior on color { ColorAnimation { duration: 180 } }

                        // Left Azure vertical accent bar
                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 3
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3.5
                            height: 20
                            radius: 2
                            color: appTheme.accent
                        }
                    }

                    Column {
                        anchors.fill: parent
                        spacing: 4

                        Repeater {
                            model: navContainer.navItems

                            Rectangle {
                                id: navRow
                                required property int index
                                required property var modelData
                                readonly property bool isActive: backend.activeTab === index
                                readonly property bool hovered: navMa.containsMouse

                                width: navContainer.width
                                height: 42
                                radius: 9
                                color: !isActive && hovered ? appTheme.bgCardHover : "transparent"
                                Behavior on color { ColorAnimation { duration: 140 } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 13
                                    anchors.rightMargin: 10
                                    spacing: 10

                                    VecIcon {
                                        iconSize: 18
                                        pathData: modelData.icon
                                        iconColor: navRow.isActive ? appTheme.accent
                                                 : navRow.hovered ? appTheme.textPrimary
                                                 : appTheme.textSecondary
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Text {
                                            text: modelData.label
                                            font.family: "Microsoft YaHei UI"
                                            font.pixelSize: 14
                                            font.weight: navRow.isActive ? Font.DemiBold : Font.Medium
                                            color: navRow.isActive ? appTheme.accent
                                                 : navRow.hovered ? appTheme.textPrimary
                                                 : appTheme.textSecondary
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }
                                        Text {
                                            text: modelData.sub
                                            font.family: "Microsoft YaHei UI"
                                            font.pixelSize: 11
                                            color: appTheme.textMuted
                                        }
                                    }

                                    // History count pill
                                    Rectangle {
                                        visible: index === 2 && backend.historyTotalCount > 0
                                        Layout.preferredWidth: Math.max(22, cntTxt.implicitWidth + 10)
                                        Layout.preferredHeight: 19
                                        radius: 9.5
                                        color: navRow.isActive ? appTheme.accent : appTheme.bgCard
                                        border.width: navRow.isActive ? 0 : 1
                                        border.color: appTheme.border

                                        Text {
                                            id: cntTxt
                                            anchors.centerIn: parent
                                            text: backend.historyTotalCount
                                            font.family: "Consolas"
                                            font.pixelSize: 11
                                            font.weight: Font.Bold
                                            color: navRow.isActive ? "#ffffff" : appTheme.textSecondary
                                        }
                                    }
                                }

                                MouseArea {
                                    id: navMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: backend.activeTab = index
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Privilege Readiness Quick Chip
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: 9
                    color: (backend.isAdmin || backend.isDevMode) ? appTheme.successBg : appTheme.warningBg
                    border.width: 1
                    border.color: (backend.isAdmin || backend.isDevMode) ? appTheme.successBd : appTheme.warningBd
                    scale: privMa.pressed ? 0.97 : 1.0
                    Behavior on scale { NumberAnimation { duration: 110 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 11
                        anchors.rightMargin: 11
                        spacing: 9

                        VecIcon {
                            iconSize: 17
                            pathData: (backend.isAdmin || backend.isDevMode) ? appTheme.icCheck : appTheme.icWarn
                            iconColor: (backend.isAdmin || backend.isDevMode) ? appTheme.successFg : appTheme.warningFg
                        }

                        Column {
                            Layout.fillWidth: true
                            Text {
                                text: backend.isAdmin ? "管理员模式已就绪"
                                    : backend.isDevMode ? "开发者免提权就绪"
                                    : "建议提权或开开发者模式"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: (backend.isAdmin || backend.isDevMode) ? appTheme.successFg : appTheme.warningFg
                                elide: Text.ElideRight
                                width: parent.width
                            }
                            Text {
                                text: "点击查看系统权限细节"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 11
                                color: appTheme.textSecondary
                            }
                        }
                    }

                    MouseArea {
                        id: privMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.activeTab = 3
                    }
                }

                // Animated Dark / Light Theme Toggle Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 9
                    color: appThemeMa.containsMouse ? appTheme.bgCardHover : appTheme.bgCard
                    border.width: 1
                    border.color: appThemeMa.containsMouse ? appTheme.borderStrong : appTheme.border
                    scale: appThemeMa.pressed ? 0.97 : 1.0
                    Behavior on scale { NumberAnimation { duration: 110 } }
                    Behavior on color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 9

                        VecIcon {
                            id: appThemeIcon
                            iconSize: 16
                            pathData: appTheme.dark ? appTheme.icSun : appTheme.icMoon
                            iconColor: appTheme.dark ? appTheme.warningFg : appTheme.accent
                            rotation: appTheme.dark ? 0 : 360
                            Behavior on rotation { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: appTheme.dark ? "切换浅色主题" : "切换深色主题"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: appTheme.textPrimary
                        }

                        // Pill indicator
                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 18
                            radius: 9
                            color: appTheme.dark ? appTheme.accent : appTheme.borderStrong
                            Behavior on color { ColorAnimation { duration: 180 } }

                            Rectangle {
                                width: 14
                                height: 14
                                radius: 7
                                y: 2
                                x: appTheme.dark ? 18 : 2
                                color: "#ffffff"
                                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                            }
                        }
                    }

                    MouseArea {
                        id: appThemeMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.toggleTheme()
                    }
                }
            }
        }

        // ================= MAIN PAGE WORKSPACE =================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            CreatePage {
                anchors.fill: parent
                theme: appTheme
                backend: win.appBackend
                visible: opacity > 0.01
                opacity: backend.activeTab === 0 ? 1.0 : 0.0
                transform: Translate {
                    y: backend.activeTab === 0 ? 0 : 12
                    Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                }
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
            }

            InspectPage {
                anchors.fill: parent
                theme: appTheme
                backend: win.appBackend
                visible: opacity > 0.01
                opacity: backend.activeTab === 1 ? 1.0 : 0.0
                transform: Translate {
                    y: backend.activeTab === 1 ? 0 : 12
                    Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                }
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
            }

            HistoryPage {
                anchors.fill: parent
                theme: appTheme
                backend: win.appBackend
                visible: opacity > 0.01
                opacity: backend.activeTab === 2 ? 1.0 : 0.0
                transform: Translate {
                    y: backend.activeTab === 2 ? 0 : 12
                    Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                }
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
            }

            GuidePage {
                anchors.fill: parent
                theme: appTheme
                backend: win.appBackend
                visible: opacity > 0.01
                opacity: backend.activeTab === 3 ? 1.0 : 0.0
                transform: Translate {
                    y: backend.activeTab === 3 ? 0 : 12
                    Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                }
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
            }
        }
    }

    // ================= CONFIRMATION MODAL OVERLAY =================
    Item {
        id: confirmDialog
        anchors.fill: parent
        z: 90
        property bool openState: false
        property string titleText: ""
        property string bodyText: ""
        property string acceptText: "确认"

        visible: opacity > 0.01
        opacity: openState ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }

        Rectangle {
            anchors.fill: parent
            color: "#88000000"
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    confirmDialog.openState = false
                    backend.confirmCanceled()
                }
            }
        }

        FluentCard {
            anchors.centerIn: parent
            width: Math.min(480, win.width - 64)
            theme: appTheme
            cardRadius: 14
            cardPadding: 22
            scale: confirmDialog.openState ? 1.0 : 0.90
            Behavior on scale {
                NumberAnimation { duration: 220; easing.type: Easing.OutBack }
            }

            // Swallow clicks inside modal card
            MouseArea {
                anchors.fill: parent
                z: -1
            }

            Column {
                width: parent.width
                spacing: 12

                RowLayout {
                    width: parent.width
                    spacing: 10
                    VecIcon {
                        iconSize: 22
                        pathData: appTheme.icWarn
                        iconColor: appTheme.warningFg
                    }
                    Text {
                        Layout.fillWidth: true
                        text: confirmDialog.titleText
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: appTheme.textPrimary
                        wrapMode: Text.WordWrap
                    }
                }

                Text {
                    width: parent.width
                    text: confirmDialog.bodyText
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    lineHeight: 1.45
                    color: appTheme.textSecondary
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    width: parent.width
                    spacing: 10
                    Item { Layout.fillWidth: true }

                    FluentButton {
                        theme: appTheme
                        text: "取消"
                        minBtnWidth: 86
                        onClicked: {
                            confirmDialog.openState = false
                            backend.confirmCanceled()
                        }
                    }

                    FluentButton {
                        theme: appTheme
                        primary: true
                        text: confirmDialog.acceptText
                        minBtnWidth: 106
                        onClicked: {
                            confirmDialog.openState = false
                            backend.confirmAccepted()
                        }
                    }
                }
            }
        }
    }

    // ================= FLOATING ANIMATED TOAST =================
    Item {
        id: toastBox
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 22
        anchors.bottomMargin: 22
        z: 100
        width: Math.min(390, win.width - 44)
        height: toastCard.implicitHeight

        property bool visibleState: false
        property string toastTitle: ""
        property string toastMsg: ""
        property int toastTone: 0 // 0=info, 1=success, 2=warning, 3=error
        property real progressValue: 0.0

        function showToast(title, msg, tone) {
            toastTitle = title
            toastMsg = msg
            toastTone = tone
            visibleState = true
            progressAnim.stop()
            progressValue = 1.0
            progressAnim.start()
            hideTimer.restart()
        }

        visible: opacity > 0.01
        opacity: visibleState ? 1.0 : 0.0
        transform: Translate {
            x: toastBox.visibleState ? 0 : 28
            Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }

        Timer {
            id: hideTimer
            interval: 3800
            onTriggered: toastBox.visibleState = false
        }

        NumberAnimation {
            id: progressAnim
            target: toastBox
            property: "progressValue"
            from: 1.0
            to: 0.0
            duration: 3800
        }

        FluentCard {
            id: toastCard
            width: parent.width
            theme: appTheme
            cardRadius: 11
            cardPadding: 14
            borderColor: toastBox.toastTone === 1 ? appTheme.successBd
                       : toastBox.toastTone === 2 ? appTheme.warningBd
                       : toastBox.toastTone === 3 ? appTheme.dangerBd
                       : appTheme.infoBd

            Column {
                width: parent.width
                spacing: 8

                RowLayout {
                    width: parent.width
                    spacing: 10

                    VecIcon {
                        iconSize: 18
                        pathData: toastBox.toastTone === 1 ? appTheme.icCheck : appTheme.icWarn
                        iconColor: toastBox.toastTone === 1 ? appTheme.successFg
                                 : toastBox.toastTone === 2 ? appTheme.warningFg
                                 : toastBox.toastTone === 3 ? appTheme.dangerFg
                                 : appTheme.infoFg
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            width: parent.width
                            text: toastBox.toastTitle
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            color: appTheme.textPrimary
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: toastBox.toastMsg.length > 0
                            width: parent.width
                            text: toastBox.toastMsg
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 12
                            color: appTheme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                    }

                    Text {
                        text: "✕"
                        font.pixelSize: 13
                        color: appTheme.textMuted
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                hideTimer.stop()
                                toastBox.visibleState = false
                            }
                        }
                    }
                }

                // Smooth countdown bar
                Rectangle {
                    width: parent.width
                    height: 3
                    radius: 1.5
                    color: appTheme.bgSubtle

                    Rectangle {
                        width: parent.width * toastBox.progressValue
                        height: parent.height
                        radius: 1.5
                        color: toastBox.toastTone === 1 ? appTheme.successFg
                             : toastBox.toastTone === 2 ? appTheme.warningFg
                             : toastBox.toastTone === 3 ? appTheme.dangerFg
                             : appTheme.infoFg
                    }
                }
            }
        }
    }
}
