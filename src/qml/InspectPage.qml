import QtQuick
import QtQuick.Layouts

FluentScrollView {
    id: pageRoot
    required property var backend

    readonly property int depthIdx: pageRoot.backend.inspectMaxDepth === 1 ? 0
                                  : pageRoot.backend.inspectMaxDepth === 4 ? 1
                                  : pageRoot.backend.inspectMaxDepth === 8 ? 2 : 3

    Column {
        width: parent.width
        padding: 24
        topPadding: 20
        bottomPadding: 28
        spacing: 14

        // Header
        RowLayout {
            width: parent.width - 48
            spacing: 16

            Column {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: "链接检测与目录扫描"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: pageRoot.theme.textPrimary
                }
                Text {
                    width: parent.width
                    text: "快速识别符号链接、目录联接（Junction）、硬链接同胞组，定位断链并一键安全拆解"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    color: pageRoot.theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }

            StatusBadge {
                theme: pageRoot.theme
                label: pageRoot.backend.inspectScanning
                       ? "正在扫描..."
                       : (pageRoot.backend.scanResults.length > 0
                          ? ("发现 " + pageRoot.backend.scanResults.length + " 项")
                          : "准备就绪")
                tone: pageRoot.backend.inspectScanning ? 4
                    : (pageRoot.backend.scanResults.length > 0 ? 1 : 0)
            }
        }

        // Query & Scan Toolbar Card
        FluentCard {
            width: parent.width - 48
            theme: pageRoot.theme
            cardPadding: 16

            Column {
                width: parent.width
                spacing: 12

                RowLayout {
                    width: parent.width
                    spacing: 8

                    FluentInput {
                        Layout.fillWidth: true
                        theme: pageRoot.theme
                        text: pageRoot.backend.inspectQueryPath
                        placeholderText: "输入任意目录或文件路径，支持单点检测或深度遍历扫描..."
                        onTextEdited: function(newText) { pageRoot.backend.inspectQueryPath = newText }
                    }

                    FluentButton {
                        theme: pageRoot.theme
                        text: "选目录"
                        onClicked: pageRoot.backend.browseInspectFolder()
                    }
                    FluentButton {
                        theme: pageRoot.theme
                        text: "选文件"
                        onClicked: pageRoot.backend.browseInspectFile()
                    }
                    FluentButton {
                        theme: pageRoot.theme
                        text: "单项诊断"
                        onClicked: pageRoot.backend.inspectSinglePathNow()
                    }
                    FluentButton {
                        theme: pageRoot.theme
                        primary: !pageRoot.backend.inspectScanning
                        danger: pageRoot.backend.inspectScanning
                        busy: pageRoot.backend.inspectScanning
                        text: pageRoot.backend.inspectScanning ? "停止扫描" : "扫描目录"
                        minBtnWidth: 112
                        onClicked: {
                            if (pageRoot.backend.inspectScanning) pageRoot.backend.cancelDirectoryScan()
                            else pageRoot.backend.startDirectoryScan()
                        }
                    }
                }

                // Scan Depth & Hardlink Options Row
                RowLayout {
                    width: parent.width
                    spacing: 14

                    Text {
                        text: "扫描深度："
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 13
                        color: pageRoot.theme.textSecondary
                    }

                    AnimatedSegmentBar {
                        theme: pageRoot.theme
                        barHeight: 28
                        labels: ["1 层", "4 层", "8 层", "无限"]
                        currentIndex: pageRoot.depthIdx
                        onActivated: function(idx) {
                            pageRoot.backend.inspectMaxDepth = (idx === 0 ? 1 : idx === 1 ? 4 : idx === 2 ? 8 : 64)
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Row {
                        spacing: 7
                        Rectangle {
                            width: 17
                            height: 17
                            radius: 5
                            color: pageRoot.backend.inspectIncludeHardLinks ? pageRoot.theme.accent : pageRoot.theme.bgInput
                            border.width: pageRoot.backend.inspectIncludeHardLinks ? 0 : 1.5
                            border.color: pageRoot.theme.borderStrong
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on color { ColorAnimation { duration: 150 } }

                            VecIcon {
                                anchors.centerIn: parent
                                iconSize: 12
                                pathData: "M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"
                                iconColor: "#ffffff"
                                visible: pageRoot.backend.inspectIncludeHardLinks
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: pageRoot.backend.inspectIncludeHardLinks = !pageRoot.backend.inspectIncludeHardLinks
                            }
                        }

                        Text {
                            text: "包含多硬链接文件 (HardLink)"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 13
                            color: pageRoot.theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: pageRoot.backend.inspectIncludeHardLinks = !pageRoot.backend.inspectIncludeHardLinks
                            }
                        }
                    }
                }
            }
        }

        // Single Path Detailed Inspection Card
        FluentCard {
            visible: pageRoot.backend.hasSingleInspect
            width: parent.width - 48
            theme: pageRoot.theme
            cardPadding: 16
            borderColor: pageRoot.backend.singleInspect.isReparse
                         ? (pageRoot.backend.singleInspect.targetHealthy ? pageRoot.theme.successBd : pageRoot.theme.dangerBd)
                         : pageRoot.theme.border

            Column {
                width: parent.width
                spacing: 10

                RowLayout {
                    width: parent.width
                    spacing: 10

                    StatusBadge {
                        theme: pageRoot.theme
                        label: pageRoot.backend.singleInspect.kindLabel || ""
                        tone: pageRoot.backend.singleInspect.isReparse
                              ? (pageRoot.backend.singleInspect.targetHealthy ? 1 : 3)
                              : (pageRoot.backend.singleInspect.hardLinkCount > 1 ? 4 : 0)
                    }

                    Text {
                        Layout.fillWidth: true
                        text: pageRoot.backend.singleInspect.path || ""
                        font.family: "Consolas"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: pageRoot.theme.textPrimary
                        elide: Text.ElideMiddle
                    }

                    FluentButton {
                        theme: pageRoot.theme
                        text: "定位"
                        btnHeight: 30
                        onClicked: pageRoot.backend.revealPath(pageRoot.backend.singleInspect.path)
                    }

                    FluentButton {
                        visible: Boolean(pageRoot.backend.singleInspect.canUnlink)
                        theme: pageRoot.theme
                        danger: true
                        text: "安全拆解"
                        btnHeight: 30
                        onClicked: pageRoot.backend.requestUnlink(pageRoot.backend.singleInspect.path)
                    }
                }

                Rectangle {
                    visible: (pageRoot.backend.singleInspect.targetPath || "").length > 0
                    width: parent.width
                    implicitHeight: tgtText.implicitHeight + 16
                    radius: 8
                    color: pageRoot.theme.bgSubtle
                    border.width: 1
                    border.color: pageRoot.theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Text {
                            text: "指向目标 →"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            color: pageRoot.theme.textSecondary
                        }
                        Text {
                            id: tgtText
                            Layout.fillWidth: true
                            text: pageRoot.backend.singleInspect.targetPath || ""
                            font.family: "Consolas"
                            font.pixelSize: 13
                            color: pageRoot.backend.singleInspect.targetHealthy ? pageRoot.theme.accent : pageRoot.theme.dangerFg
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                Text {
                    visible: (pageRoot.backend.singleInspect.detailNote || "").length > 0
                    width: parent.width
                    text: pageRoot.backend.singleInspect.detailNote || ""
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    color: pageRoot.theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                // Hardlink sibling names
                Column {
                    visible: (pageRoot.backend.singleInspect.siblings || []).length > 0
                    width: parent.width
                    spacing: 4
                    Text {
                        text: "同磁盘数据块硬链接姐妹路径："
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: pageRoot.theme.infoFg
                    }
                    Repeater {
                        model: pageRoot.backend.singleInspect.siblings || []
                        Text {
                            required property string modelData
                            width: parent.width
                            text: "• " + modelData
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: pageRoot.theme.textSecondary
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }

        // Directory Scan Results Section
        Column {
            width: parent.width - 48
            spacing: 8

            RowLayout {
                width: parent.width
                spacing: 12
                Text {
                    text: "目录扫描结果（" + pageRoot.backend.scanResults.length + " 项）"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: pageRoot.theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: pageRoot.backend.inspectStatusText
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 12
                    color: pageRoot.theme.textMuted
                    elide: Text.ElideRight
                }
            }

            // Empty placeholder when not scanning and no items
            FluentCard {
                visible: pageRoot.backend.scanResults.length === 0 && !pageRoot.backend.hasSingleInspect
                width: parent.width
                theme: pageRoot.theme
                cardPadding: 28

                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    VecIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        iconSize: 34
                        pathData: pageRoot.theme.icSearch
                        iconColor: pageRoot.theme.textMuted
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "输入路径后点击“单项诊断”或“扫描目录”"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: pageRoot.theme.textSecondary
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "工具将自动解析 NTFS Reparse Point，校验目标可用性并列出硬链接关联"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 12
                        color: pageRoot.theme.textMuted
                    }
                }
            }

            Repeater {
                model: pageRoot.backend.scanResults

                Item {
                    id: scanRowHost
                    required property int index
                    required property var modelData
                    readonly property bool hovered: scanMa.containsMouse

                    width: parent.width
                    height: scanCard.implicitHeight

                    HoverHandler { id: scanHover }

                    FluentCard {
                        id: scanCard
                        width: parent.width
                        theme: pageRoot.theme
                        hoverLift: true
                        hovered: scanHover.hovered
                        cardPadding: 13

                        Column {
                            width: parent.width
                            spacing: 6

                            RowLayout {
                                width: parent.width
                                spacing: 10

                                StatusBadge {
                                    theme: pageRoot.theme
                                    label: modelData.kindCode
                                    tone: modelData.isReparse
                                          ? (modelData.targetHealthy ? 1 : 3)
                                          : 4
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.path
                                    font.family: "Consolas"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: pageRoot.theme.textPrimary
                                    elide: Text.ElideMiddle
                                }

                                FluentButton {
                                    theme: pageRoot.theme
                                    text: "详情"
                                    btnHeight: 28
                                    onClicked: pageRoot.backend.inspectSpecificPath(modelData.path)
                                }
                                FluentButton {
                                    theme: pageRoot.theme
                                    text: "定位"
                                    btnHeight: 28
                                    onClicked: pageRoot.backend.revealPath(modelData.path)
                                }
                                FluentButton {
                                    visible: modelData.canUnlink
                                    theme: pageRoot.theme
                                    danger: true
                                    text: "拆解"
                                    btnHeight: 28
                                    onClicked: pageRoot.backend.requestUnlink(modelData.path)
                                }
                            }

                            Text {
                                width: parent.width
                                text: modelData.targetPath.length > 0
                                      ? ("→ " + modelData.targetPath + (modelData.detailNote.length > 0 ? "  (" + modelData.detailNote + ")" : ""))
                                      : modelData.detailNote
                                font.family: "Consolas"
                                font.pixelSize: 12
                                color: modelData.isReparse && !modelData.targetHealthy
                                       ? pageRoot.theme.dangerFg
                                       : pageRoot.theme.textSecondary
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
