#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "app_actions.h"
#include "components/datatable.h"
#include "components/input.h"
#include "components/switch.h"
#include "core/dsl.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

namespace fl {

inline void DrawInspectedDetailCard(eui::Ui& ui,
                                    const std::string& idPrefix,
                                    AppState& state,
                                    const ui_kit::UiPalette& p,
                                    float cardW,
                                    const std::string& title,
                                    const InspectedLink& info) {
    const bool isUnlinkable = info.exists &&
                              info.kindCode != "NORMAL" &&
                              info.kindCode != "MISSING" &&
                              info.kindCode != "REPARSE";
    const int healthTone = !info.exists ? 2 : (!isUnlinkable ? 0 : (info.targetExists ? 1 : 3));
    const std::string healthLabel = !info.exists
        ? "路径不存在"
        : (!isUnlinkable ? "普通路径" : (info.targetExists ? "目标有效" : "目标缺失"));

    ui_kit::SectionCard(ui, idPrefix, p, cardW, 20.0f, 14.0f, [&](float innerW) {
        ui.row(idPrefix + ".hdr")
            .size(innerW, 28.0f)
            .gap(10.0f)
            .alignItems(eui::Align::CENTER)
            .content([&] {
                const float badgeW1 = 152.0f;
                const float badgeW2 = 128.0f;
                const float titleW = std::max(80.0f, innerW - badgeW1 - badgeW2 - 20.0f);

                ui.text(idPrefix + ".title")
                    .size(titleW, 28.0f)
                    .text(title)
                    .fontSize(15.0f)
                    .fontWeight(700)
                    .color(p.ink)
                    .verticalAlign(eui::VerticalAlign::Center)
                    .build();

                ui_kit::StatusBadge(
                    ui, idPrefix + ".kindBadge", p, badgeW1, 26.0f,
                    info.kindLabel, 0,
                    isUnlinkable ? p.info : ui_kit::SemanticColor{p.secondaryText, p.pillBg, p.hairline});

                ui_kit::StatusBadge(
                    ui, idPrefix + ".healthBadge", p, badgeW2, 26.0f,
                    healthLabel, 0,
                    ui_kit::ToneColors(p, healthTone));
            })
            .build();

        ui_kit::Hairline(ui, idPrefix + ".div", p, innerW);

        const std::string targetDisplay = !info.targetPath.empty()
            ? info.targetPath
            : (info.kindCode == "/H"
                ? ("共享同一份磁盘文件实体（" + std::to_string(info.hardLinkCount) + " 个硬链接入口）")
                : "当前非重定向链接，无指向目标");
        const std::string detailDisplay = info.detailNote.empty()
            ? ("底层标识: " + info.kindCode)
            : info.detailNote;

        ui_kit::MetaWell(ui, idPrefix + ".metaWell", p, innerW, {
            {"检查路径", info.path, true, true},
            {"真实目标", targetDisplay, !info.targetPath.empty(), !info.targetPath.empty()},
            {"底层详情", detailDisplay, false, false},
        });

        ui.row(idPrefix + ".actions")
            .size(innerW, 38.0f)
            .gap(8.0f)
            .alignItems(eui::Align::CENTER)
            .content([&] {
                ui_kit::ActionButton(
                    ui, idPrefix + ".openLink", p, 122.0f, 36.0f,
                    "定位入口", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                    [&] { win::RevealInExplorer(info.path); },
                    !info.exists, 13.0f);

                ui_kit::ActionButton(
                    ui, idPrefix + ".openTarget", p, 122.0f, 36.0f,
                    "定位目标", 0xF08Eu, ui_kit::ButtonKind::Secondary,
                    [&] { win::RevealInExplorer(info.targetPath); },
                    info.targetPath.empty(), 13.0f);

                ui_kit::ActionButton(
                    ui, idPrefix + ".copyTarget", p, 114.0f, 36.0f,
                    "复制路径", 0xF0C5u, ui_kit::ButtonKind::Ghost,
                    [&] {
                        const std::string val = info.targetPath.empty() ? info.path : info.targetPath;
                        if (win::CopyTextToClipboard(val)) {
                            ui_kit::TriggerToast(state, "已复制路径", val, 1, 0xF0C5u);
                        }
                    },
                    false, 13.0f);

                const float spacerW = std::max(8.0f, innerW - 122.0f - 122.0f - 114.0f - 142.0f - 32.0f);
                ui.rect(idPrefix + ".sp").size(spacerW, 1.0f).color(ui_kit::kTransparent).build();

                ui_kit::ActionButton(
                    ui, idPrefix + ".unlinkBtn", p, 142.0f, 36.0f,
                    "安全解除链接", 0xF127u, ui_kit::ButtonKind::Danger,
                    [&] { actions::RequestUnlinkItem(state, info); },
                    !isUnlinkable, 13.0f);
            })
            .build();
    });
}

inline void ComposeInspectPage(eui::Ui& ui,
                               AppState& state,
                               const ui_kit::UiPalette& p,
                               float viewW,
                               float viewH) {
    const bool scanning = app::async::running("fl.scan");
    const std::string tableKey = std::to_string(state.inspectTableRevision) + "_" +
                                 std::to_string(state.selectedInspectIndex) + "_" +
                                 (state.hasInspectedSingle ? "1" : "0");

    ui_kit::CenteredScrollPage(
        ui, "inspect.scroll", p, viewW, viewH, 880.0f, 32.0f,
        state.inspectScrollOffset, tableKey,
        [&](eui::Ui& innerUi, float studioW) {
            ui_kit::PageHeader(
                innerUi, "inspect.pageHdr", studioW,
                "链接体检与扫描",
                "识别 NTFS 符号链接、目录联接与硬链接，追踪真实目标或安全解除绑定",
                p);

            ui_kit::SectionCard(innerUi, "inspect.queryCard", p, studioW, 20.0f, 14.0f, [&](float cardW) {
                const float dirW = 76.0f;
                const float fileW = 76.0f;
                const float checkW = 104.0f;
                const float scanW = 136.0f;
                const float rowGap = 8.0f;
                const float inputW = std::max(120.0f, cardW - dirW - fileW - checkW - scanW - rowGap * 4.0f);

                innerUi.row("inspect.inputRow")
                    .size(cardW, 44.0f)
                    .gap(rowGap)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        components::input(innerUi, "inspect.queryInput")
                            .theme(p.tokens)
                            .size(inputW, 44.0f)
                            .fontSize(14.0f)
                            .value(state.inspectQueryPath)
                            .placeholder("输入路径进行单路径体检，或递归扫描文件夹...")
                            .onChange([&](const std::string& v) { state.inspectQueryPath = v; })
                            .onEnter([&] { actions::InspectPathNow(state, state.inspectQueryPath); })
                            .build();

                        ui_kit::ActionButton(
                            innerUi, "inspect.pickDir", p, dirW, 44.0f,
                            "文件夹", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                            [&] {
                                const auto chosen = win::BrowseDirectory("选择要体检或扫描的目录", state.inspectQueryPath);
                                if (!chosen.empty()) {
                                    actions::InspectPathNow(state, chosen);
                                }
                            },
                            false, 13.0f);

                        ui_kit::ActionButton(
                            innerUi, "inspect.pickFile", p, fileW, 44.0f,
                            "文件", 0xF15Bu, ui_kit::ButtonKind::Secondary,
                            [&] {
                                const auto chosen = win::BrowseFile("选择要检查的文件", state.inspectQueryPath);
                                if (!chosen.empty()) {
                                    actions::InspectPathNow(state, chosen);
                                }
                            },
                            false, 13.0f);

                        ui_kit::ActionButton(
                            innerUi, "inspect.analyzeBtn", p, checkW, 44.0f,
                            "单路径体检", 0xF002u, ui_kit::ButtonKind::InfoAccent,
                            [&] { actions::InspectPathNow(state, state.inspectQueryPath); },
                            false, 13.0f);

                        if (scanning) {
                            ui_kit::ActionButton(
                                innerUi, "inspect.stopBtn", p, scanW, 44.0f,
                                "停止扫描", 0xF04Du, ui_kit::ButtonKind::Danger,
                                [&] {
                                    app::async::cancel("fl.scan");
                                    state.inspectStatusText = "已停止扫描";
                                },
                                false, 13.5f);
                        } else {
                            ui_kit::HeroButton(
                                innerUi, "inspect.scanBtn", p, scanW, 44.0f,
                                "扫描目录", 0xF0B0u,
                                [&] { actions::StartDirectoryScan(state); },
                                false, 13.5f);
                        }
                    })
                    .build();

                innerUi.row("inspect.optRow")
                    .size(cardW, 32.0f)
                    .gap(12.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        components::toggleSwitch(innerUi, "inspect.hardSwitch")
                            .theme(p.tokens)
                            .size(124.0f, 30.0f)
                            .checked(state.inspectIncludeHardLinks)
                            .text("包含硬链接")
                            .fontSize(12.5f)
                            .transition(p.anim)
                            .onChange([&](bool val) {
                                state.inspectIncludeHardLinks = val;
                                actions::PersistSettings(state);
                            })
                            .build();

                        const float depthGroupW = 248.0f;
                        innerUi.stack("inspect.depthTray")
                            .size(depthGroupW, 30.0f)
                            .content([&] {
                                innerUi.rect("inspect.depthTrayBg")
                                    .size(depthGroupW, 30.0f)
                                    .radius(9.0f)
                                    .color(p.cardAltBg)
                                    .build();

                                innerUi.row("inspect.depthRow")
                                    .position(8.0f, 3.0f)
                                    .size(depthGroupW - 12.0f, 24.0f)
                                    .gap(4.0f)
                                    .alignItems(eui::Align::CENTER)
                                    .content([&] {
                                        innerUi.text("inspect.depthLbl")
                                            .size(56.0f, 24.0f)
                                            .text("扫描深度")
                                            .fontSize(12.0f)
                                            .color(p.mutedText)
                                            .verticalAlign(eui::VerticalAlign::Center)
                                            .build();

                                        static constexpr int kDepths[] = {2, 4, 6, 8};
                                        for (int d : kDepths) {
                                            const bool sel = (state.inspectMaxDepth == d);
                                            const std::string did = "inspect.d" + std::to_string(d);
                                            innerUi.stack(did)
                                                .size(40.0f, 24.0f)
                                                .visualStateFrom(did, 0.96f)
                                                .transition(p.anim)
                                                .onClick([&state, d] {
                                                    state.inspectMaxDepth = d;
                                                    actions::PersistSettings(state);
                                                })
                                                .content([&] {
                                                    innerUi.rect(did + ".bg")
                                                        .size(40.0f, 24.0f)
                                                        .radius(7.0f)
                                                        .color(sel ? p.cardBg : ui_kit::kTransparent)
                                                        .hoverColor(sel ? p.cardBg : p.navHover)
                                                        .border(sel ? 1.0f : 0.0f,
                                                                sel ? p.hairlineStrong : ui_kit::kTransparent)
                                                        .visualStateFrom(did, 1.0f)
                                                        .transition(p.anim)
                                                        .animate(eui::AnimProperty::Color)
                                                        .build();
                                                    innerUi.text(did + ".txt")
                                                        .size(40.0f, 24.0f)
                                                        .text(std::to_string(d) + "层")
                                                        .fontSize(12.0f)
                                                        .fontWeight(sel ? 650 : 500)
                                                        .color(sel ? p.accent : p.secondaryText)
                                                        .horizontalAlign(eui::HorizontalAlign::Center)
                                                        .verticalAlign(eui::VerticalAlign::Center)
                                                        .build();
                                                })
                                                .build();
                                        }
                                    })
                                    .build();
                            })
                            .build();

                        const float statusW = std::max(60.0f, cardW - 124.0f - depthGroupW - 24.0f);
                        innerUi.text("inspect.statusText")
                            .size(statusW, 30.0f)
                            .text(state.inspectStatusText)
                            .fontSize(12.5f)
                            .color(p.mutedText)
                            .horizontalAlign(eui::HorizontalAlign::Right)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();
                    })
                    .build();
            });

