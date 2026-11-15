#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "app_actions.h"
#include "components/datatable.h"
#include "components/input.h"
#include "components/segmented.h"
#include "core/dsl.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

namespace fl {

inline LinkType ParseHistoryLinkType(const std::string& typeCode) {
    if (win::ContainsCaseInsensitive(typeCode, "/D")) return LinkType::Directory;
    if (win::ContainsCaseInsensitive(typeCode, "/H")) return LinkType::HardLink;
    if (win::ContainsCaseInsensitive(typeCode, "FILE")) return LinkType::FileSymlink;
    return LinkType::Junction;
}

inline void ComposeHistoryPage(eui::Ui& ui,
                               AppState& state,
                               const ui_kit::UiPalette& p,
                               float viewW,
                               float viewH) {
    const std::string histKey = std::to_string(state.historyTableRevision) + "_" +
                                std::to_string(state.selectedHistoryIndex) + "_" +
                                std::to_string(state.historyCategoryFilter);
    const bool hasRows = state.historyTableRows && !state.historyTableRows->empty();

    ui_kit::CenteredScrollPage(
        ui, "history.scroll", p, viewW, viewH, 900.0f, 32.0f,
        state.historyScrollOffset, histKey,
        [&](eui::Ui& innerUi, float studioW) {
            const std::string countLabel = "累计 " + std::to_string(state.history.size()) + " 条操作";
            ui_kit::PageHeader(
                innerUi, "history.pageHdr", studioW,
                "操作历史记录",
                "所有建链与搬家操作均已持久化记录，点击行可查看命令并一键回填",
                p, 140.0f, [&] {
                    ui_kit::StatusBadge(
                        innerUi, "history.totalBadge", p, 134.0f, 28.0f,
                        countLabel, 0,
                        state.history.empty()
                            ? ui_kit::SemanticColor{p.mutedText, p.pillBg, p.hairline}
                            : p.info);
                });

            ui_kit::SectionCard(innerUi, "history.filterCard", p, studioW, 18.0f, 12.0f, [&](float cardW) {
                static constexpr int kSegToFilter[] = {0, 2, 1, 5, 6};
                int segIdx = 0;
                for (int i = 0; i < 5; ++i) {
                    if (kSegToFilter[i] == state.historyCategoryFilter) {
                        segIdx = i;
                        break;
                    }
                }

                const float segW = 266.0f;
                const float dirW = 92.0f;
                const float clearW = 72.0f;
                const float gap = 8.0f;
                const float searchW = std::max(120.0f, cardW - segW - dirW - clearW - gap * 3.0f);

                innerUi.row("history.filterRow")
                    .size(cardW, 42.0f)
                    .gap(gap)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        components::input(innerUi, "history.searchInput")
                            .theme(p.tokens)
                            .size(searchW, 42.0f)
                            .fontSize(13.5f)
                            .value(state.historyFilterText)
                            .placeholder("搜索路径、命令类型或执行结果...")
                            .onChange([&](const std::string& val) {
                                state.historyFilterText = val;
                                actions::RebuildHistoryTable(state);
                            })
                            .build();

                        components::segmented(innerUi, "history.catSeg")
                            .theme(p.tokens)
                            .size(segW, 38.0f)
                            .items({"全部", "/J", "/D", "成功", "异常"})
                            .selected(segIdx)
                            .fontSize(12.5f)
                            .transition(p.anim)
                            .onChange([&](int idx) {
                                const int mapped = (idx >= 0 && idx < 5) ? kSegToFilter[idx] : 0;
                                state.historyCategoryFilter = mapped;
                                actions::RebuildHistoryTable(state);
                            })
                            .build();

                        ui_kit::ActionButton(
                            innerUi, "history.openLogDir", p, dirW, 38.0f,
                            "日志目录", 0xF07Cu, ui_kit::ButtonKind::Ghost,
                            [&] {
                                const auto pDir = std::filesystem::path(win::GetHistoryFilePath()).parent_path();
                                win::OpenFolderInExplorer(win::WideToUtf8(pDir.wstring()));
                            },
                            false, 12.5f);

                        ui_kit::ActionButton(
                            innerUi, "history.clearBtn", p, clearW, 38.0f,
                            "清空", 0xF1F8u, ui_kit::ButtonKind::Danger,
                            [&] {
                                if (state.history.empty()) return;
                                state.confirmAction = ConfirmAction::ClearHistory;
                                state.confirmTitle = "清空历史日志";
                                state.confirmMessage = "确认清空全部已保存的操作记录？\n此操作仅清除日志记录，不会影响磁盘上已创建的任何符号链接或文件。";
                                state.confirmPrimaryLabel = "确认清空";
                                state.confirmOpen = true;
                            },
                            state.history.empty(), 12.5f);
                    })
                    .build();
            });

            ui_kit::SectionCard(innerUi, "history.tableCard", p, studioW, 20.0f, 14.0f, [&](float cardW) {
                if (!hasRows) {
                    innerUi.column("history.emptyCol")
                        .width(cardW)
                        .gap(8.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            innerUi.stack("history.emptyIcon")
                                .size(48.0f, 48.0f)
                                .content([&] {
                                    innerUi.rect("history.emptyIconBg")
                                        .size(48.0f, 48.0f)
                                        .radius(24.0f)
                                        .color(p.pillBg)
                                        .build();
                                    innerUi.text("history.emptyIconIc")
                                        .size(48.0f, 48.0f)
                                        .icon(0xF1DAu)
                                        .fontSize(18.0f)
                                        .color(p.mutedText)
                                        .horizontalAlign(eui::HorizontalAlign::Center)
                                        .verticalAlign(eui::VerticalAlign::Center)
                                        .build();
                                })
                                .build();

                            innerUi.text("history.emptyTitle")
                                .size(cardW, 24.0f)
                                .text(state.history.empty() ? "暂无历史操作记录" : "没有符合筛选条件的记录")
                                .fontSize(15.5f)
                                .fontWeight(650)
                                .color(p.ink)
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            innerUi.text("history.emptySub")
                                .size(cardW, 20.0f)
                                .text(state.history.empty()
                                    ? "在“新建与迁移”中完成一次建链后，记录会自动出现在这里。"
                                    : "请尝试清除搜索关键词或切换为“全部”分类。")
                                .fontSize(13.0f)
                                .color(p.mutedText)
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();
                        })
                        .build();
                } else {
                    ui_kit::SectionTitle(
                        innerUi, "history.tblHdr", p, cardW,
                        "历史列表",
                        "显示 " + std::to_string(state.historyTableRows->size()) + " 条匹配记录");

                    const float c0 = 152.0f;
                    const float c1 = 88.0f;
                    const float c4 = 128.0f;
                    const float rem = std::max(180.0f, cardW - c0 - c1 - c4 - 2.0f);
                    const float c2 = std::floor(rem * 0.5f);
                    const float c3 = std::max(80.0f, rem - c2);

                    components::dataTable(innerUi, "history.table")
                        .theme(p.tokens)
                        .size(cardW, 280.0f)
                        .columns({"时间", "类型", "链接路径", "目标路径", "执行结果"})
                        .columnWidths({c0, c1, c2, c3, c4})
                        .rowHeight(40.0f)
                        .data(state.historyTableRows, state.historyTableRevision)
                        .sortable(true)
                        .resizable(true)
                        .onRowClick([&](std::size_t rowIdx) {
                            if (rowIdx < state.filteredHistoryIndices.size()) {
                                state.selectedHistoryIndex = state.filteredHistoryIndices[rowIdx];
                            }
                        })
                        .build();
                }
            });

