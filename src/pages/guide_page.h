#pragma once

#include <algorithm>
#include <string>

#include "app_actions.h"
#include "core/dsl.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

namespace fl {

struct GuideMatrixItem {
    const char* badge;
    const char* name;
    const char* privilege;
    int privTone;
    const char* features;
    const char* usage;
};

inline void DrawEnvTile(eui::Ui& ui,
                        const std::string& id,
                        const ui_kit::UiPalette& p,
                        float width,
                        const std::string& title,
                        const std::string& badgeText,
                        const ui_kit::SemanticColor& sem,
                        const std::string& desc,
                        const std::string& btnLabel,
                        uint32_t btnIcon,
                        bool btnDisabled,
                        const std::function<void()>& onAction) {
    ui.stack(id)
        .size(width, 136.0f)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, 136.0f)
                .radius(12.0f)
                .color(p.cardAltBg)
                .build();

            const float cw = std::max(80.0f, width - 32.0f);
            ui.column(id + ".col")
                .position(16.0f, 14.0f)
                .size(cw, 108.0f)
                .gap(8.0f)
                .content([&] {
                    ui.row(id + ".top")
                        .size(cw, 26.0f)
                        .gap(8.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            const float bW = 92.0f;
                            ui.text(id + ".title")
                                .size(std::max(40.0f, cw - bW - 8.0f), 26.0f)
                                .text(title)
                                .fontSize(14.5f)
                                .fontWeight(700)
                                .color(p.ink)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            ui_kit::StatusBadge(ui, id + ".badge", p, bW, 24.0f, badgeText, 0, sem, 12.0f);
                        })
                        .build();

                    ui.text(id + ".desc")
                        .size(cw, 36.0f)
                        .text(desc)
                        .fontSize(12.5f)
                        .lineHeight(18.0f)
                        .color(p.secondaryText)
                        .wrap(true)
                        .build();

                    ui_kit::ActionButton(
                        ui, id + ".btn", p, 148.0f, 32.0f,
                        btnLabel, btnIcon,
                        btnDisabled ? ui_kit::ButtonKind::Ghost : ui_kit::ButtonKind::Secondary,
                        onAction, btnDisabled, 12.5f);
                })
                .build();
        })
        .build();
}

struct GuideMatrixCols {
    float padX = 10.0f;
    float rowW = 0.0f;
    float typeW = 148.0f;
    float privW = 128.0f;
    float featW = 0.0f;
    float useW = 0.0f;

    static GuideMatrixCols compute(float cardW) {
        GuideMatrixCols c;
        c.rowW = std::max(280.0f, cardW - c.padX * 2.0f);
        const float rem = std::max(180.0f, c.rowW - c.typeW - c.privW - 36.0f);
        c.featW = std::floor(rem * 0.56f);
        c.useW = std::max(80.0f, rem - c.featW);
        return c;
    }
};

inline void DrawMatrixTableRow(eui::Ui& ui,
                               const std::string& id,
                               const ui_kit::UiPalette& p,
                               const GuideMatrixCols& cols,
                               const GuideMatrixItem& item) {
    ui.row(id)
        .position(cols.padX, 0.0f)
        .size(cols.rowW, 58.0f)
        .gap(12.0f)
        .alignItems(eui::Align::CENTER)
        .content([&] {
            ui.row(id + ".c1")
                .size(cols.typeW, 44.0f)
                .gap(8.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.stack(id + ".code")
                        .size(38.0f, 24.0f)
                        .content([&] {
                            ui.rect(id + ".codeBg")
                                .size(38.0f, 24.0f)
                                .radius(6.0f)
                                .color(p.accentSoft)
                                .build();
                            ui.text(id + ".codeTxt")
                                .size(38.0f, 24.0f)
                                .text(item.badge)
                                .fontFamily("monospace")
                                .fontSize(11.5f)
                                .fontWeight(700)
                                .color(p.accent)
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();
                        })
                        .build();

                    ui.text(id + ".name")
                        .size(cols.typeW - 46.0f, 24.0f)
                        .text(item.name)
                        .fontSize(13.5f)
                        .fontWeight(650)
                        .color(p.ink)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                })
                .build();

            ui.stack(id + ".c2")
                .size(cols.privW, 44.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui_kit::StatusBadge(
                        ui, id + ".priv", p, 120.0f, 25.0f,
                        item.privilege, 0, ui_kit::ToneColors(p, item.privTone), 11.5f);
                })
                .build();

            ui.text(id + ".feat")
                .size(cols.featW, 44.0f)
                .text(item.features)
                .fontSize(12.5f)
                .lineHeight(18.0f)
                .color(p.secondaryText)
                .wrap(true)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();

            ui.text(id + ".use")
                .size(cols.useW, 44.0f)
                .text(item.usage)
                .fontSize(12.5f)
                .lineHeight(18.0f)
                .color(p.mutedText)
                .wrap(true)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();
}