            if (!state.hasInspectedSingle && state.scannedLinks.empty()) {
                ui_kit::SectionCard(innerUi, "inspect.emptyCard", p, studioW, 36.0f, 14.0f, [&](float cardW) {
                    innerUi.column("inspect.emptyCol")
                        .width(cardW)
                        .gap(10.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            innerUi.stack("inspect.emptyIcon")
                                .size(52.0f, 52.0f)
                                .content([&] {
                                    innerUi.rect("inspect.emptyIconBg")
                                        .size(52.0f, 52.0f)
                                        .radius(26.0f)
                                        .color(p.accentSoft)
                                        .build();
                                    innerUi.text("inspect.emptyIconIc")
                                        .size(52.0f, 52.0f)
                                        .icon(0xF002u)
                                        .fontSize(20.0f)
                                        .color(p.accent)
                                        .horizontalAlign(eui::HorizontalAlign::Center)
                                        .verticalAlign(eui::VerticalAlign::Center)
                                        .build();
                                })
                                .build();

                            innerUi.text("inspect.emptyTitle")
                                .size(cardW, 24.0f)
                                .text("输入或选择路径开始体检")
                                .fontSize(16.0f)
                                .fontWeight(700)
                                .color(p.ink)
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            innerUi.text("inspect.emptyHint")
                                .size(std::min(cardW, 540.0f), 22.0f)
                                .text("可检查单个文件或文件夹的底层链接属性，也可递归扫描目录下所有隐含的重定向入口。")
                                .fontSize(13.0f)
                                .color(p.mutedText)
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            if (!state.linkPath.empty()) {
                                ui_kit::ActionButton(
                                    innerUi, "inspect.importCurrentBtn", p, 226.0f, 38.0f,
                                    "体检“新建与迁移”当前入口", 0xF061u, ui_kit::ButtonKind::InfoAccent,
                                    [&] { actions::InspectPathNow(state, state.linkPath); },
                                    false, 13.0f);
                            }
                        })
                        .build();
                });
            }

            if (state.hasInspectedSingle) {
                DrawInspectedDetailCard(
                    innerUi, "inspect.single", state, p, studioW,
                    "单路径检查结果", state.inspectedSingle);
            }

            if (!state.scannedLinks.empty()) {
                ui_kit::SectionCard(innerUi, "inspect.tableCard", p, studioW, 20.0f, 14.0f, [&](float cardW) {
                    ui_kit::SectionTitle(
                        innerUi, "inspect.listTitle", p, cardW,
                        "扫描结果列表",
                        "共 " + std::to_string(state.scannedLinks.size()) + " 项 · 点击行查看详情与安全解链");

                    const float col0 = 78.0f;
                    const float col1 = 96.0f;
                    const float rem = std::max(180.0f, cardW - col0 - col1 - 2.0f);
                    const float col2 = std::floor(rem * 0.48f);
                    const float col3 = std::max(90.0f, rem - col2);

                    components::dataTable(innerUi, "inspect.table")
                        .theme(p.tokens)
                        .size(cardW, 264.0f)
                        .columns({"类型", "状态", "链接入口", "指向目标"})
                        .columnWidths({col0, col1, col2, col3})
                        .rowHeight(40.0f)
                        .data(state.inspectTableRows, state.inspectTableRevision)
                        .sortable(true)
                        .resizable(true)
                        .onRowClick([&](std::size_t row) {
                            if (row < state.scannedLinks.size()) {
                                state.selectedInspectIndex = static_cast<int>(row);
                            }
                        })
                        .build();
                });

                if (state.selectedInspectIndex >= 0 &&
                    state.selectedInspectIndex < static_cast<int>(state.scannedLinks.size())) {
                    DrawInspectedDetailCard(
                        innerUi, "inspect.selected", state, p, studioW,
                        "已选中链接详情",
                        state.scannedLinks[static_cast<size_t>(state.selectedInspectIndex)]);
                }
            }
        });
}

} // namespace fl
