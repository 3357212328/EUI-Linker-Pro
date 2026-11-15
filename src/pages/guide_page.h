#pragma once

#include "app_actions.h"
#include "eui_neo.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace fl::pages {

namespace detail {

struct GuideFaqCard {
    unsigned int icon;
    const char* title;
    const char* badge;
    int tone;
    const char* body;
};

inline void DrawMatrixRow(eui::Ui& ui,
                          const std::string& id,
                          const ui_kit::UiPalette& p,
                          float width,
                          bool isHeader,
                          const std::string& c0,
                          const std::string& c1,
                          const std::string& c2,
                          const std::string& c3,
                          const std::string& c4) {
    const float innerW = std::max(80.0f, width - 24.0f);
    const float w0 = 152.0f;
    const float w1 = 92.0f;
    const float w2 = 126.0f;
    const float w3 = 142.0f;
    const float w4 = std::max(120.0f, innerW - w0 - w1 - w2 - w3 - 32.0f);

    components::card(ui, id)
        .theme(p.tokens)
        .width(width)
        .wrapContentHeight()
        .padding(12.0f)
        .color(isHeader ? p.tokens.surfaceHover : p.cardAltBg)
        .radius(8.0f)
        .border(1.0f, p.border)
        .shadow(ui_kit::NoShadow())
        .content([&] {
            ui.row(id + ".row")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .gap(8.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    auto cell = [&](const std::string& cid, float cw, const std::string& text, bool bold, const eui::Color& col) {
                        ui.text(cid)
                            .width(cw)
                            .height(eui::SizeValue::wrapContent())
                            .maxWidth(cw)
                            .wrap(true)
                            .text(text)
                            .fontSize(14.5f)
                            .lineHeight(20.5f)
                            .fontWeight(bold ? 600 : 400)
                            .color(col)
                            .build();
                    };
                    cell(id + ".0", w0, c0, true, isHeader ? p.text : p.accentText);
                    cell(id + ".1", w1, c1, isHeader, isHeader ? p.text : p.mutedText);
                    cell(id + ".2", w2, c2, isHeader, isHeader ? p.text : p.mutedText);
                    cell(id + ".3", w3, c3, isHeader, isHeader ? p.text : p.mutedText);
                    cell(id + ".4", w4, c4, isHeader, isHeader ? p.text : p.text);
                })
                .build();
        })
        .build();
}

} // namespace detail

