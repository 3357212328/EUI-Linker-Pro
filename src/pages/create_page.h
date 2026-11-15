#pragma once

#include <algorithm>
#include <filesystem>
#include <string>

#include "app_actions.h"
#include "components/input.h"
#include "components/segmented.h"
#include "components/switch.h"
#include "core/dsl.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

namespace fl {

struct LinkOptionSpec {
    LinkType type;
    const char* title;
    const char* badge;
    const char* tagline;
    const char* detail;
    bool allowInMoveMode;
};

inline std::string NormalizePastedInput(const std::string& raw) {
    std::string out = win::CleanPathInput(raw);
    if (out.size() >= 2 && out.front() == '"' && out.back() == '"') {
        out = win::CleanPathInput(out.substr(1, out.size() - 2));
    }
    return out;
}

inline void DrawLinkTypeSelector(eui::Ui& ui,
                                 AppState& state,
                                 const ui_kit::UiPalette& p,
                                 float width) {
    static constexpr LinkOptionSpec kOptions[] = {
        {
            LinkType::Junction,
            "目录联接",
            "/J",
            "免提权 · 推荐",
            "NTFS 目录联接：同机推荐首选，无需管理员权限即可生效，最适合游戏、缓存与开发依赖搬家。",
            true
        },
        {
            LinkType::Directory,
            "符号链接",
            "/D",
            "支持网络共享",
            "目录符号链接：支持相对路径与 \\\\NAS 共享路径，建议配合 Windows 开发者模式或管理员使用。",
            true
        },
        {
            LinkType::HardLink,
            "文件硬链",
            "/H",
            "同盘零占空间",
            "文件硬链接：同一磁盘分区内共享同一文件数据块，删除其一不影响原文件且免提权。",
            false
        },
        {
            LinkType::FileSymlink,
            "文件软链",
            "File",
            "跨盘单文件",
            "单文件符号链接：可将任意盘符或网络位置上的配置文件、存档单文件透明映射到目标位置。",
            false
        },
    };

    const bool isMove = (state.operationMode == OperationMode::MoveAndLink);
    const float trayPad = 4.0f;
    const float gap = 6.0f;
    const float pillW = std::max(80.0f, (width - trayPad * 2.0f - gap * 3.0f) / 4.0f);
    const float pillH = 54.0f;
    const float trayH = pillH + trayPad * 2.0f;

    std::string activeDetail = kOptions[0].detail;
    for (const auto& opt : kOptions) {
        if (opt.type == state.linkType) {
            activeDetail = opt.detail;
            break;
        }
    }

    ui.column("create.typeSelWrap")
        .width(width)
        .gap(8.0f)
        .content([&] {
            ui.stack("create.typeTray")
                .size(width, trayH)
                .content([&] {
                    ui.rect("create.typeTrayBg")
                        .size(width, trayH)
                        .radius(13.0f)
                        .color(p.cardAltBg)
                        .build();

                    ui.row("create.typeRow")
                        .position(trayPad, trayPad)
                        .size(width - trayPad * 2.0f, pillH)
                        .gap(gap)
                        .content([&] {
                            for (int i = 0; i < 4; ++i) {
                                const auto& opt = kOptions[i];
                                const bool selected = (state.linkType == opt.type);
                                const bool disabled = isMove && !opt.allowInMoveMode;
                                const std::string pid = "create.typeOpt." + std::to_string(i);

                                auto pill = ui.stack(pid)
                                    .size(pillW, pillH)
                                    .visualStateFrom(pid, disabled ? 1.0f : 0.98f)
                                    .transition(p.anim);
                                if (!disabled) {
                                    pill.onClick([&state, t = opt.type] {
                                        if (state.linkType != t) {
                                            state.linkType = t;
                                            actions::PersistSettings(state);
                                            actions::MarkValidationDirty(state);
                                        }
                                    });
                                }

                                pill.content([&] {
                                    auto bg = ui.rect(pid + ".bg")
                                        .size(pillW, pillH)
                                        .radius(10.0f)
                                        .color(selected ? p.cardBg : ui_kit::kTransparent)
                                        .hoverColor(disabled ? ui_kit::kTransparent
                                                             : (selected ? p.cardBg : p.navHover))
                                        .pressedColor(disabled ? ui_kit::kTransparent : p.pillHover)
                                        .border(selected ? 1.0f : 0.0f,
                                                selected ? p.hairlineStrong : ui_kit::kTransparent)
                                        .visualStateFrom(pid, 1.0f)
                                        .transition(p.anim)
                                        .animate(eui::AnimProperty::Color | eui::AnimProperty::Border);
                                    if (selected) {
                                        bg.shadow(10.0f, 0.0f, 2.0f, p.shadow);
                                    }
                                    bg.build();

                                    ui.column(pid + ".col")
                                        .position(12.0f, 8.0f)
                                        .size(std::max(40.0f, pillW - 24.0f), pillH - 16.0f)
                                        .gap(3.0f)
                                        .content([&] {
                                            const float innerPillW = std::max(40.0f, pillW - 24.0f);
                                            const float badgeW = 36.0f;
                                            ui.row(pid + ".top")
                                                .size(innerPillW, 20.0f)
                                                .gap(6.0f)
                                                .alignItems(eui::Align::CENTER)
                                                .content([&] {
                                                    ui.text(pid + ".title")
                                                        .size(std::max(20.0f, innerPillW - badgeW - 6.0f), 20.0f)
                                                        .text(opt.title)
                                                        .fontSize(13.5f)
                                                        .fontWeight(selected ? 700 : 600)
                                                        .color(disabled ? p.faintText : (selected ? p.ink : p.secondaryText))
                                                        .verticalAlign(eui::VerticalAlign::Center)
                                                        .build();

                                                    ui.stack(pid + ".badge")
                                                        .size(badgeW, 18.0f)
                                                        .content([&] {
                                                            ui.rect(pid + ".badgeBg")
                                                                .size(badgeW, 18.0f)
                                                                .radius(6.0f)
                                                                .color(disabled
                                                                    ? ui_kit::kTransparent
                                                                    : (selected ? p.accentSoft : p.pillBg))
                                                                .build();
                                                            ui.text(pid + ".badgeTxt")
                                                                .size(badgeW, 18.0f)
                                                                .text(opt.badge)
                                                                .fontFamily("monospace")
                                                                .fontSize(11.0f)
                                                                .fontWeight(700)
                                                                .color(disabled
                                                                    ? p.faintText
                                                                    : (selected ? p.accent : p.mutedText))
                                                                .horizontalAlign(eui::HorizontalAlign::Center)
                                                                .verticalAlign(eui::VerticalAlign::Center)
                                                                .build();
                                                        })
                                                        .build();
                                                })
                                                .build();

                                            ui.text(pid + ".tag")
                                                .size(innerPillW, 16.0f)
                                                .text(disabled ? "搬家模式不可用" : opt.tagline)
                                                .fontSize(11.5f)
                                                .color(disabled ? p.faintText : (selected ? p.accent : p.mutedText))
                                                .verticalAlign(eui::VerticalAlign::Center)
                                                .build();
                                        })
                                        .build();
                                }).build();
                            }
                        })
                        .build();
                })
                .build();

            ui.text("create.typeDetail")
                .size(width, 20.0f)
                .text(activeDetail)
                .fontSize(12.5f)
                .color(p.mutedText)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();
}

inline void DrawPathSlotHeader(eui::Ui& ui,
                               const std::string& id,
                               const ui_kit::UiPalette& p,
                               float width,
                               const std::string& label,
                               const std::string& hint) {
    ui.row(id)
        .size(width, 20.0f)
        .gap(12.0f)
        .alignItems(eui::Align::CENTER)
        .content([&] {
            const float labelW = std::min(width * 0.48f, static_cast<float>(label.size()) * 9.0f + 16.0f);
            ui.text(id + ".lbl")
                .size(labelW, 20.0f)
                .text(label)
                .fontSize(13.5f)
                .fontWeight(650)
                .color(p.ink)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();

            ui.text(id + ".hint")
                .size(std::max(40.0f, width - labelW - 12.0f), 20.0f)
                .text(hint)
                .fontSize(12.0f)
                .color(p.mutedText)
                .horizontalAlign(eui::HorizontalAlign::Right)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();
}

inline void ComposeCreatePage(eui::Ui& ui,
                              AppState& state,
                              const ui_kit::UiPalette& p,
                              float viewW,
                              float viewH) {
    const bool isMove = (state.operationMode == OperationMode::MoveAndLink);
    const bool wantsDir = win::IsDirectoryTargetRequired(state.linkType);
    const bool executing = app::async::running("fl.create");
    const bool canExecute = !executing && state.cachedValidation.canExecute;

    const std::string key = std::string("create_") + (isMove ? "move_" : "link_") +
                            std::to_string(state.consoleOutput.size());

    ui_kit::CenteredScrollPage(
        ui, "create.scroll", p, viewW, viewH, 820.0f, 32.0f,
        state.createScrollOffset, key,
        [&](eui::Ui& innerUi, float studioW) {
            // 1. Editorial Page Header + Compact EUI-NEO Segmented Mode Switch
            ui_kit::PageHeader(
                innerUi, "create.pageHdr", studioW,
                "新建链接与迁移",
                isMove
                    ? "先安全迁移真实目录至新位置，再在原位留存透明联接"
                    : "为目录或文件创建 NTFS 链接入口，零冗余占用空间",
                p, 240.0f, [&] {
                    components::segmented(innerUi, "create.modeSeg")
                        .theme(p.tokens)
                        .size(238.0f, 38.0f)
                        .items({"创建新链接", "目录搬家并建链"})
                        .selected(isMove ? 1 : 0)
                        .fontSize(13.0f)
                        .transition(p.anim)
                        .onChange([&](int idx) {
                            const OperationMode nextMode = (idx == 1)
                                ? OperationMode::MoveAndLink
                                : OperationMode::CreateOnly;
                            if (state.operationMode != nextMode) {
                                state.operationMode = nextMode;
                                if (nextMode == OperationMode::MoveAndLink &&
                                    state.linkType != LinkType::Junction &&
                                    state.linkType != LinkType::Directory) {
                                    state.linkType = LinkType::Junction;
                                }
                                actions::PersistSettings(state);
                                actions::MarkValidationDirty(state);
                            }
                        })
                        .build();
                });

            // 2. Unified Type & Paths Studio Card
            ui_kit::SectionCard(innerUi, "create.studioCard", p, studioW, 22.0f, 16.0f, [&](float cardW) {
                DrawLinkTypeSelector(innerUi, state, p, cardW);

                ui_kit::Hairline(innerUi, "create.div1", p, cardW);

                // Slot 1: Link / Source input
                DrawPathSlotHeader(
                    innerUi, "create.slot1Hdr", p, cardW,
                    isMove ? "现有源目录路径" : "新链接入口路径",
                    isMove ? "将被迁往新盘符，原地自动留存链接" : "将创建一个透明访问入口（不得已存在）");

                const float browseW = 84.0f;
                const float pasteW = 68.0f;
                const float btnGap = 8.0f;
                const float input1W = std::max(140.0f, cardW - browseW - pasteW - btnGap * 2.0f);

                innerUi.row("create.slot1Row")
                    .size(cardW, 44.0f)
                    .gap(btnGap)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        components::input(innerUi, "create.linkInput")
                            .theme(p.tokens)
                            .size(input1W, 44.0f)
                            .fontSize(14.0f)
                            .value(state.linkPath)
                            .placeholder(isMove
                                ? "例如 C:\\Users\\Me\\Documents\\LargeProject"
                                : (wantsDir ? "例如 C:\\Games\\MyApp 或选目录后点下方追加原名"
                                            : "例如 C:\\Configs\\settings.json"))
                            .onChange([&](const std::string& val) {
                                state.linkPath = val;
                                actions::MarkValidationDirty(state);
                            })
                            .onEnter([&] {
                                actions::ExecuteOrConfirmCreate(state);
                            })
                            .build();

                        ui_kit::ActionButton(
                            innerUi, "create.slot1Browse", p, browseW, 44.0f,
                            "浏览", 0xF07Cu, ui_kit::ButtonKind::Secondary,
                            [&] {
                                if (isMove) {
                                    const auto chosen = win::BrowseDirectory("选择需要搬家腾空的源目录", state.linkPath);
                                    if (!chosen.empty()) {
                                        state.linkPath = win::CleanPathInput(chosen);
                                        actions::MarkValidationDirty(state);
                                    }
                                } else {
                                    std::string chosen = state.linkPath;
                                    if (win::ChooseNewLinkPath(chosen, wantsDir)) {
                                        state.linkPath = win::CleanPathInput(chosen);
                                        actions::MarkValidationDirty(state);
                                    }
                                }
                            },
                            false, 13.5f);

                        ui_kit::ActionButton(
                            innerUi, "create.slot1Paste", p, pasteW, 44.0f,
                            "粘贴", 0xF0EAu, ui_kit::ButtonKind::Secondary,
                            [&] {
                                const std::string clip = NormalizePastedInput(win::ReadTextFromClipboard());
                                if (!clip.empty()) {
                                    state.linkPath = clip;
                                    actions::MarkValidationDirty(state);
                                }
                            },
                            false, 13.5f);
                    })
                    .build();

                // Quiet connector & inline helpers
                innerUi.row("create.bridgeRow")
                    .size(cardW, 32.0f)
                    .gap(8.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        const float rightToolsW = isMove
                            ? (86.0f + 142.0f + 16.0f)
                            : (70.0f + 86.0f + 142.0f + 24.0f);
                        const float arrowAreaW = std::max(80.0f, cardW - rightToolsW - 8.0f);

                        innerUi.row("create.bridgeLeft")
                            .size(arrowAreaW, 32.0f)
                            .gap(7.0f)
                            .alignItems(eui::Align::CENTER)
                            .content([&] {
                                innerUi.text("create.bridgeArrow")
                                    .size(16.0f, 32.0f)
                                    .icon(0xF063u)
                                    .fontSize(13.0f)
                                    .color(p.accent)
                                    .horizontalAlign(eui::HorizontalAlign::Center)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .build();

                                innerUi.text("create.bridgeLbl")
                                    .size(std::max(40.0f, arrowAreaW - 23.0f), 32.0f)
                                    .text(isMove ? "迁移至下方目录后原地建立链接" : "透明指向下方真实目标")
                                    .fontSize(12.5f)
                                    .color(p.mutedText)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .build();
                            })
                            .build();

                        if (!isMove) {
                            ui_kit::ActionButton(
                                innerUi, "create.swapBtn", p, 70.0f, 30.0f,
                                "互换", 0xF362u, ui_kit::ButtonKind::Ghost,
                                [&] {
                                    std::swap(state.linkPath, state.targetPath);
                                    actions::MarkValidationDirty(state);
                                },
                                state.linkPath.empty() && state.targetPath.empty(), 12.5f);
                        }

                        ui_kit::ActionButton(
                            innerUi, "create.appendBtn", p, 86.0f, 30.0f,
                            "追加原名", 0xF067u, ui_kit::ButtonKind::Ghost,
                            [&] {
                                actions::ApplyQuickFix(
                                    state,
                                    isMove ? FixAction::AppendLinkFolderNameToTarget
                                           : FixAction::AppendTargetNameToLink);
                            },
                            state.linkPath.empty() || state.targetPath.empty(), 12.5f);

                        components::toggleSwitch(innerUi, "create.autoParentSwitch")
                            .theme(p.tokens)
                            .size(142.0f, 30.0f)
                            .checked(state.autoCreateParent)
                            .text("自动建父目录")
                            .fontSize(12.5f)
                            .transition(p.anim)
                            .onChange([&](bool val) {
                                state.autoCreateParent = val;
                                actions::PersistSettings(state);
                                actions::MarkValidationDirty(state);
                            })
                            .build();
                    })
                    .build();

                // Slot 2: Target / Destination input
                DrawPathSlotHeader(
                    innerUi, "create.slot2Hdr", p, cardW,
                    isMove ? "迁移后的新位置" : "真实目标路径",
                    isMove ? "选好父盘符会自动附加原目录名" : "支持本地文件夹、文件或 \\\\NAS 共享");

                const float input2W = std::max(140.0f, cardW - browseW - pasteW - btnGap * 2.0f);
                innerUi.row("create.slot2Row")
                    .size(cardW, 44.0f)
                    .gap(btnGap)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        components::input(innerUi, "create.targetInput")
                            .theme(p.tokens)
                            .size(input2W, 44.0f)
                            .fontSize(14.0f)
                            .value(state.targetPath)
                            .placeholder(isMove
                                ? "例如 D:\\Migrated\\LargeProject"
                                : (wantsDir ? "例如 D:\\Storage\\MyRealFolder 或 \\\\NAS\\Share"
                                            : "例如 D:\\Storage\\settings.json"))
                            .onChange([&](const std::string& val) {
                                state.targetPath = val;
                                actions::MarkValidationDirty(state);
                            })
                            .onEnter([&] {
                                actions::ExecuteOrConfirmCreate(state);
                            })
                            .build();

                        ui_kit::ActionButton(
                            innerUi, "create.slot2Browse", p, browseW, 44.0f,
                            wantsDir ? "选目录" : "选文件",
                            wantsDir ? 0xF07Cu : 0xF15Bu,
                            ui_kit::ButtonKind::Secondary,
                            [&] {
                                if (wantsDir || isMove) {
                                    const auto chosen = win::BrowseDirectory(
                                        isMove ? "选择迁移目标位置（选上级目录会自动拼接原名）"
                                               : "选择真实目标目录",
                                        state.targetPath);
                                    if (!chosen.empty()) {
                                        std::filesystem::path pTarget(win::Utf8ToWide(win::CleanPathInput(chosen)));
                                        if (isMove && !state.linkPath.empty()) {
                                            const auto srcName = std::filesystem::path(
                                                win::Utf8ToWide(win::CleanPathInput(state.linkPath))).filename();
                                            if (!srcName.empty() && pTarget.filename() != srcName) {
                                                pTarget /= srcName;
                                            }
                                        }
                                        state.targetPath = win::CleanPathInput(win::WideToUtf8(pTarget.wstring()));
                                        actions::MarkValidationDirty(state);
                                    }
                                } else {
                                    const auto chosen = win::BrowseFile("选择真实目标文件", state.targetPath);
                                    if (!chosen.empty()) {
                                        state.targetPath = win::CleanPathInput(chosen);
                                        actions::MarkValidationDirty(state);
                                    }
                                }
                            },
                            false, 13.5f);

                        ui_kit::ActionButton(
                            innerUi, "create.slot2Paste", p, pasteW, 44.0f,
                            "粘贴", 0xF0EAu, ui_kit::ButtonKind::Secondary,
                            [&] {
                                const std::string clip = NormalizePastedInput(win::ReadTextFromClipboard());
                                if (!clip.empty()) {
                                    state.targetPath = clip;
                                    actions::MarkValidationDirty(state);
                                }
                            },
                            false, 13.5f);
                    })
                    .build();
            });

            // 3. Preflight Validation, Command Preview & Primary Execution Bar
            ui_kit::SectionCard(innerUi, "create.execCard", p, studioW, 20.0f, 14.0f, [&](float cardW) {
                const auto& val = state.cachedValidation;
                int bannerTone = 0;
                uint32_t bannerIcon = 0xF05Au;
                if (val.level == ValidationLevel::Valid) {
                    bannerTone = 1;
                    bannerIcon = 0xF058u;
                } else if (val.level == ValidationLevel::Warning) {
                    bannerTone = 2;
                    bannerIcon = 0xF071u;
                } else if (val.level == ValidationLevel::Error) {
                    bannerTone = 3;
                    bannerIcon = 0xF057u;
                }

                ui_kit::BannerBox(
                    innerUi, "create.preflight", p, cardW,
                    ui_kit::ToneColors(p, bannerTone), bannerIcon,
                    val.message,
                    val.quickFixLabel,
                    val.quickFix == FixAction::None
                        ? std::function<void()>()
                        : std::function<void()>([&] {
                            actions::ApplyQuickFix(state, val.quickFix);
                        }));

                const std::string previewCmd = isMove
                    ? ("robocopy \"" +
                       (state.linkPath.empty() ? "<源目录>" : win::CleanPathInput(state.linkPath)) +
                       "\" \"" +
                       (state.targetPath.empty() ? "<新位置>" : win::CleanPathInput(state.targetPath)) +
                       "\" /E /MOVE  &&  " +
                       win::BuildMklinkCommand(state.linkType, state.linkPath, state.targetPath))
                    : win::BuildMklinkCommand(state.linkType, state.linkPath, state.targetPath);

                ui_kit::CodeBar(
                    innerUi, "create.cmdBar", p, cardW,
                    previewCmd, "复制", 0xF0C5u,
                    [&] {
                        if (win::CopyTextToClipboard(previewCmd)) {
                            ui_kit::TriggerToast(state, "已复制命令", previewCmd, 1, 0xF0C5u);
                        }
                    });

                const float heroW = isMove ? 216.0f : 196.0f;
                const float resetW = 84.0f;
                innerUi.row("create.actionBar")
                    .size(cardW, 46.0f)
                    .gap(10.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        const std::string ctaLabel = executing
                            ? "正在执行..."
                            : (isMove ? "开始搬家并建链" : "立即创建链接");

                        ui_kit::HeroButton(
                            innerUi, "create.runBtn", p, heroW, 46.0f,
                            ctaLabel,
                            isMove ? 0xF061u : 0xF0C1u,
                            [&] { actions::ExecuteOrConfirmCreate(state); },
                            !canExecute, 14.5f);

                        ui_kit::ActionButton(
                            innerUi, "create.resetBtn", p, resetW, 44.0f,
                            "重置", 0xF0E2u, ui_kit::ButtonKind::Ghost,
                            [&] {
                                state.linkPath.clear();
                                state.targetPath.clear();
                                state.consoleOutput.clear();
                                state.statusText = "就绪";
                                state.statusTone = 0;
                                actions::MarkValidationDirty(state);
                            },
                            executing, 13.5f);

                        innerUi.text("create.shortcutHint")
                            .size(std::max(40.0f, cardW - heroW - resetW - 20.0f), 44.0f)
                            .text("Ctrl + Enter 快速执行")
                            .fontSize(12.0f)
                            .color(p.faintText)
                            .horizontalAlign(eui::HorizontalAlign::Right)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();
                    })
                    .build();
            });

            // 4. On-Demand Execution Output Card
            if (!state.consoleOutput.empty()) {
                ui_kit::SectionCard(innerUi, "create.resultCard", p, studioW, 20.0f, 12.0f, [&](float cardW) {
                    innerUi.row("create.resHdr")
                        .size(cardW, 30.0f)
                        .gap(8.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            ui_kit::StatusBadge(
                                innerUi, "create.resBadge", p, 124.0f, 28.0f,
                                state.statusText.empty() ? "执行反馈" : state.statusText,
                                0, ui_kit::ToneColors(p, state.statusTone));

                            const float spacerW = std::max(10.0f, cardW - 124.0f - 88.0f - 82.0f - 66.0f - 32.0f);
                            innerUi.rect("create.resSpacer").size(spacerW, 1.0f).color(ui_kit::kTransparent).build();

                            ui_kit::ActionButton(
                                innerUi, "create.resOpenBtn", p, 88.0f, 30.0f,
                                "定位入口", 0xF07Cu, ui_kit::ButtonKind::Ghost,
                                [&] { win::RevealInExplorer(state.linkPath); },
                                state.linkPath.empty(), 12.5f);

                            ui_kit::ActionButton(
                                innerUi, "create.resInspectBtn", p, 82.0f, 30.0f,
                                "去体检", 0xF002u, ui_kit::ButtonKind::InfoAccent,
                                [&] {
                                    if (!state.linkPath.empty()) {
                                        actions::InspectPathNow(state, state.linkPath);
                                    }
                                    state.activeTab = 1;
                                },
                                state.linkPath.empty(), 12.5f);

                            ui_kit::ActionButton(
                                innerUi, "create.resCloseBtn", p, 66.0f, 30.0f,
                                "收起", 0xF00Du, ui_kit::ButtonKind::Ghost,
                                [&] { state.consoleOutput.clear(); },
                                false, 12.5f);
                        })
                        .build();

                    const int lines = static_cast<int>(std::count(state.consoleOutput.begin(), state.consoleOutput.end(), '\n')) + 1;
                    const float logH = std::clamp(static_cast<float>(lines) * 20.0f + 22.0f, 76.0f, 220.0f);
                    innerUi.stack("create.logWell")
                        .size(cardW, logH)
                        .content([&] {
                            innerUi.rect("create.logBg")
                                .size(cardW, logH)
                                .radius(10.0f)
                                .color(p.cardAltBg)
                                .build();

                            innerUi.text("create.logTxt")
                                .position(14.0f, 10.0f)
                                .size(cardW - 28.0f, logH - 20.0f)
                                .text(state.consoleOutput)
                                .fontFamily("monospace")
                                .fontSize(12.5f)
                                .lineHeight(19.0f)
                                .color(p.monoText)
                                .wrap(true)
                                .build();
                        })
                        .build();
                });
            }
        });
}

} // namespace fl