inline void ComposeGuidePage(eui::Ui& ui,
                             AppState& state,
                             const ui_kit::UiPalette& p,
                             float viewW,
                             float viewH) {
    ui_kit::CenteredScrollPage(
        ui, "guide.scroll", p, viewW, viewH, 840.0f, 32.0f,
        state.guideScrollOffset, "guide",
        [&](eui::Ui& innerUi, float studioW) {
            ui_kit::PageHeader(
                innerUi, "guide.pageHdr", studioW,
                "环境诊断与使用指南",
                "查看系统级权限就绪度，快速对照四种 NTFS 链接机制与安全边界",
                p, 108.0f, [&] {
                    ui_kit::ActionButton(
                        innerUi, "guide.refreshBtn", p, 104.0f, 36.0f,
                        "刷新状态", 0xF021u, ui_kit::ButtonKind::Secondary,
                        [&] {
                            actions::RefreshPrivileges(state);
                            ui_kit::TriggerToast(state, "已刷新系统状态", "权限与开发者模式状态已更新。", 1, 0xF058u);
                        },
                        false, 12.5f);
                });

            ui_kit::SectionCard(innerUi, "guide.envCard", p, studioW, 22.0f, 14.0f, [&](float cardW) {
                ui_kit::SectionTitle(
                    innerUi, "guide.envTitle", p, cardW,
                    "系统运行权限",
                    "日常使用目录联接 /J 与文件硬链 /H 完全免提权");

                const float tileW = std::max(220.0f, (cardW - 14.0f) * 0.5f);
                innerUi.row("guide.envRow")
                    .size(cardW, 136.0f)
                    .gap(14.0f)
                    .content([&] {
                        DrawEnvTile(
                            innerUi, "guide.adminTile", p, tileW,
                            "管理员权限",
                            state.isAdmin ? "已提升" : "标准权限",
                            state.isAdmin ? p.success : p.warning,
                            state.isAdmin
                                ? "当前已拥有最高管理员令牌，可直接创建任意跨系统级符号链接。"
                                : "若未开启开发者模式，创建 /D 或文件软链时可点下方按需提权。",
                            state.isAdmin ? "当前无需重启" : "以管理员重启",
                            0xF3EDu, state.isAdmin,
                            [&] {
                                if (!win::RelaunchAsAdmin()) {
                                    ui_kit::TriggerToast(state, "未获授权", "用户取消了管理员 UAC 确认。", 2, 0xF071u);
                                }
                            });

                        DrawEnvTile(
                            innerUi, "guide.devTile", p, tileW,
                            "开发者模式",
                            state.isDevMode ? "已开启" : "未开启",
                            state.isDevMode ? p.info : ui_kit::SemanticColor{p.mutedText, p.pillBg, p.hairline},
                            state.isDevMode
                                ? "Windows 允许非管理员进程直接创建 /D 与单文件符号链接。"
                                : "推荐前往系统设置开启，开启后创建所有链接均无需 UAC 弹窗。",
                            "打开系统设置",
                            0xF013u, false,
                            [&] { win::OpenWindowsDeveloperSettings(); });
                    })
                    .build();
            });

            ui_kit::SectionCard(innerUi, "guide.matrixCard", p, studioW, 22.0f, 10.0f, [&](float cardW) {
                ui_kit::SectionTitle(
                    innerUi, "guide.matrixTitle", p, cardW,
                    "NTFS 链接选型对比",
                    "85% 场景推荐首选 /J 目录联接");

                const auto cols = GuideMatrixCols::compute(cardW);

                innerUi.stack("guide.tblHdrBgWrap")
                    .size(cardW, 34.0f)
                    .content([&] {
                        innerUi.rect("guide.tblHdrBg")
                            .size(cardW, 34.0f)
                            .radius(9.0f)
                            .color(p.cardAltBg)
                            .build();

                        innerUi.row("guide.tblHdr")
                            .position(cols.padX, 0.0f)
                            .size(cols.rowW, 34.0f)
                            .gap(12.0f)
                            .alignItems(eui::Align::CENTER)
                            .content([&] {
                                innerUi.text("guide.th1").size(cols.typeW, 34.0f).text("链接类型")
                                    .fontSize(12.0f).fontWeight(650).color(p.mutedText)
                                    .verticalAlign(eui::VerticalAlign::Center).build();
                                innerUi.text("guide.th2").size(cols.privW, 34.0f).text("权限要求")
                                    .fontSize(12.0f).fontWeight(650).color(p.mutedText)
                                    .verticalAlign(eui::VerticalAlign::Center).build();
                                innerUi.text("guide.th3").size(cols.featW, 34.0f).text("机制与边界")
                                    .fontSize(12.0f).fontWeight(650).color(p.mutedText)
                                    .verticalAlign(eui::VerticalAlign::Center).build();
                                innerUi.text("guide.th4").size(cols.useW, 34.0f).text("推荐场景")
                                    .fontSize(12.0f).fontWeight(650).color(p.mutedText)
                                    .verticalAlign(eui::VerticalAlign::Center).build();
                            })
                            .build();
                    })
                    .build();

                static constexpr GuideMatrixItem kMatrix[] = {
                    { "/J",   "目录联接", "免提权推荐", 1,
                      "支持本地跨盘跳转；不支持 \\\\NAS 共享与相对路径。",
                      "软件/游戏搬家、Docker/Node 缓存转移" },
                    { "/D",   "符号链接", "开发者或管理", 2,
                      "全功能目录指针，支持相对路径与远程共享，兼容性最高。",
                      "工程复用、跨机共享挂载、脚本路径引用" },
                    { "/H",   "文件硬链", "同卷免提权", 1,
                      "仅限同一 NTFS 盘符，删原文件仍完整访问底层数据。",
                      "视频素材多库共用、镜像/备份去重" },
                    { "File", "文件软链", "开发者或管理", 2,
                      "支持跨盘单文件、相对路径或网络路径灵活映射。",
                      "存档/配置文件跨盘与网盘实时同步" },
                };

                for (int i = 0; i < 4; ++i) {
                    DrawMatrixTableRow(innerUi, "guide.mRow." + std::to_string(i), p, cols, kMatrix[i]);
                    if (i < 3) {
                        ui_kit::Hairline(innerUi, "guide.mDiv." + std::to_string(i), p, cardW);
                    }
                }
            });

            ui_kit::SectionCard(innerUi, "guide.faqCard", p, studioW, 22.0f, 12.0f, [&](float cardW) {
                ui_kit::SectionTitle(innerUi, "guide.faqTitle", p, cardW, "常见问题与安全守则");

                struct FaqItem {
                    const char* q;
                    const char* a;
                };
                static constexpr FaqItem kFaqs[] = {
                    { "为什么点击“目录搬家并建链”有时会被前置校验拦截？",
                      "说明源目录中存在正在运行或被后台进程锁定的文件。关闭对应应用、IDE 或云盘同步客户端后即可一键迁移。" },
                    { "怎样安全删掉一个符号链接而不伤及真实目录内的数据？",
                      "千万不要手动拖到回收站或盲目强制删目录。请在左侧进入“体检与扫描”，点击“安全解除链接”，程序会仅解除挂载入口本身。" },
                    { "硬链接 /H 为什么不能跨盘符创建？",
                      "硬链接直接指向同一分区底层的 MFT 文件记录号，这是 NTFS 文件系统规格限制。如需跨盘，请使用“文件软链”。" },
                };

                for (int i = 0; i < 3; ++i) {
                    const std::string fid = "guide.faq." + std::to_string(i);
                    innerUi.column(fid)
                        .width(cardW)
                        .gap(4.0f)
                        .content([&] {
                            innerUi.text(fid + ".q")
                                .size(cardW, 22.0f)
                                .text(std::string("0") + std::to_string(i + 1) + " · " + kFaqs[i].q)
                                .fontSize(13.5f)
                                .fontWeight(650)
                                .color(p.ink)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            innerUi.text(fid + ".a")
                                .size(cardW, 42.0f)
                                .text(kFaqs[i].a)
                                .fontSize(12.5f)
                                .lineHeight(18.5f)
                                .color(p.secondaryText)
                                .wrap(true)
                                .build();
                        })
                        .build();

                    if (i < 2) {
                        ui_kit::Hairline(innerUi, fid + ".div", p, cardW);
                    }
                }
            });
        });
}

} // namespace fl
