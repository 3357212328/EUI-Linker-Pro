#pragma once

#include "app_actions.h"
#include "eui_neo.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

#include <algorithm>
#include <string>
#include <vector>

namespace fl::pages {

namespace detail {

inline void DrawInspectedDetailCard(eui::Ui& ui,
                                    const std::string& idPrefix,
                                    AppState& state,
                                    const ui_kit::UiPalette& p,
                                    float cardW,
                                    const InspectedLink& info,
                                    const std::string& title) {
    const bool isUnlinkable = info.exists &&
        (info.kindCode == "/D" || info.kindCode == "/J" || info.kindCode == "/H" || info.kindCode == "FILE");

    int healthTone = 0;
    unsigned int healthIcon = 0xF05Au;
    std::string healthText = "非链接实体";
    if (!info.exists) {
        healthTone = 3;
        healthIcon = 0xF057u;
        healthText = "路径不存在";
    } else if (isUnlinkable) {
        healthTone = info.targetExists ? 1 : 3;
        healthIcon = info.targetExists ? 0xF058u : 0xF071u;
        healthText = info.targetExists ? "目标有效" : "目标失联 (悬空)";
    } else if (info.kindCode == "REPARSE") {
        healthTone = 2;
        healthIcon = 0xF071u;
        healthText = "系统重解析点";
    }

    ui_kit::SectionCard(ui, idPrefix, p, cardW, 16.0f, 10.0f, [&](float innerW) {
        ui.row(idPrefix + ".hdr")
            .width(innerW)
            .height(28.0f)
            .gap(8.0f)
            .alignItems(eui::Align::CENTER)
            .content([&] {
                ui.text(idPrefix + ".title")
                    .text(title)
                    .fontSize(15.0f)
                    .lineHeight(20.0f)
                    .fontWeight(600)
                    .color(p.text)
                    .build();

                ui_kit::StatusBadge(ui, idPrefix + ".kindBadge", p, 142.0f, 24.0f,
                                    info.kindLabel, 0xF0C1u,
                                    isUnlinkable ? p.info : ui_kit::SemanticColor{p.mutedText, p.tokens.surfaceHover, p.border});

                ui.row(idPrefix + ".sp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                ui_kit::StatusBadge(ui, idPrefix + ".healthBadge", p, 128.0f, 24.0f,
                                    healthText, healthIcon,
                                    ui_kit::ToneColors(p, healthTone));
            })
            .build();

        ui.text(idPrefix + ".pathLbl")
            .width(innerW)
            .height(eui::SizeValue::wrapContent())
            .maxWidth(innerW)
            .wrap(true)
            .fontFamily("monospace")
            .fontSize(12.5f)
            .lineHeight(18.0f)
            .color(p.text)
            .text("入口路径: " + info.path)
            .build();

        if (!info.targetPath.empty()) {
            ui.text(idPrefix + ".targetLbl")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .maxWidth(innerW)
                .wrap(true)
                .fontFamily("monospace")
                .fontSize(12.5f)
                .lineHeight(18.0f)
                .color(info.targetExists ? p.accentText : p.danger.fg)
                .text((info.kindCode == "/H" ? "兄弟路径示例: " : "真实指向目标: ") + info.targetPath)
                .build();
        }

        if (info.kindCode == "/H" && info.hardLinkSiblings.size() > 1) {
            std::string sibList = "同一文件数据块关联的其余硬链接入口:\n";
            const std::size_t shown = std::min<std::size_t>(info.hardLinkSiblings.size(), 5);
            for (std::size_t i = 0; i < shown; ++i) {
                sibList += "  • " + info.hardLinkSiblings[i] + (i + 1 < shown ? "\n" : "");
            }
            if (info.hardLinkSiblings.size() > shown) {
                sibList += "\n  ...共 " + std::to_string(info.hardLinkSiblings.size()) + " 个兄弟路径";
            }
            ui.text(idPrefix + ".siblings")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .maxWidth(innerW)
                .wrap(true)
                .fontFamily("monospace")
                .fontSize(12.0f)
                .lineHeight(17.0f)
                .color(p.mutedText)
                .text(sibList)
                .build();
        }

        if (!info.detailNote.empty()) {
            ui.text(idPrefix + ".note")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .maxWidth(innerW)
                .wrap(true)
                .fontSize(12.5f)
                .lineHeight(17.0f)
                .color(p.mutedText)
                .text(info.detailNote)
                .build();
        }

        ui.row(idPrefix + ".actions")
            .width(innerW)
            .height(34.0f)
            .gap(8.0f)
            .content([&] {
                ui_kit::ActionButton(ui, idPrefix + ".openLink", p, 134.0f, 34.0f,
                                     "定位链接入口", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                                     [path = info.path] { win::RevealInExplorer(path); },
                                     !info.exists, 12.0f);

                ui_kit::ActionButton(ui, idPrefix + ".openTarget", p, 134.0f, 34.0f,
                                     "定位真实目标", 0xF08Eu, ui_kit::ButtonKind::Secondary,
                                     [target = info.targetPath] { win::RevealInExplorer(target); },
                                     info.targetPath.empty(), 12.0f);

                ui_kit::ActionButton(ui, idPrefix + ".copyTarget", p, 118.0f, 34.0f,
                                     "复制目标路径", 0xF0C5u, ui_kit::ButtonKind::Subtle,
                                     [&state, target = info.targetPath, path = info.path] {
                                         const std::string val = target.empty() ? path : target;
                                         if (win::CopyTextToClipboard(val)) {
                                             ui_kit::TriggerToast(state, "已复制路径", val, 1, 0xF0C5u);
                                         }
                                     },
                                     !info.exists, 12.0f);

                ui.row(idPrefix + ".actSp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                ui_kit::ActionButton(ui, idPrefix + ".unlinkBtn", p, 148.0f, 34.0f,
                                     "安全解除此链接", 0xF127u, ui_kit::ButtonKind::Danger,
                                     [&state, info] { actions::RequestUnlinkItem(state, info); },
                                     !isUnlinkable, 12.5f);
            })
            .build();
    });
}

} // namespace detail

inline void ComposeInspectPage(eui::Ui& ui,
                               AppState& state,
                               const ui_kit::UiPalette& p,
                               float width,
                               float height) {
    const bool scanning = app::async::running("fl.scan");

    components::scrollView(ui, "page.inspect.scroll")
        .theme(p.tokens)
        .size(width, height)
        .offset(state.inspectScrollOffset)
        .gap(14.0f)
        .onChange([&](float offset) { state.inspectScrollOffset = offset; })
        .content([&](eui::Ui& innerUi, float innerW, float /*viewportH*/) {
            ui.column("inspect.col")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .gap(14.0f)
                .content([&] {
                    // 1. Search / Query Toolbar Card
                    ui_kit::SectionCard(innerUi, "inspect.queryCard", p, innerW, 18.0f, 12.0f, [&](float cardW) {
                        ui_kit::SectionHeader(innerUi, "inspect.hdr", p, 0xF002u,
                                              "链接体检与目录扫描",
                                              "精准识别 /D /J /H 与文件软链、检测悬空目标，并安全剥离链接而不伤及实体文件");

                        const float actBtnW = 94.0f;
                        const float pickW = 74.0f;
                        const float gap = 6.0f;
                        const float inputW = std::max(140.0f, cardW - pickW * 2.0f - actBtnW * 2.0f - gap * 4.0f);

                        innerUi.row("inspect.inputRow")
                            .width(cardW)
                            .height(38.0f)
                            .gap(gap)
                            .alignItems(eui::Align::CENTER)
                            .content([&] {
                                components::input(innerUi, "inspect.pathInput")
                                    .theme(p.tokens)
                                    .size(inputW, 38.0f)
                                    .fontSize(13.5f)
                                    .value(state.inspectQueryPath)
                                    .placeholder("输入要体检的文件/链接路径，或选择文件夹扫描...")
                                    .onChange([&](const std::string& v) {
                                        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                                            state.inspectQueryPath = v.substr(1, v.size() - 2);
                                        } else {
                                            state.inspectQueryPath = v;
                                        }
                                    })
                                    .onEnter([&] { actions::InspectPathNow(state, state.inspectQueryPath); })
                                    .build();

                                ui_kit::ActionButton(innerUi, "inspect.pickDir", p, pickW, 38.0f,
                                                     "文件夹", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                                                     [&] {
                                                         auto dir = win::BrowseDirectory("选择要检查或扫描的文件夹", state.inspectQueryPath);
                                                         if (!dir.empty()) {
                                                             actions::InspectPathNow(state, dir);
                                                         }
                                                     },
                                                     false, 12.0f);

                                ui_kit::ActionButton(innerUi, "inspect.pickFile", p, pickW, 38.0f,
                                                     "选文件", 0xF15Bu, ui_kit::ButtonKind::Secondary,
                                                     [&] {
                                                         auto file = win::BrowseFile("选择要体检的文件或链接", state.inspectQueryPath);
                                                         if (!file.empty()) {
                                                             actions::InspectPathNow(state, file);
                                                         }
                                                     },
                                                     false, 12.0f);

                                ui_kit::ActionButton(innerUi, "inspect.analyzeBtn", p, actBtnW, 38.0f,
                                                     "单路径体检", 0xF002u, ui_kit::ButtonKind::InfoAccent,
                                                     [&] { actions::InspectPathNow(state, state.inspectQueryPath); },
                                                     false, 12.5f);

                                ui_kit::ActionButton(innerUi, "inspect.scanBtn", p, actBtnW, 38.0f,
                                                     scanning ? "停止扫描" : "扫描子目录",
                                                     scanning ? 0xF00Du : 0xF021u,
                                                     scanning ? ui_kit::ButtonKind::Danger : ui_kit::ButtonKind::Primary,
                                                     [&] {
                                                         if (scanning) {
                                                             app::async::cancel("fl.scan");
                                                             state.inspectStatusText = "已停止目录扫描。";
                                                         } else {
                                                             actions::StartDirectoryScan(state);
                                                         }
                                                     },
                                                     false, 12.5f);
                            })
                            .build();

                        innerUi.row("inspect.optRow")
                            .width(cardW)
                            .height(30.0f)
                            .gap(12.0f)
                            .alignItems(eui::Align::CENTER)
                            .content([&] {
                                components::toggleSwitch(innerUi, "inspect.incHard")
                                    .theme(p.tokens)
                                    .size(190.0f, 26.0f)
                                    .checked(state.inspectIncludeHardLinks)
                                    .text("检索普通硬链接文件 (/H)")
                                    .fontSize(12.5f)
                                    .transition(p.anim)
                                    .onChange([&](bool v) { state.inspectIncludeHardLinks = v; })
                                    .build();

                                innerUi.text("inspect.depthLbl")
                                    .text("扫描深度:")
                                    .fontSize(12.5f)
                                    .lineHeight(16.0f)
                                    .color(p.mutedText)
                                    .build();

                                const int depthIdx = state.inspectMaxDepth <= 2 ? 0 : (state.inspectMaxDepth <= 4 ? 1 : 2);
                                components::segmented(innerUi, "inspect.depthSeg")
                                    .theme(p.tokens)
                                    .size(168.0f, 28.0f)
                                    .items({"2 层", "4 层", "8 层"})
                                    .selected(depthIdx)
                                    .fontSize(12.0f)
                                    .transition(p.anim)
                                    .onChange([&](int idx) {
                                        state.inspectMaxDepth = idx == 0 ? 2 : (idx == 1 ? 4 : 8);
                                    })
                                    .build();

                                innerUi.row("inspect.optSp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                                innerUi.text("inspect.statusLine")
                                    .text(state.inspectStatusText)
                                    .fontSize(12.5f)
                                    .lineHeight(16.0f)
                                    .color(scanning ? p.accentText : p.mutedText)
                                    .build();
                            })
                            .build();
                    });

                    // 2. Single-path inspection detail
                    if (state.hasInspectedSingle) {
                        detail::DrawInspectedDetailCard(innerUi, "inspect.single", state, p, innerW,
                                                        state.inspectedSingle, "当前路径体检结果");
                    }

                    // 3. Scan Results DataTable + Selected Item Drawer
                    ui_kit::SectionCard(innerUi, "inspect.tableCard", p, innerW, 18.0f, 12.0f, [&](float cardW) {
                        innerUi.row("inspect.tblHdr")
                            .width(cardW)
                            .height(26.0f)
                            .alignItems(eui::Align::CENTER)
                            .content([&] {
                                innerUi.text("inspect.tblTitle")
                                    .text("目录链接扫描结果 (" + std::to_string(state.scannedLinks.size()) + " 项)")
                                    .fontSize(15.0f)
                                    .lineHeight(20.0f)
                                    .fontWeight(600)
                                    .color(p.text)
                                    .build();
                                innerUi.row("inspect.tblSp").flexGrow(1.0f).height(1.0f).content([] {}).build();
                                innerUi.text("inspect.tblHint")
                                    .text("点击列表行查看完整指向路径或执行安全解除")
                                    .fontSize(12.0f)
                                    .lineHeight(16.0f)
                                    .color(p.subtleText)
                                    .build();
                            })
                            .build();

                        const float col0 = 64.0f;
                        const float col1 = 84.0f;
                        const float col2 = std::max(160.0f, std::floor((cardW - col0 - col1) * 0.48f));
                        const float col3 = std::max(160.0f, cardW - col0 - col1 - col2 - 2.0f);

                        components::dataTable(innerUi, "inspect.table")
                            .theme(p.tokens)
                            .size(cardW, 236.0f)
                            .columns({"类型", "状态", "链接入口路径", "指向目标 / 硬链接信息"})
                            .columnWidths({col0, col1, col2, col3})
                            .rowHeight(34.0f)
                            .data(state.inspectTableRows, state.inspectTableRevision)
                            .sortable(true)
                            .resizable(true)
                            .onRowClick([&](std::size_t origIdx) {
                                if (origIdx < state.scannedLinks.size()) {
                                    state.selectedInspectIndex = static_cast<int>(origIdx);
                                }
                            })
                            .build();
                    });

                    if (state.selectedInspectIndex >= 0 &&
                        state.selectedInspectIndex < static_cast<int>(state.scannedLinks.size())) {
                        detail::DrawInspectedDetailCard(
                            innerUi, "inspect.selected", state, p, innerW,
                            state.scannedLinks[static_cast<std::size_t>(state.selectedInspectIndex)],
                            "已选中列表项详情与操作");
                    }
                })
                .build();
        })
        .build();
}

} // namespace fl::pages
