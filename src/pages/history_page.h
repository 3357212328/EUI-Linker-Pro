#pragma once

#include "app_actions.h"
#include "eui_neo.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

#include <algorithm>
#include <string>

namespace fl::pages {

namespace detail {

inline LinkType ParseHistoryLinkType(const std::string& typeCode) {
    if (win::ContainsCaseInsensitive(typeCode, "/D")) return LinkType::Directory;
    if (win::ContainsCaseInsensitive(typeCode, "/H")) return LinkType::HardLink;
    if (win::ContainsCaseInsensitive(typeCode, "FILE")) return LinkType::FileSymlink;
    return LinkType::Junction;
}

} // namespace detail

inline void ComposeHistoryPage(eui::Ui& ui,
                               AppState& state,
                               const ui_kit::UiPalette& p,
                               float width,
                               float height) {
    const bool hasSelected = state.selectedHistoryIndex >= 0 &&
        state.selectedHistoryIndex < static_cast<int>(state.history.size());

    ui.column("page.history.root")
        .size(width, height)
        .gap(12.0f)
        .content([&] {
            // 1. Search & Filter Toolbar Card
            ui_kit::SectionCard(ui, "history.toolbarCard", p, width, 16.0f, 10.0f, [&](float innerW) {
                ui.row("history.topRow")
                    .width(innerW)
                    .height(38.0f)
                    .gap(8.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        const float logBtnW = 112.0f;
                        const float clearBtnW = 96.0f;
                        const float searchW = std::max(160.0f, innerW - logBtnW - clearBtnW - 16.0f);

                        components::input(ui, "history.searchInput")
                            .theme(p.tokens)
                            .size(searchW, 38.0f)
                            .fontSize(13.5f)
                            .value(state.historyFilterText)
                            .placeholder("搜索历史记录：时间、/D /J /H、链接或目标路径、状态结果...")
                            .onChange([&](const std::string& v) {
                                state.historyFilterText = v;
                                actions::RebuildHistoryTable(state);
                            })
                            .build();

                        ui_kit::ActionButton(ui, "history.openLogDir", p, logBtnW, 38.0f,
                                             "打开日志目录", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                                             [&] { win::OpenFolderInExplorer(win::GetAppDataDir()); },
                                             false, 12.0f);

                        ui_kit::ActionButton(ui, "history.clearBtn", p, clearBtnW, 38.0f,
                                             "清空日志", 0xF1F8u, ui_kit::ButtonKind::Danger,
                                             [&] {
                                                 state.confirmAction = ConfirmAction::ClearHistory;
                                                 state.confirmTitle = "确认清空全部历史记录";
                                                 state.confirmMessage =
                                                     "即将清空本地保存的所有 mklink 操作记录（共 " +
                                                     std::to_string(state.history.size()) +
                                                     " 条）。注意：这只会删除日志文本，不会影响已建立的系统链接。";
                                                 state.confirmPrimaryLabel = "确认清空";
                                                 state.confirmOpen = true;
                                             },
                                             state.history.empty(), 12.0f);
                    })
                    .build();

                ui.row("history.filterRow")
                    .width(innerW)
                    .height(32.0f)
                    .gap(10.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        const float segW = std::min(510.0f, std::max(340.0f, innerW - 180.0f));
                        components::segmented(ui, "history.catSeg")
                            .theme(p.tokens)
                            .size(segW, 30.0f)
                            .items({"全部", "/D 目录", "/J 联接", "/H 硬链", "文件软链", "仅成功", "仅失败"})
                            .selected(state.historyCategoryFilter)
                            .fontSize(12.0f)
                            .transition(p.anim)
                            .onChange([&](int idx) {
                                state.historyCategoryFilter = idx;
                                actions::RebuildHistoryTable(state);
                            })
                            .build();

                        ui.row("history.filterSp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                        ui.text("history.countLbl")
                            .text("显示 " + std::to_string(state.filteredHistoryIndices.size()) +
                                  " / 共 " + std::to_string(state.history.size()) + " 条")
                            .fontSize(12.5f)
                            .lineHeight(16.0f)
                            .color(p.mutedText)
                            .build();
                    })
                    .build();
            });

            // 2. Virtualized DataTable Card
            const float reservedBottom = hasSelected ? 200.0f : 12.0f;
            const float tableCardH = std::max(176.0f, height - 118.0f - reservedBottom - 12.0f);
            const float tableW = std::max(300.0f, width - 28.0f);
            const float tableH = std::max(140.0f, tableCardH - 28.0f);

            components::card(ui, "history.tableCard")
                .theme(p.tokens)
                .size(width, tableCardH)
                .padding(14.0f)
                .color(p.cardBg)
                .radius(14.0f)
                .border(1.0f, p.border)
                .shadow(p.cardShadow)
                .content([&] {
                    const float c0 = 148.0f;
                    const float c1 = 82.0f;
                    const float c4 = 126.0f;
                    const float rem = std::max(180.0f, tableW - c0 - c1 - c4 - 2.0f);
                    const float c2 = std::floor(rem * 0.5f);
                    const float c3 = std::max(80.0f, rem - c2);

                    components::dataTable(ui, "history.table")
                        .theme(p.tokens)
                        .size(tableW, tableH)
                        .columns({"时间", "类型", "链接路径 (Link)", "目标路径 (Target)", "执行结果"})
                        .columnWidths({c0, c1, c2, c3, c4})
                        .rowHeight(34.0f)
                        .data(state.historyTableRows, state.historyTableRevision)
                        .sortable(true)
                        .resizable(true)
                        .onRowClick([&](std::size_t rowIdx) {
                            if (rowIdx < state.filteredHistoryIndices.size()) {
                                state.selectedHistoryIndex = state.filteredHistoryIndices[rowIdx];
                            }
                        })
                        .build();
                })
                .build();

            // 3. Selected History Inspector Card
            if (hasSelected) {
                const HistoryItem& item = state.history[static_cast<std::size_t>(state.selectedHistoryIndex)];
                ui_kit::SectionCard(ui, "history.detailCard", p, width, 16.0f, 8.0f, [&](float innerW) {
                    ui.row("history.detailHdr")
                        .width(innerW)
                        .height(26.0f)
                        .gap(8.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            ui.text("history.detailTitle")
                                .text("选中记录详情 · " + item.timestamp)
                                .fontSize(14.5f)
                                .lineHeight(18.0f)
                                .fontWeight(600)
                                .color(p.text)
                                .build();

                            ui_kit::StatusBadge(ui, "history.detType", p, 86.0f, 22.0f,
                                                item.type, 0xF0C1u, p.info);

                            ui.row("history.detSp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                            ui_kit::StatusBadge(ui, "history.detRes", p, 146.0f, 22.0f,
                                                item.result,
                                                item.success ? 0xF058u : 0xF057u,
                                                item.success ? p.success : p.danger);
                        })
                        .build();

                    ui.text("history.detLink")
                        .width(innerW)
                        .height(eui::SizeValue::wrapContent())
                        .maxWidth(innerW)
                        .wrap(true)
                        .fontFamily("monospace")
                        .fontSize(12.0f)
                        .lineHeight(17.0f)
                        .color(p.text)
                        .text("Link  : " + item.linkPath)
                        .build();

                    ui.text("history.detTarget")
                        .width(innerW)
                        .height(eui::SizeValue::wrapContent())
                        .maxWidth(innerW)
                        .wrap(true)
                        .fontFamily("monospace")
                        .fontSize(12.0f)
                        .lineHeight(17.0f)
                        .color(p.mutedText)
                        .text("Target: " + item.targetPath)
                        .build();

                    ui.row("history.detActions")
                        .width(innerW)
                        .height(34.0f)
                        .gap(8.0f)
                        .content([&] {
                            ui_kit::ActionButton(ui, "history.openLinkBtn", p, 128.0f, 34.0f,
                                                 "定位链接位置", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                                                 [link = item.linkPath] { win::RevealInExplorer(link); },
                                                 false, 12.0f);

                            ui_kit::ActionButton(ui, "history.openTargetBtn", p, 128.0f, 34.0f,
                                                 "定位真实目标", 0xF08Eu, ui_kit::ButtonKind::Secondary,
                                                 [target = item.targetPath] { win::RevealInExplorer(target); },
                                                 false, 12.0f);

                            ui_kit::ActionButton(ui, "history.inspectBtn", p, 118.0f, 34.0f,
                                                 "链接状态体检", 0xF002u, ui_kit::ButtonKind::InfoAccent,
                                                 [&state, link = item.linkPath] {
                                                     actions::InspectPathNow(state, link);
                                                     state.activeTab = 1;
                                                 },
                                                 false, 12.0f);

                            ui_kit::ActionButton(ui, "history.copyBtn", p, 102.0f, 34.0f,
                                                 "复制命令", 0xF0C5u, ui_kit::ButtonKind::Subtle,
                                                 [&state, item] {
                                                     const LinkType lt = detail::ParseHistoryLinkType(item.type);
                                                     const std::string cmd = win::BuildMklinkCommand(lt, item.linkPath, item.targetPath);
                                                     if (win::CopyTextToClipboard(cmd)) {
                                                         ui_kit::TriggerToast(state, "已复制命令", cmd, 1, 0xF0C5u);
                                                     }
                                                 },
                                                 false, 12.0f);

                            ui.row("history.actSp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                            ui_kit::ActionButton(ui, "history.loadBtn", p, 138.0f, 34.0f,
                                                 "回填参数到创建页", 0xF061u, ui_kit::ButtonKind::Primary,
                                                 [&state, item] {
                                                     state.linkType = detail::ParseHistoryLinkType(item.type);
                                                     state.operationMode = OperationMode::CreateOnly;
                                                     state.linkPath = item.linkPath;
                                                     state.targetPath = item.targetPath;
                                                     state.activeTab = 0;
                                                     ui_kit::TriggerToast(state, "已回填参数", "已切换至创建工作台并填入原参数。", 0, 0xF05Au);
                                                 },
                                                 false, 12.0f);
                        })
                        .build();
                });
            }
        })
        .build();
}

} // namespace fl::pages
