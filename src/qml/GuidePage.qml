import QtQuick
import QtQuick.Layouts

FluentScrollView {
    id: pageRoot
    required property var backend

    readonly property var matrixData: [
        {
            code: "/D", name: "目录符号链接 (Symbolic Link)", badgeTone: 4,
            points: "支持跨驱动器、网络 UNC 与相对路径；资源管理器中透明穿透访问；需要管理员权限或开启 Windows 开发者模式。"
        },
        {
            code: "/J", name: "目录联接点 (Directory Junction)", badgeTone: 1,
            points: "无需管理员权限即可在任意本地 NTFS 分区创建；非常适合游戏库、缓存目录搬家；始终使用绝对路径绑定本地卷。"
        },
        {
            code: "/H", name: "文件硬链接 (Hard Link)", badgeTone: 0,
            points: "与源文件共用同一 NTFS 数据簇，免提权即可创建；仅限同一分区（盘符）内使用；删除任一入口不影响其他副本。"
        },
        {
            code: "FILE", name: "文件符号链接 (File Symlink)", badgeTone: 2,
            points: "指向单个文件的透明符号重定向，支持跨盘与相对路径；适合配置文件同步，需要管理员或开发者模式权限。"
        }
    ]

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
                    text: "权限中心与使用指南"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: pageRoot.theme.textPrimary
                }
                Text {
                    text: "检视当前系统提权条件，理解不同 NTFS 链接的安全特性与最佳实践"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    color: pageRoot.theme.textSecondary
                }
            }

            FluentButton {
                theme: pageRoot.theme
                text: "刷新权限状态"
                onClicked: pageRoot.backend.refreshPrivileges()
            }
        }

        // Privilege Cards Row (2 columns)
        RowLayout {
            width: parent.width - 48
            spacing: 12

            FluentCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 100
                theme: pageRoot.theme
                cardPadding: 16
                borderColor: pageRoot.backend.isAdmin ? pageRoot.theme.successBd : pageRoot.theme.border

                Column {
                    width: parent.width
                    spacing: 10

                    RowLayout {
                        width: parent.width
                        Text {
                            Layout.fillWidth: true
                            text: "管理员权限 (UAC)"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            color: pageRoot.theme.textPrimary
                        }
                        StatusBadge {
                            theme: pageRoot.theme
                            label: pageRoot.backend.isAdmin ? "已提升" : "标准权限"
                            tone: pageRoot.backend.isAdmin ? 1 : 2
                        }
                    }

                    Text {
                        width: parent.width
                        text: pageRoot.backend.isAdmin
                              ? "当前进程拥有完整管理员令牌，可自由创建所有类型的目录符号链接、文件符号链接、联接与硬链接。"
                              : "尚未通过 UAC 提权。若未开启开发者模式，创建 /D 或 FILE 符号链接时需要请求提升权限。"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 13
                        color: pageRoot.theme.textSecondary
                        wrapMode: Text.WordWrap
                    }

                    FluentButton {
                        visible: !pageRoot.backend.isAdmin
                        theme: pageRoot.theme
                        primary: true
                        text: "以管理员身份重启"
                        iconPath: pageRoot.theme.icShield
                        onClicked: pageRoot.backend.relaunchAdmin()
                    }
                }
            }

            FluentCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 100
                theme: pageRoot.theme
                cardPadding: 16
                borderColor: pageRoot.backend.isDevMode ? pageRoot.theme.successBd : pageRoot.theme.border

                Column {
                    width: parent.width
                    spacing: 10

                    RowLayout {
                        width: parent.width
                        Text {
                            Layout.fillWidth: true
                            text: "Windows 开发者模式"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            color: pageRoot.theme.textPrimary
                        }
                        StatusBadge {
                            theme: pageRoot.theme
                            label: pageRoot.backend.isDevMode ? "已开启" : "未开启"
                            tone: pageRoot.backend.isDevMode ? 1 : 0
                        }
                    }

                    Text {
                        width: parent.width
                        text: pageRoot.backend.isDevMode
                              ? "系统已启用开发者模式！Windows 10/11 将直接允许普通权限用户免 UAC 创建符号链接。"
                              : "开启 Windows「开发人员模式」后，日常使用 /D 或文件符号链接无需每次触发 UAC 提权弹窗。"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 13
                        color: pageRoot.theme.textSecondary
                        wrapMode: Text.WordWrap
                    }

                    FluentButton {
                        theme: pageRoot.theme
                        text: "打开系统开发者设置"
                        onClicked: pageRoot.backend.openDeveloperSettings()
                    }
                }
            }
        }

        // NTFS Matrix Title
        Text {
            text: "NTFS 四种链接特性对比"
            font.family: "Microsoft YaHei UI"
            font.pixelSize: 14
            font.weight: Font.Bold
            color: pageRoot.theme.textPrimary
        }

        GridLayout {
            width: parent.width - 48
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            Repeater {
                model: pageRoot.matrixData

                FluentCard {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 100
                    theme: pageRoot.theme
                    cardPadding: 15

                    Column {
                        width: parent.width
                        spacing: 8

                        RowLayout {
                            width: parent.width
                            spacing: 8
                            StatusBadge {
                                theme: pageRoot.theme
                                label: modelData.code
                                tone: modelData.badgeTone
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: pageRoot.theme.textPrimary
                            }
                        }

                        Text {
                            width: parent.width
                            text: modelData.points
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 13
                            color: pageRoot.theme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        // Safety Principles Card
        FluentCard {
            width: parent.width - 48
            theme: pageRoot.theme
            cardPadding: 16

            Column {
                width: parent.width
                spacing: 8

                Text {
                    text: "安全操作准则与“安全拆解”机制"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: pageRoot.theme.textPrimary
                }

                Text {
                    width: parent.width
                    text: "1. 严禁直接对正在迁移或系统核心目录盲目删改：使用「搬家并链接」时，程序会先完整拷贝并校验后重命名回滚点，确认无误再建立链接，失败自动回退。\n" +
                          "2. 使用「安全拆解」移除链接时，程序底层只移除重解析点（Reparse Point）本身或单一硬链接名，绝对不会递归删除真实目标文件夹内部的数据。\n" +
                          "3. 支持快捷键：Ctrl+1~4 切换工作台页面，Esc 关闭弹窗。"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 13
                    lineHeight: 1.45
                    color: pageRoot.theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