inline void ComposeGuidePage(eui::Ui& ui,
                             AppState& state,
                             const ui_kit::UiPalette& p,
                             float width,
                             float height) {
    const std::string scrollKey =
        std::string("guide:") +
        std::to_string(state.isAdmin ? 1 : 0) + ":" +
        std::to_string(state.isDevMode ? 1 : 0);

    components::scrollView(ui, "page.guide.scroll")
        .theme(p.tokens)
        .size(width, height)
        .offset(state.guideScrollOffset)
        .step(64.0f)
        .gap(14.0f)
        .transition(0.08f, eui::Ease::OutCubic)
        .contentKey(scrollKey)
        .onChange([&](float offset) { state.guideScrollOffset = offset; })
        .content([&](eui::Ui& innerUi, float innerW, float /*viewportH*/) {
            innerUi.column("guide.col")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .gap(14.0f)
                .content([&] {
                    // 1. Environment & Privilege Helper Card
                    ui_kit::SectionCard(innerUi, "guide.envCard", p, innerW, 18.0f, 12.0f, [&](float cardW) {
                        ui_kit::SectionHeader(innerUi, "guide.envHdr", p, 0xF3EDu,
                                              "Windows 权限与环境诊断",
                                              "查看当前权限对 /D、文件符号链接的支持情况，并可一键开启开发者模式或提权重启");

                        auto drawStatusBadges = [&] {
                            ui_kit::StatusBadge(innerUi, "guide.adminStatus", p, 204.0f, 32.0f,
                                                state.isAdmin ? "已获取管理员权限" : "当前为标准用户权限",
                                                0xF3EDu,
                                                state.isAdmin ? p.success : p.warning);

                            ui_kit::StatusBadge(innerUi, "guide.devStatus", p, 222.0f, 32.0f,
                                                state.isDevMode ? "开发者模式：已开启" : "开发者模式：未开启",
                                                0xF013u,
                                                state.isDevMode ? p.success : p.info);
                        };

                        auto drawEnvButtons = [&] {
                            ui_kit::ActionButton(innerUi, "guide.devBtn", p, 168.0f, 38.0f,
                                                 "打开系统开发者设置", 0xF08Eu, ui_kit::ButtonKind::Secondary,
                                                 [&] {
                                                     win::OpenWindowsDeveloperSettings();
                                                     ui_kit::TriggerToast(state, "已打开设置", "请在系统设置中启用“开发人员模式”，之后点刷新状态即可生效。", 0, 0xF05Au);
                                                 },
                                                 false, 14.0f);

                            ui_kit::ActionButton(innerUi, "guide.refreshEnvBtn", p, 108.0f, 38.0f,
                                                 "刷新状态", 0xF021u, ui_kit::ButtonKind::Subtle,
                                                 [&] {
                                                     actions::RefreshPrivileges(state);
                                                     ui_kit::TriggerToast(state, "环境已刷新",
                                                                          state.isDevMode ? "检测到开发者模式已开启。" : "权限状态已更新。",
                                                                          0, 0xF021u);
                                                 },
                                                 false, 14.0f);

                            ui_kit::ActionButton(innerUi, "guide.elevateBtn", p, 160.0f, 38.0f,
                                                 state.isAdmin ? "已处管理员模式" : "以管理员身份重启",
                                                 0xF3EDu, ui_kit::ButtonKind::Primary,
                                                 [&] {
                                                     if (win::RelaunchAsAdmin()) {
                                                         std::exit(0);
                                                     } else {
                                                         ui_kit::TriggerToast(state, "未完成提权", "用户取消了 UAC 权限提示，当前维持原权限运行。", 2, 0xF071u);
                                                     }
                                                 },
                                                 state.isAdmin, 14.0f);
                        };

                        if (cardW >= 920.0f) {
                            innerUi.row("guide.envRow")
                                .width(cardW)
                                .height(38.0f)
                                .gap(10.0f)
                                .alignItems(eui::Align::CENTER)
                                .content([&] {
                                    drawStatusBadges();
                                    innerUi.row("guide.envSp").flexGrow(1.0f).height(1.0f).content([] {}).build();
                                    drawEnvButtons();
                                })
                                .build();
                        } else {
                            innerUi.row("guide.envBadgesRow")
                                .width(cardW)
                                .height(34.0f)
                                .gap(10.0f)
                                .alignItems(eui::Align::CENTER)
                                .content(drawStatusBadges)
                                .build();
                            innerUi.row("guide.envBtnsRow")
                                .width(cardW)
                                .height(38.0f)
                                .gap(10.0f)
                                .alignItems(eui::Align::CENTER)
                                .content(drawEnvButtons)
                                .build();
                        }
                    });

                    // 2. Link Types Comparison Matrix
                    ui_kit::SectionCard(innerUi, "guide.matrixCard", p, innerW, 18.0f, 10.0f, [&](float cardW) {
                        ui_kit::SectionHeader(innerUi, "guide.matrixHdr", p, 0xF02Du,
                                              "四种 Windows 链接机制技术对照表",
                                              "根据适用对象、跨盘需求和系统权限快速选型");

                        detail::DrawMatrixRow(innerUi, "guide.m.hdr", p, cardW, true,
                                              "类型 / 参数", "目标类型", "跨盘 / UNC", "创建权限", "核心特征与最佳应用场景");
                        detail::DrawMatrixRow(innerUi, "guide.m.j", p, cardW, false,
                                              "目录联接 (/J)", "仅文件夹", "本地跨盘\n不支持 UNC", "无需管理员",
                                              "C盘瘦身与游戏/工具搬家的首选方式，对几乎所有 Windows 软件完全透明兼容。");
                        detail::DrawMatrixRow(innerUi, "guide.m.d", p, cardW, false,
                                              "目录符号链接 (/D)", "仅文件夹", "跨盘 + 相对\n+ 网络 UNC", "管理员 或\n开发者模式",
                                              "支持相对路径（移动整组工程仍可用）以及局域网共享目录映射 (\\\\Server\\Share)。");
                        detail::DrawMatrixRow(innerUi, "guide.m.h", p, cardW, false,
                                              "文件硬链接 (/H)", "仅文件", "严格同分区\n(同盘符)", "无需管理员",
                                              "多个目录共享同一物理文件数据块；零性能损耗，删除任一入口均不影响其他入口。");
                        detail::DrawMatrixRow(innerUi, "guide.m.f", p, cardW, false,
                                              "文件符号链接", "仅文件", "跨盘 + 相对\n+ 网络 UNC", "管理员 或\n开发者模式",
                                              "当单文件需要跨磁盘分区链接或指向局域网文件时使用，类似操作系统级深度替身。");
                    });

                    // 3. Practical Best Practices Cards
                    const detail::GuideFaqCard cards[3] = {
                        {0xF079u, "安全迁移 C 盘大文件夹（目录搬家并建链）标准流程", "推荐实践", 1,
                         "1. 先完全退出占用目标目录的软件或游戏客户端，防止移动过程触发文件占用锁；\n"
                         "2. 在“创建与迁移”页面上方切换到「目录搬家并建链」，类型保持推荐的「/J 目录联接」；\n"
                         "3. 左侧选择当前 C 盘膨胀目录（如 AppData\\Local\\...），右侧选择 D 盘等大容量盘的目标新路径；\n"
                         "4. 软件将先调用 Windows IFileOperation 将数据完整迁移过去，再在原位置无感建立原名联接。"},
                        {0xF071u, "高危避坑：如何安全移除链接而不伤害原始真实数据", "重要警告", 3,
                         "切勿使用老旧命令行工具的递归清空指令直接清除链接内部文件——某些工具或脚本会顺着链接穿透进入真实目标目录误删源数据！\n"
                         "安全做法：始终进入本工具「链接体检与管理」标签页，选中链接并点击「安全解除此链接」。系统会底层调用 RemoveDirectoryW / DeleteFileW 仅卸载链接入口，目标文件夹内容绝对安全。"},
                        {0xF05Au, "如何辨别悬空失效链接与多入口硬链接文件", "深度原理", 0,
                         "当真实目标被手动移走、重命名或卸载外接硬盘时，原本指向它的符号链接或目录联接就会变成“悬空链接”（双击会报位置不可用）。\n"
                         "硬链接 (/H) 与之相反：它没有“主体与替身”的主从之分，多个路径均直接绑定同一个 NTFS 文件节点；只有当所有入口都被移除后磁盘空间才会真正释放。在「链接体检」中可一键查看任意文件全部兄弟硬链接。"}
                    };

                    for (int i = 0; i < 3; ++i) {
                        const auto& c = cards[i];
                        ui_kit::SectionCard(innerUi, "guide.faq." + std::to_string(i), p, innerW, 18.0f, 10.0f, [&](float cardW) {
                            innerUi.row("guide.faq." + std::to_string(i) + ".hdr")
                                .width(cardW)
                                .height(32.0f)
                                .gap(10.0f)
                                .alignItems(eui::Align::CENTER)
                                .content([&] {
                                    innerUi.text("guide.faq." + std::to_string(i) + ".icon")
                                        .icon(c.icon)
                                        .fontSize(17.5f)
                                        .lineHeight(21.0f)
                                        .color(ui_kit::ToneColors(p, c.tone).fg)
                                        .build();

                                    innerUi.text("guide.faq." + std::to_string(i) + ".title")
                                        .text(c.title)
                                        .fontSize(17.5f)
                                        .lineHeight(22.0f)
                                        .fontWeight(600)
                                        .color(p.text)
                                        .build();

                                    innerUi.row("guide.faq." + std::to_string(i) + ".sp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                                    ui_kit::StatusBadge(innerUi, "guide.faq." + std::to_string(i) + ".badge",
                                                        p, 96.0f, 28.0f, c.badge, 0, ui_kit::ToneColors(p, c.tone));
                                })
                                .build();

                            innerUi.text("guide.faq." + std::to_string(i) + ".body")
                                .width(cardW)
                                .height(eui::SizeValue::wrapContent())
                                .maxWidth(cardW)
                                .wrap(true)
                                .text(c.body)
                                .fontSize(15.0f)
                                .lineHeight(23.0f)
                                .color(p.mutedText)
                                .build();
                        });
                    }
                })
                .build();
        })
        .build();
}

} // namespace fl::pages
