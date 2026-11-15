import QtQuick
import QtQuick.Layouts

FluentScrollView {
    id: pageRoot
    required property var backend

    readonly property var typeMeta: [
        { code: "/D", title: "目录符号链接", desc: "mklink /D，支持跨盘与相对路径，开发推荐" },
        { code: "/J", title: "目录联接", desc: "mklink /J，无需管理员权限，兼容性好" },
        { code: "/H", title: "文件硬链接", desc: "mklink /H，同卷共享文件数据块，删除互不影响" },
        { code: "FILE", title: "文件符号链接", desc: "mklink，支持跨分区指向单个文件" }
    ]

    Column {
        width: parent.width
        padding: 24
        topPadding: 20
        bottomPadding: 28
        spacing: 14

        // Header + Operation Mode switcher
        RowLayout {
            width: parent.width - 48
            spacing: 16

            Column {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    text: "创建 NTFS 链接"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: pageRoot.theme.textPrimary
                }
                Text {
                    text: pageRoot.backend.operationMode === 1
                          ? "先安全迁移真实文件到目标卷，再在原位置留下无缝链接（适合释放 C 盘空间）"
                          : "选择链接模式、配置入口路径与真实目标，实时生成预览命令并校验磁盘风险"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    color: pageRoot.theme.textSecondary
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }

            AnimatedSegmentBar {
                theme: pageRoot.theme
                labels: ["仅创建链接", "搬家并链接"]
                currentIndex: pageRoot.backend.operationMode
                onActivated: function(idx) { pageRoot.backend.operationMode = idx }
            }
        }

        // Section 1: 2x2 Link Type Cards
        Column {
            width: parent.width - 48
            spacing: 8

            RowLayout {
                width: parent.width
                Text {
                    text: "链接类型"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: pageRoot.theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: pageRoot.backend.operationMode === 1
                          ? "搬家模式下建议使用 /D 或 /J 目录链接，或对单个文件使用 FILE / H"
                          : "点击卡片切换 mklink 参数模式"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 12
                    color: pageRoot.theme.textMuted
                }
            }

            GridLayout {
                width: parent.width
                columns: 2
                rowSpacing: 10
                columnSpacing: 10

                Repeater {
                    model: pageRoot.typeMeta

                    Item {
                        id: cardSlot
                        required property int index
                        required property var modelData
                        readonly property bool selected: pageRoot.backend.linkType === index
                        readonly property bool hovered: cardMa.containsMouse

                        Layout.fillWidth: true
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 68

                        scale: cardMa.pressed ? 0.985 : (hovered ? 1.008 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

                        FluentCard {
                            anchors.fill: parent
                            theme: pageRoot.theme
                            hoverLift: true
                            hovered: cardSlot.hovered
                            cardRadius: 10
                            cardColor: cardSlot.selected ? pageRoot.theme.accentSoft
                                     : (cardSlot.hovered ? pageRoot.theme.bgCardHover : pageRoot.theme.bgCard)
                            borderColor: cardSlot.selected ? pageRoot.theme.accent
                                       : (cardSlot.hovered ? pageRoot.theme.borderStrong : pageRoot.theme.border)
                            cardBorderWidth: cardSlot.selected ? 1.5 : 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 10

                                // Mode code badge pill
                                Rectangle {
                                    Layout.preferredWidth: 50
                                    Layout.preferredHeight: 28
                                    radius: 7
                                    color: cardSlot.selected ? pageRoot.theme.accent : pageRoot.theme.bgSubtle
                                    border.width: cardSlot.selected ? 0 : 1
                                    border.color: pageRoot.theme.border
                                    Behavior on color { ColorAnimation { duration: 160 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.code
                                        font.family: "Consolas"
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                        color: cardSlot.selected ? pageRoot.theme.accentOn : pageRoot.theme.textPrimary
                                    }
                                }

                                Column {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: modelData.title
                                        font.family: "Microsoft YaHei UI"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: cardSlot.selected ? pageRoot.theme.accent : pageRoot.theme.textPrimary
                                    }
                                    Text {
                                        width: parent.width
                                        text: modelData.desc
                                        font.family: "Microsoft YaHei UI"
                                        font.pixelSize: 12
                                        color: pageRoot.theme.textSecondary
                                        elide: Text.ElideRight
                                    }
                                }

                                // Animated selection checkmark indicator
                                Item {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    scale: cardSlot.selected ? 1.0 : 0.4
                                    opacity: cardSlot.selected ? 1.0 : 0.0
                                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                                    Behavior on opacity { NumberAnimation { duration: 160 } }

                                    VecIcon {
                                        anchors.centerIn: parent
                                        iconSize: 18
                                        pathData: pageRoot.theme.icCheck
                                        iconColor: pageRoot.theme.accent
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: cardMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pageRoot.backend.linkType = index
                        }
                    }
                }
            }
        }

        // Section 2: Path Configuration Card
        FluentCard {
            width: parent.width - 48
            theme: pageRoot.theme
            cardPadding: 16

            Column {
                width: parent.width
                spacing: 12

                // Source / Link Path Row
                Column {
                    width: parent.width
                    spacing: 6

                    RowLayout {
                        width: parent.width
                        Text {
                            text: pageRoot.backend.operationMode === 1
                                  ? "原始位置（迁移后将在此处留下透明链接入口）"
                                  : "链接入口位置（新建的链接路径）"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: pageRoot.theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Row {
                            spacing: 6
                            Text {
                                text: "填入示例"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 12
                                color: exMa.containsMouse ? pageRoot.theme.accentHover : pageRoot.theme.accent
                                MouseArea {
                                    id: exMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: pageRoot.backend.fillExample()
                                }
                            }
                            Text {
                                text: "·"
                                font.pixelSize: 12
                                color: pageRoot.theme.textMuted
                            }
                            Text {
                                text: "清空"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 12
                                color: clrMa.containsMouse ? pageRoot.theme.dangerFg : pageRoot.theme.textSecondary
                                MouseArea {
                                    id: clrMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: pageRoot.backend.clearInputs()
                                }
                            }
                        }
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 8

                        FluentInput {
                            Layout.fillWidth: true
                            theme: pageRoot.theme
                            text: pageRoot.backend.linkPath
                            placeholderText: pageRoot.backend.operationMode === 1
                                             ? "例如：C:\\Users\\Demo\\AppData\\Local\\LargeCache"
                                             : "例如：D:\\Work\\ProjectLink"
                            onTextEdited: function(newText) { pageRoot.backend.linkPath = newText }
                        }

                        FluentButton {
                            theme: pageRoot.theme
                            text: pageRoot.backend.operationMode === 1
                                  ? (pageRoot.backend.linkType >= 2 ? "选择文件" : "选择目录")
                                  : "浏览..."
                            minBtnWidth: 88
                            onClicked: {
                                if (pageRoot.backend.operationMode === 1) {
                                    if (pageRoot.backend.linkType >= 2) pageRoot.backend.browseLinkFile()
                                    else pageRoot.backend.browseLinkFolder()
                                } else {
                                    pageRoot.backend.browseLinkSave()
                                }
                            }
                        }
                    }
                }

                // Divider + Spinning Swap Path Button
                RowLayout {
                    width: parent.width
                    spacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 1
                        Layout.alignment: Qt.AlignVCenter
                        color: pageRoot.theme.border
                    }

                    Rectangle {
                        id: swapBtn
                        Layout.preferredWidth: 112
                        Layout.preferredHeight: 28
                        radius: 14
                        color: swapMa.containsMouse ? pageRoot.theme.accentSoft : pageRoot.theme.bgSubtle
                        border.width: 1
                        border.color: swapMa.containsMouse ? pageRoot.theme.accentBorder : pageRoot.theme.border
                        scale: swapMa.pressed ? 0.94 : (swapMa.containsMouse ? 1.03 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 120 } }
                        Behavior on color { ColorAnimation { duration: 140 } }

                        Row {
                            anchors.centerIn: parent
                            spacing: 5
                            VecIcon {
                                id: swapIcon
                                iconSize: 15
                                pathData: pageRoot.theme.icSwap
                                iconColor: swapMa.containsMouse ? pageRoot.theme.accent : pageRoot.theme.textSecondary
                                anchors.verticalCenter: parent.verticalCenter
                                RotationAnimation on rotation {
                                    id: swapSpin
                                    running: false
                                    from: 0
                                    to: 180
                                    duration: 280
                                    easing.type: Easing.OutBack
                                }
                            }
                            Text {
                                text: "互换路径"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: swapMa.containsMouse ? pageRoot.theme.accent : pageRoot.theme.textSecondary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: swapMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                swapSpin.restart()
                                pageRoot.backend.swapPaths()
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 1
                        Layout.alignment: Qt.AlignVCenter
                        color: pageRoot.theme.border
                    }
                }

                // Target Path Row
                Column {
                    width: parent.width
                    spacing: 6

                    Text {
                        text: pageRoot.backend.operationMode === 1
                              ? "目标仓库（文件将迁移到此真实目录或路径）"
                              : "真实目标路径（链接最终指向的对象）"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: pageRoot.theme.textPrimary
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 8

                        FluentInput {
                            Layout.fillWidth: true
                            theme: pageRoot.theme
                            text: pageRoot.backend.targetPath
                            placeholderText: "例如：E:\\DataArchive\\LargeCache"
                            onTextEdited: function(newText) { pageRoot.backend.targetPath = newText }
                        }

                        FluentButton {
                            theme: pageRoot.theme
                            text: pageRoot.backend.linkType >= 2 ? "选择文件" : "选择目录"
                            minBtnWidth: 88
                            onClicked: {
                                if (pageRoot.backend.linkType >= 2) pageRoot.backend.browseTargetFile()
                                else pageRoot.backend.browseTargetFolder()
                            }
                        }
                    }
                }

                // Options row: Auto-create parent directory toggle
                Row {
                    width: parent.width
                    spacing: 9

                    Rectangle {
                        id: chkBox
                        width: 18
                        height: 18
                        radius: 5
                        anchors.verticalCenter: parent.verticalCenter
                        color: pageRoot.backend.autoCreateParent ? pageRoot.theme.accent : pageRoot.theme.bgInput
                        border.width: pageRoot.backend.autoCreateParent ? 0 : 1.5
                        border.color: pageRoot.theme.borderStrong
                        Behavior on color { ColorAnimation { duration: 150 } }

                        VecIcon {
                            anchors.centerIn: parent
                            iconSize: 13
                            pathData: "M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"
                            iconColor: "#ffffff"
                            visible: pageRoot.backend.autoCreateParent
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pageRoot.backend.autoCreateParent = !pageRoot.backend.autoCreateParent
                        }
                    }

                    Text {
                        text: "若父目录不存在则自动创建"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 13
                        color: pageRoot.theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pageRoot.backend.autoCreateParent = !pageRoot.backend.autoCreateParent
                        }
                    }
                }
            }
        }

        // Section 3: Live Validation Banner
        Rectangle {
            id: valBanner
            width: parent.width - 48
            implicitHeight: valRow.implicitHeight + 20
            radius: 10
            border.width: 1

            readonly property int lvl: pageRoot.backend.validationLevel // 0=Info,1=Valid,2=Warn,3=Error
            color: lvl === 1 ? pageRoot.theme.successBg
                 : lvl === 2 ? pageRoot.theme.warningBg
                 : lvl === 3 ? pageRoot.theme.dangerBg
                 : pageRoot.theme.infoBg
            border.color: lvl === 1 ? pageRoot.theme.successBd
                        : lvl === 2 ? pageRoot.theme.warningBd
                        : lvl === 3 ? pageRoot.theme.dangerBd
                        : pageRoot.theme.infoBd

            Behavior on color { ColorAnimation { duration: 180 } }
            Behavior on border.color { ColorAnimation { duration: 180 } }
            Behavior on implicitHeight { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

            RowLayout {
                id: valRow
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: 10

                VecIcon {
                    iconSize: 18
                    pathData: valBanner.lvl === 1 ? pageRoot.theme.icCheck : pageRoot.theme.icWarn
                    iconColor: valBanner.lvl === 1 ? pageRoot.theme.successFg
                             : valBanner.lvl === 2 ? pageRoot.theme.warningFg
                             : valBanner.lvl === 3 ? pageRoot.theme.dangerFg
                             : pageRoot.theme.infoFg
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    Layout.fillWidth: true
                    text: pageRoot.backend.validationMessage
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: valBanner.lvl === 1 ? pageRoot.theme.successFg
                         : valBanner.lvl === 2 ? pageRoot.theme.warningFg
                         : valBanner.lvl === 3 ? pageRoot.theme.dangerFg
                         : pageRoot.theme.infoFg
                    wrapMode: Text.WordWrap
                    Behavior on color { ColorAnimation { duration: 180 } }
                }

                FluentButton {
                    visible: pageRoot.backend.quickFixLabel.length > 0
                    theme: pageRoot.theme
                    text: pageRoot.backend.quickFixLabel
                    btnHeight: 28
                    onClicked: pageRoot.backend.applyQuickFix()
                }
            }
        }

        // Section 4: Command Preview & Execute Bar
        FluentCard {
            width: parent.width - 48
            theme: pageRoot.theme
            cardPadding: 14

            Column {
                width: parent.width
                spacing: 10

                RowLayout {
                    width: parent.width
                    spacing: 10

                    Text {
                        text: "命令预览"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: pageRoot.theme.textSecondary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: 8
                        color: pageRoot.theme.bgCode
                        border.width: 1
                        border.color: pageRoot.theme.border

                        TextInput {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: Text.AlignVCenter
                            readOnly: true
                            selectByMouse: true
                            clip: true
                            text: pageRoot.backend.commandPreview
                            font.family: "Consolas"
                            font.pixelSize: 13
                            color: pageRoot.theme.accent
                        }
                    }

                    FluentButton {
                        theme: pageRoot.theme
                        text: "复制"
                        iconPath: pageRoot.theme.icCopy
                        btnHeight: 34
                        onClicked: pageRoot.backend.copyCommand()
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: 10

                    FluentButton {
                        visible: pageRoot.backend.lastCreatedLink.length > 0
                        theme: pageRoot.theme
                        text: "在资源管理器打开"
                        onClicked: pageRoot.backend.revealCreatedLink()
                    }

                    FluentButton {
                        visible: pageRoot.backend.lastCreatedLink.length > 0
                        theme: pageRoot.theme
                        text: "立即体检此链接"
                        iconPath: pageRoot.theme.icSearch
                        onClicked: pageRoot.backend.inspectCreatedLink()
                    }

                    Item { Layout.fillWidth: true }

                    FluentButton {
                        theme: pageRoot.theme
                        primary: true
                        busy: pageRoot.backend.createBusy
                        buttonEnabled: pageRoot.backend.canExecute
                        minBtnWidth: 158
                        btnHeight: 38
                        text: pageRoot.backend.createBusy
                              ? "正在执行..."
                              : (pageRoot.backend.operationMode === 1 ? "搬家并创建链接" : "立即创建链接")
                        iconPath: pageRoot.theme.icAddLink
                        onClicked: pageRoot.backend.executeCreateAction()
                    }
                }
            }
        }

        // Section 5: Expandable Execution Console Log
        FluentCard {
            visible: pageRoot.backend.consoleOutput.length > 0
            opacity: pageRoot.backend.consoleOutput.length > 0 ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 220 } }
            width: parent.width - 48
            theme: pageRoot.theme
            cardPadding: 14

            Column {
                width: parent.width
                spacing: 8

                Text {
                    text: "执行输出日志"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: pageRoot.theme.textSecondary
                }

                Rectangle {
                    width: parent.width
                    height: Math.min(180, Math.max(56, logEdit.contentHeight + 18))
                    radius: 8
                    color: pageRoot.theme.bgCode
                    border.width: 1
                    border.color: pageRoot.theme.border

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 8
                        contentHeight: logEdit.contentHeight
                        clip: true

                        TextEdit {
                            id: logEdit
                            width: parent.width
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            text: pageRoot.backend.consoleOutput
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: pageRoot.theme.textPrimary
                        }
                    }
                }
            }
        }
    }
}
