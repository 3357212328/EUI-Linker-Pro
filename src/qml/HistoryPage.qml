import QtQuick
import QtQuick.Layouts

FluentScrollView {
    id: pageRoot
    required property var backend

    Column {
        width: parent.width
        padding: 24
        topPadding: 20
        bottomPadding: 28
        spacing: 14

        // Header
        RowLayout {
            width: parent.width - 48
            spacing: 12

            Column {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: "操作历史记录"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: pageRoot.theme.textPrimary
                }
                Text {
                    text: "本地自动记录每一次链接创建、迁移与拆解操作，支持一键回填到新建表单"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    color: pageRoot.theme.textSecondary
                }
            }

            StatusBadge {
                theme: pageRoot.theme
                label: "共 " + pageRoot.backend.historyTotalCount + " 条"
                tone: 0
            }

            FluentButton {
                visible: pageRoot.backend.historyTotalCount > 0
                theme: pageRoot.theme
                danger: true
                text: "清空历史"
                onClicked: pageRoot.backend.requestClearHistory()
            }
        }

        // Search & Category Filter Card
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

                    FluentInput {
                        Layout.fillWidth: true
                        theme: pageRoot.theme
                        text: pageRoot.backend.historyFilterText
                        placeholderText: "按路径、类型代码或执行结果关键词过滤..."
                        onTextEdited: function(newText) { pageRoot.backend.historyFilterText = newText }
                    }

                    FluentButton {
                        visible: pageRoot.backend.historyFilterText.length > 0 || pageRoot.backend.historyCategoryFilter !== 0
                        theme: pageRoot.theme
                        text: "重置过滤"
                        onClicked: {
                            pageRoot.backend.historyFilterText = ""
                            pageRoot.backend.historyCategoryFilter = 0
                        }
                    }
                }

                AnimatedSegmentBar {
                    theme: pageRoot.theme
                    barHeight: 30
                    labels: ["全部", "/D", "/J", "/H", "FILE", "搬家", "仅成功"]
                    currentIndex: pageRoot.backend.historyCategoryFilter
                    onActivated: function(idx) { pageRoot.backend.historyCategoryFilter = idx }
                }
            }
        }

        // Empty State
        FluentCard {
            visible: pageRoot.backend.historyRows.length === 0
            width: parent.width - 48
            theme: pageRoot.theme
            cardPadding: 32

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                VecIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    iconSize: 34
                    pathData: pageRoot.theme.icHistory
                    iconColor: pageRoot.theme.textMuted
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: pageRoot.backend.historyTotalCount === 0 ? "暂无操作历史" : "当前筛选条件下无匹配记录"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: pageRoot.theme.textSecondary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: pageRoot.backend.historyTotalCount === 0
                          ? "在“创建链接”页执行操作后，记录将自动保存于本地配置目录"
                          : "尝试调整上方的类型筛选标签或搜索关键词"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 12
                    color: pageRoot.theme.textMuted
                }
            }
        }

        // History Items List
        Column {
            width: parent.width - 48
            spacing: 10

            Repeater {
                model: pageRoot.backend.historyRows

                Item {
                    id: histRow
                    required property int index
                    required property var modelData

                    width: parent.width
                    height: histCard.implicitHeight

                    HoverHandler { id: rowHover }

                    FluentCard {
                        id: histCard
                        width: parent.width
                        theme: pageRoot.theme
                        hoverLift: true
                        hovered: rowHover.hovered
                        cardPadding: 14

                        Column {
                            width: parent.width
                            spacing: 8

                            RowLayout {
                                width: parent.width
                                spacing: 10

                                StatusBadge {
                                    theme: pageRoot.theme
                                    label: modelData.typeCode
                                    tone: modelData.success ? 4 : 3
                                }

                                StatusBadge {
                                    theme: pageRoot.theme
                                    label: modelData.success ? "成功" : "失败"
                                    tone: modelData.success ? 1 : 3
                                }

                                Text {
                                    text: modelData.timestamp
                                    font.family: "Consolas"
                                    font.pixelSize: 12
                                    color: pageRoot.theme.textMuted
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.result
                                    font.family: "Microsoft YaHei UI"
                                    font.pixelSize: 13
                                    color: pageRoot.theme.textSecondary
                                    elide: Text.ElideRight
                                }

                                FluentButton {
                                    theme: pageRoot.theme
                                    text: "回填表单"
                                    btnHeight: 28
                                    onClicked: pageRoot.backend.backfillHistory(modelData.sourceIndex)
                                }
                                FluentButton {
                                    theme: pageRoot.theme
                                    text: "体检"
                                    btnHeight: 28
                                    onClicked: pageRoot.backend.inspectHistoryPath(modelData.linkPath)
                                }
                                FluentButton {
                                    theme: pageRoot.theme
                                    text: "定位"
                                    btnHeight: 28
                                    onClicked: pageRoot.backend.revealPath(modelData.linkPath)
                                }
                            }

                            Rectangle {
                                width: parent.width
                                implicitHeight: pathCol.implicitHeight + 12
                                radius: 8
                                color: pageRoot.theme.bgSubtle
                                border.width: 1
                                border.color: pageRoot.theme.border

                                Column {
                                    id: pathCol
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 3

                                    Text {
                                        width: parent.width
                                        text: "入口: " + modelData.linkPath
                                        font.family: "Consolas"
                                        font.pixelSize: 12
                                        color: pageRoot.theme.textPrimary
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        visible: modelData.targetPath.length > 0
                                        width: parent.width
                                        text: "目标: " + modelData.targetPath
                                        font.family: "Consolas"
                                        font.pixelSize: 12
                                        color: pageRoot.theme.textSecondary
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