            if (state.selectedHistoryIndex >= 0 &&
                state.selectedHistoryIndex < static_cast<int>(state.history.size())) {
                const HistoryItem& rec = state.history[static_cast<size_t>(state.selectedHistoryIndex)];

                ui_kit::SectionCard(innerUi, "history.detailCard", p, studioW, 20.0f, 14.0f, [&](float cardW) {
                    innerUi.row("history.detHdr")
                        .size(cardW, 28.0f)
                        .gap(10.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            const float badgeW = 104.0f;
                            const float timeW = 160.0f;
                            const float titleW = std::max(80.0f, cardW - badgeW - timeW - 20.0f);

                            innerUi.text("history.detTitle")
                                .size(titleW, 28.0f)
                                .text("记录详情 · " + rec.type)
                                .fontSize(15.0f)
                                .fontWeight(700)
                                .color(p.ink)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            innerUi.text("history.detTime")
                                .size(timeW, 28.0f)
                                .text(rec.timestamp)
                                .fontFamily("monospace")
                                .fontSize(12.0f)
                                .color(p.mutedText)
                                .horizontalAlign(eui::HorizontalAlign::Right)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            ui_kit::StatusBadge(
                                innerUi, "history.detBadge", p, badgeW, 26.0f,
                                rec.success ? "执行成功" : "执行异常", 0,
                                rec.success ? p.success : p.danger);
                        })
                        .build();

                    ui_kit::Hairline(innerUi, "history.detDiv", p, cardW);

                    ui_kit::MetaWell(innerUi, "history.detWell", p, cardW, {
                        {"链接入口", rec.linkPath, true, true},
                        {"真实目标", rec.targetPath, true, false},
                        {"执行结果", rec.result, false, false},
                    });

                    innerUi.row("history.detActions")
                        .size(cardW, 42.0f)
                        .gap(8.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            ui_kit::ActionButton(
                                innerUi, "history.openLinkBtn", p, 118.0f, 38.0f,
                                "定位入口", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                                [&] { win::RevealInExplorer(rec.linkPath); },
                                rec.linkPath.empty(), 13.0f);

                            ui_kit::ActionButton(
                                innerUi, "history.openTargetBtn", p, 118.0f, 38.0f,
                                "定位目标", 0xF08Eu, ui_kit::ButtonKind::Secondary,
                                [&] { win::RevealInExplorer(rec.targetPath); },
                                rec.targetPath.empty(), 13.0f);

                            ui_kit::ActionButton(
                                innerUi, "history.inspectBtn", p, 102.0f, 38.0f,
                                "体检状态", 0xF002u, ui_kit::ButtonKind::InfoAccent,
                                [&] {
                                    actions::InspectPathNow(state, rec.linkPath);
                                    state.activeTab = 1;
                                },
                                rec.linkPath.empty(), 13.0f);

                            ui_kit::ActionButton(
                                innerUi, "history.copyBtn", p, 98.0f, 38.0f,
                                "复制命令", 0xF0C5u, ui_kit::ButtonKind::Ghost,
                                [&] {
                                    const std::string cmd = win::BuildMklinkCommand(
                                        ParseHistoryLinkType(rec.type), rec.linkPath, rec.targetPath);
                                    if (win::CopyTextToClipboard(cmd)) {
                                        ui_kit::TriggerToast(state, "已复制命令", cmd, 1, 0xF0C5u);
                                    }
                                },
                                false, 13.0f);

                            const float spW = std::max(8.0f, cardW - 118.0f - 118.0f - 102.0f - 98.0f - 168.0f - 40.0f);
                            innerUi.rect("history.detSp").size(spW, 1.0f).color(ui_kit::kTransparent).build();

                            ui_kit::HeroButton(
                                innerUi, "history.loadBtn", p, 168.0f, 40.0f,
                                "回填至新建页", 0xF061u,
                                [&] {
                                    state.operationMode = win::ContainsCaseInsensitive(rec.type, "MOVE")
                                        ? OperationMode::MoveAndLink
                                        : OperationMode::CreateOnly;
                                    state.linkType = ParseHistoryLinkType(rec.type);
                                    state.linkPath = rec.linkPath;
                                    state.targetPath = rec.targetPath;
                                    state.activeTab = 0;
                                    actions::MarkValidationDirty(state);
                                    ui_kit::TriggerToast(state, "已回填参数", "已切换至“新建与迁移”并填入原路径参数。", 0, 0xF05Au);
                                },
                                false, 13.5f);
                        })
                        .build();
                });
            }
        });
}

} // namespace fl
