#pragma once

#include "app_actions.h"
#include "eui_neo.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace fl::pages {

namespace detail {

inline std::string NormalizePastedInput(std::string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

struct LinkTypeCardSpec {
    LinkType type;
    const char* code;
    const char* title;
    const char* badge;
    const char* desc;
    unsigned int icon;
};

inline void DrawLinkTypeCard(eui::Ui& ui,
                             const std::string& id,
                             const ui_kit::UiPalette& p,
                             float width,
                             float height,
                             const LinkTypeCardSpec& spec,
                             bool selected,
                             std::function<void()> onSelect) {
    const eui::Color normalBg = selected ? p.accentSoft : p.cardAltBg;
    const eui::Color hoverBg = selected
        ? components::theme::withAlpha(p.accent, p.tokens.dark ? 0.24f : 0.16f)
        : p.tokens.surfaceHover;
    const eui::Color pressedBg = components::theme::withAlpha(p.accent, p.tokens.dark ? 0.30f : 0.22f);
    const eui::Color borderCol = selected ? p.accentText : p.border;
    const float borderW = selected ? 1.5f : 1.0f;

    ui.stack(id)
        .size(width, height)
        .visualStateFrom(id + ".bg", 0.985f)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, height)
                .states(normalBg, hoverBg, pressedBg)
                .radius(10.0f)
                .border(borderW, borderCol)
                .transition(p.anim)
                .accessible(core::AccessibleRole::Radio, spec.title, selected ? "selected" : "")
                .onClick(std::move(onSelect))
                .build();

            ui.column(id + ".col")
                .size(width, height)
                .padding(12.0f, 14.0f, 12.0f, 14.0f)
                .gap(6.0f)
                .content([&] {
                    ui.row(id + ".top")
                        .width(std::max(0.0f, width - 28.0f))
                        .height(eui::SizeValue::wrapContent())
                        .gap(8.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            ui.text(id + ".icon")
                                .icon(spec.icon)
                                .fontSize(15.5f)
                                .lineHeight(20.0f)
                                .color(selected ? p.accentText : p.mutedText)
                                .build();

                            ui.text(id + ".title")
                                .text(spec.title)
                                .fontSize(15.5f)
                                .lineHeight(20.0f)
                                .fontWeight(600)
                                .color(selected ? p.text : p.text)
                                .build();

                            ui.row(id + ".spacer").flexGrow(1.0f).height(1.0f).content([] {}).build();

                            ui.stack(id + ".pill")
                                .size(68.0f, 22.0f)
                                .content([&] {
                                    ui.rect(id + ".pill.bg")
                                        .size(68.0f, 22.0f)
                                        .radius(11.0f)
                                        .color(selected ? p.accent : p.tokens.surfaceHover)
                                        .build();
                                    ui.text(id + ".pill.txt")
                                        .size(68.0f, 22.0f)
                                        .text(spec.badge)
                                        .fontSize(12.5f)
                                        .lineHeight(15.0f)
                                        .fontWeight(600)
                                        .color(selected ? eui::Color{1, 1, 1, 1} : p.mutedText)
                                        .horizontalAlign(eui::HorizontalAlign::Center)
                                        .verticalAlign(eui::VerticalAlign::Center)
                                        .build();
                                })
                                .build();
                        })
                        .build();

                    ui.text(id + ".desc")
                        .width(std::max(0.0f, width - 28.0f))
                        .height(eui::SizeValue::wrapContent())
                        .maxWidth(std::max(0.0f, width - 28.0f))
                        .wrap(true)
                        .text(spec.desc)
                        .fontSize(14.0f)
                        .lineHeight(19.5f)
                        .color(p.mutedText)
                        .build();
                })
                .build();
        })
        .build();
}

inline void DrawLeftColumn(eui::Ui& ui, AppState& state, const ui_kit::UiPalette& p, float colW) {
    ui.column("create.left")
        .width(colW)
        .height(eui::SizeValue::wrapContent())
        .gap(14.0f)
        .content([&] {
            // 1. Mode & Link Type Card
            ui_kit::SectionCard(ui, "create.typeCard", p, colW, 18.0f, 12.0f, [&](float innerW) {
                ui_kit::SectionHeader(ui, "create.typeHdr", p, 0xF0C1u,
                                      "工作模式与链接类型",
                                      "支持全部 4 种 Windows NTFS 链接及安全“搬家并建链”");

                components::segmented(ui, "create.opMode")
                    .theme(p.tokens)
                    .size(innerW, 40.0f)
                    .items({"标准创建链接 (Create)", "目录搬家并建链 (Move & Link)"})
                    .selected(state.operationMode == OperationMode::MoveAndLink ? 1 : 0)
                    .fontSize(14.5f)
                    .transition(p.anim)
                    .onChange([&](int idx) {
                        state.operationMode = (idx == 1) ? OperationMode::MoveAndLink : OperationMode::CreateOnly;
                        if (state.operationMode == OperationMode::MoveAndLink &&
                            !win::IsDirectoryTargetRequired(state.linkType)) {
                            state.linkType = LinkType::Junction;
                        }
                        actions::MarkValidationDirty(state);
                    })
                    .build();

                const LinkTypeCardSpec specs[4] = {
                    {LinkType::Junction, "/J", "目录联接 (/J)", "免提权",
                     "本地 NTFS 目录重定向，免管理员权限，适合游戏/缓存目录迁移。", 0xF07Bu},
                    {LinkType::Directory, "/D", "目录符号链接 (/D)", "支持跨网",
                     "支持 UNC 网络路径与相对路径，需管理员或开发者模式。", 0xF07Cu},
                    {LinkType::HardLink, "/H", "文件硬链接 (/H)", "零开销",
                     "同分区文件共享数据块，删其一不影响实体内容，极稳固。", 0xF0C1u},
                    {LinkType::FileSymlink, "FILE", "文件符号链接", "单文件",
                     "跨磁盘或网络指向单个文件，类似快捷方式但透明重定向。", 0xF15Bu}
                };

                const float cardGap = 10.0f;
                const float cardW = std::max(140.0f, (innerW - cardGap) * 0.5f);
                const float cardH = 98.0f;

                ui.row("create.types.r0")
                    .width(innerW)
                    .height(cardH)
                    .gap(cardGap)
                    .content([&] {
                        for (int i = 0; i < 2; ++i) {
                            const auto& s = specs[i];
                            DrawLinkTypeCard(ui, "create.type." + std::to_string(i), p, cardW, cardH, s,
                                             state.linkType == s.type, [&state, t = s.type] {
                                                 state.linkType = t;
                                                 actions::MarkValidationDirty(state);
                                                 actions::PersistSettings(state);
                                             });
                        }
                    })
                    .build();

                ui.row("create.types.r1")
                    .width(innerW)
                    .height(cardH)
                    .gap(cardGap)
                    .content([&] {
                        for (int i = 2; i < 4; ++i) {
                            const auto& s = specs[i];
                            DrawLinkTypeCard(ui, "create.type." + std::to_string(i), p, cardW, cardH, s,
                                             state.linkType == s.type, [&state, t = s.type] {
                                                 state.linkType = t;
                                                 actions::MarkValidationDirty(state);
                                                 actions::PersistSettings(state);
                                             });
                        }
                    })
                    .build();
            });

            // 2. Smart Path Configuration Card
            ui_kit::SectionCard(ui, "create.pathCard", p, colW, 18.0f, 12.0f, [&](float innerW) {
                const bool isMoveMode = (state.operationMode == OperationMode::MoveAndLink);
                const bool wantsDir = win::IsDirectoryTargetRequired(state.linkType);
                ui_kit::SectionHeader(ui, "create.pathHdr", p, 0xF07Bu,
                                      isMoveMode ? "目录搬家路径配置" : "路径参数配置",
                                      isMoveMode
                                          ? "把已有的庞大文件夹迁到其他盘，并在原路径无缝建立链接"
                                          : "支持直接粘贴带引号的 Windows Explorer 路径（自动去除双引号）");

                // Link Path
                ui.text("create.linkLabel")
                    .text(isMoveMode
                              ? "现有源目录（将被整体迁移走，并在原地建链）"
                              : "链接路径 Link（即将新生成的虚拟入口，当前应不存在）")
                    .fontSize(15.0f)
                    .lineHeight(20.0f)
                    .fontWeight(600)
                    .color(p.text)
                    .build();

                const float btnW = 78.0f;
                const float btnGap = 6.0f;
                const float inputW = std::max(140.0f, innerW - btnW * 2.0f - btnGap * 2.0f);

                ui.row("create.linkRow")
                    .width(innerW)
                    .height(42.0f)
                    .gap(btnGap)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        components::input(ui, "create.linkInput")
                            .theme(p.tokens)
                            .size(inputW, 42.0f)
                            .fontSize(15.5f)
                            .value(state.linkPath)
                            .placeholder(isMoveMode
                                             ? "例如 C:\\Users\\Me\\AppData\\Local\\LargeApp"
                                             : "例如 C:\\Work\\ShortcutLink")
                            .onChange([&](const std::string& v) {
                                state.linkPath = NormalizePastedInput(v);
                                actions::MarkValidationDirty(state);
                            })
                            .onEnter([&] {
                                actions::ExecuteOrConfirmCreate(state);
                            })
                            .build();

                        ui_kit::ActionButton(ui, "create.linkPick", p, btnW, 42.0f,
                                             isMoveMode ? "选目录" : "选位置",
                                             isMoveMode ? 0xF07Bu : 0xF067u,
                                             ui_kit::ButtonKind::Secondary,
                                             [&] {
                                                 if (isMoveMode) {
                                                     auto chosen = win::BrowseDirectory("选择要迁移的现有源文件夹", state.linkPath);
                                                     if (!chosen.empty()) {
                                                         state.linkPath = win::CleanPathInput(chosen);
                                                         actions::MarkValidationDirty(state);
                                                     }
                                                 } else {
                                                     std::string chosen;
                                                     if (win::ChooseNewLinkPath(chosen, wantsDir)) {
                                                         state.linkPath = win::CleanPathInput(chosen);
                                                         actions::MarkValidationDirty(state);
                                                     }
                                                 }
                                             },
                                             false, 14.0f);

                        ui_kit::ActionButton(ui, "create.linkPaste", p, btnW, 42.0f,
                                             "粘贴", 0xF0EAu, ui_kit::ButtonKind::Secondary,
                                             [&] {
                                                 const std::string clip = win::CleanPathInput(win::ReadTextFromClipboard());
                                                 if (!clip.empty()) {
                                                     state.linkPath = clip;
                                                     actions::MarkValidationDirty(state);
                                                 }
                                             },
                                             false, 14.0f);
                    })
                    .build();

                // Utility row between Link & Target
                ui.row("create.midTools")
                    .width(innerW)
                    .height(34.0f)
                    .gap(8.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        ui_kit::ActionButton(ui, "create.swapBtn", p, 110.0f, 34.0f,
                                             "互换路径", 0xF362u, ui_kit::ButtonKind::Subtle,
                                             [&] {
                                                 std::swap(state.linkPath, state.targetPath);
                                                 actions::MarkValidationDirty(state);
                                             },
                                             false, 13.5f);

                        ui_kit::ActionButton(ui, "create.appendBtn", p, 138.0f, 34.0f,
                                             isMoveMode ? "目标追加原名" : "链接追加目标名",
                                             0xF067u, ui_kit::ButtonKind::Subtle,
                                             [&] {
                                                 actions::ApplyQuickFix(state, isMoveMode
                                                     ? FixAction::AppendLinkFolderNameToTarget
                                                     : FixAction::AppendTargetNameToLink);
                                             },
                                             false, 13.5f);

                        ui.row("create.midTools.sp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                        components::toggleSwitch(ui, "create.autoParent")
                            .theme(p.tokens)
                            .size(182.0f, 30.0f)
                            .checked(state.autoCreateParent)
                            .text("自动创建父目录")
                            .fontSize(14.5f)
                            .transition(p.anim)
                            .onChange([&](bool v) {
                                state.autoCreateParent = v;
                                actions::MarkValidationDirty(state);
                                actions::PersistSettings(state);
                            })
                            .build();
                    })
                    .build();

                // Target Path
                ui.text("create.targetLabel")
                    .text(isMoveMode
                              ? "迁入新目标（Target — 迁移后的最终保存路径，应尚不存在）"
                              : "真实目标 Target（数据实际所在的现有文件或文件夹）")
                    .fontSize(15.0f)
                    .lineHeight(20.0f)
                    .fontWeight(600)
                    .color(p.text)
                    .build();

                const float tBtnW = 70.0f;
                const float tInputW = std::max(130.0f, innerW - tBtnW * 3.0f - btnGap * 3.0f);
                ui.row("create.targetRow")
                    .width(innerW)
                    .height(42.0f)
                    .gap(btnGap)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        components::input(ui, "create.targetInput")
                            .theme(p.tokens)
                            .size(tInputW, 42.0f)
                            .fontSize(15.5f)
                            .value(state.targetPath)
                            .placeholder(isMoveMode
                                             ? "例如 D:\\MovedApps\\LargeApp"
                                             : "例如 D:\\RealStorage\\Assets")
                            .onChange([&](const std::string& v) {
                                state.targetPath = NormalizePastedInput(v);
                                actions::MarkValidationDirty(state);
                            })
                            .onEnter([&] {
                                actions::ExecuteOrConfirmCreate(state);
                            })
                            .build();

                        ui_kit::ActionButton(ui, "create.targetDirPick", p, tBtnW, 42.0f,
                                             isMoveMode ? "选母盘" : "文件夹",
                                             0xF07Cu, ui_kit::ButtonKind::Secondary,
                                             [&] {
                                                 auto chosen = win::BrowseDirectory("选择目录", state.targetPath);
                                                 if (!chosen.empty()) {
                                                     std::string clean = win::CleanPathInput(chosen);
                                                     if (isMoveMode && !state.linkPath.empty()) {
                                                         const auto srcName = std::filesystem::path(win::CleanPathInputW(state.linkPath)).filename();
                                                         if (!srcName.empty()) {
                                                             clean = win::WideToUtf8((std::filesystem::path(win::Utf8ToWide(clean)) / srcName).wstring());
                                                         }
                                                     }
                                                     state.targetPath = std::move(clean);
                                                     actions::MarkValidationDirty(state);
                                                 }
                                             },
                                             false, 13.5f);

                        ui_kit::ActionButton(ui, "create.targetFilePick", p, tBtnW, 42.0f,
                                             isMoveMode ? "选新址" : "选文件",
                                             0xF15Bu, ui_kit::ButtonKind::Secondary,
                                             [&] {
                                                 if (isMoveMode) {
                                                     std::string chosen;
                                                     if (win::ChooseNewLinkPath(chosen, true)) {
                                                         state.targetPath = win::CleanPathInput(chosen);
                                                         actions::MarkValidationDirty(state);
                                                     }
                                                 } else {
                                                     auto chosen = win::BrowseFile("选择目标文件", state.targetPath);
                                                     if (!chosen.empty()) {
                                                         state.targetPath = win::CleanPathInput(chosen);
                                                         actions::MarkValidationDirty(state);
                                                     }
                                                 }
                                             },
                                             false, 13.5f);

                        ui_kit::ActionButton(ui, "create.targetPaste", p, tBtnW, 42.0f,
                                             "粘贴", 0xF0EAu, ui_kit::ButtonKind::Secondary,
                                             [&] {
                                                 const std::string clip = win::CleanPathInput(win::ReadTextFromClipboard());
                                                 if (!clip.empty()) {
                                                     state.targetPath = clip;
                                                     actions::MarkValidationDirty(state);
                                                 }
                                             },
                                             false, 13.5f);
                    })
                    .build();
            });
        })
        .build();
}

inline void DrawRightColumn(eui::Ui& ui, AppState& state, const ui_kit::UiPalette& p, float colW) {
    const ValidationResult& val = actions::EnsureValidation(state);
    const bool busy = app::async::running("fl.create");

    int valTone = 0;
    unsigned int valIcon = 0xF05Au;
    switch (val.level) {
        case ValidationLevel::Valid:   valTone = 1; valIcon = 0xF058u; break;
        case ValidationLevel::Warning: valTone = 2; valIcon = 0xF071u; break;
        case ValidationLevel::Error:   valTone = 3; valIcon = 0xF057u; break;
        default:                       valTone = 0; valIcon = 0xF05Au; break;
    }

    const std::string cmdPreview = (state.operationMode == OperationMode::MoveAndLink)
        ? "[1] 迁移源目录 -> \"" + (state.targetPath.empty() ? "<新目标路径>" : win::CleanPathInput(state.targetPath)) + "\"\n[2] " +
          win::BuildMklinkCommand(state.linkType, state.linkPath, state.targetPath)
        : win::BuildMklinkCommand(state.linkType, state.linkPath, state.targetPath);

    ui.column("create.right")
        .width(colW)
        .height(eui::SizeValue::wrapContent())
        .gap(14.0f)
        .content([&] {
            // 3. Preview & Execution Card
            ui_kit::SectionCard(ui, "create.execCard", p, colW, 18.0f, 12.0f, [&](float innerW) {
                ui_kit::SectionHeader(ui, "create.execHdr", p, 0xF120u,
                                      "实时校验与命令预览",
                                      "自动检测盘符分区、文件系统类型及管理员权限要求");

                ui_kit::BannerBox(ui, "create.valBanner", p, innerW,
                                  ui_kit::ToneColors(p, valTone), valIcon, val.message,
                                  val.quickFixLabel,
                                  val.quickFix != FixAction::None
                                      ? std::function<void()>([&state, fix = val.quickFix] {
                                            actions::ApplyQuickFix(state, fix);
                                        })
                                      : std::function<void()>{});

                // Monospace command preview box
                components::card(ui, "create.cmdBox")
                    .theme(p.tokens)
                    .width(innerW)
                    .wrapContentHeight()
                    .padding(12.0f)
                    .color(p.consoleBg)
                    .radius(10.0f)
                    .border(1.0f, p.border)
                    .shadow(ui_kit::NoShadow())
                    .content([&] {
                        const float cmdInnerW = std::max(80.0f, innerW - 24.0f);
                        ui.column("create.cmdBox.col")
                            .width(cmdInnerW)
                            .height(eui::SizeValue::wrapContent())
                            .gap(8.0f)
                            .content([&] {
                                ui.row("create.cmdBox.top")
                                    .width(cmdInnerW)
                                    .height(28.0f)
                                    .alignItems(eui::Align::CENTER)
                                    .content([&] {
                                        ui.text("create.cmdBox.lbl")
                                            .text("GENERATED COMMAND")
                                            .fontSize(13.0f)
                                            .lineHeight(16.0f)
                                            .fontWeight(700)
                                            .color(p.subtleText)
                                            .build();
                                        ui.row("create.cmdBox.sp").flexGrow(1.0f).height(1.0f).content([] {}).build();
                                        ui_kit::ActionButton(ui, "create.copyCmd", p, 96.0f, 28.0f,
                                                             "复制命令", 0xF0C5u, ui_kit::ButtonKind::Subtle,
                                                             [&] {
                                                                 if (win::CopyTextToClipboard(win::BuildMklinkCommand(state.linkType, state.linkPath, state.targetPath))) {
                                                                     ui_kit::TriggerToast(state, "已复制到剪贴板", "mklink 命令行已准备就绪。", 1, 0xF0C5u);
                                                                 }
                                                             },
                                                             false, 13.0f);
                                    })
                                    .build();

                                ui.text("create.cmdText")
                                    .width(cmdInnerW)
                                    .height(eui::SizeValue::wrapContent())
                                    .maxWidth(cmdInnerW)
                                    .wrap(true)
                                    .fontFamily("monospace")
                                    .fontSize(14.5f)
                                    .lineHeight(21.0f)
                                    .color(p.consoleText)
                                    .text(cmdPreview)
                                    .build();
                            })
                            .build();
                    })
                    .build();

                // Primary Action Row
                const float clearW = 114.0f;
                const float execW = std::max(160.0f, innerW - clearW - 10.0f);
                const std::string primaryLabel = busy
                    ? "正在执行..."
                    : (state.operationMode == OperationMode::MoveAndLink
                           ? "确认搬家并建立链接"
                           : "立即创建系统链接");

                ui.row("create.actionRow")
                    .width(innerW)
                    .height(46.0f)
                    .gap(10.0f)
                    .content([&] {
                        ui_kit::ActionButton(ui, "create.executeBtn", p, execW, 46.0f,
                                             primaryLabel, busy ? 0xF021u : 0xF04Bu,
                                             ui_kit::ButtonKind::Primary,
                                             [&] { actions::ExecuteOrConfirmCreate(state); },
                                             !val.canExecute || busy, 15.5f);

                        ui_kit::ActionButton(ui, "create.resetBtn", p, clearW, 46.0f,
                                             "重置输入", 0xF021u,
                                             ui_kit::ButtonKind::Secondary,
                                             [&] {
                                                 state.linkPath.clear();
                                                 state.targetPath.clear();
                                                 state.statusText = "就绪";
                                                 state.statusTone = 0;
                                                 state.consoleOutput.clear();
                                                 actions::MarkValidationDirty(state);
                                             },
                                             busy, 14.5f);
                    })
                    .build();
            });

            // 4. Console & Quick Jump Card
            ui_kit::SectionCard(ui, "create.outCard", p, colW, 18.0f, 12.0f, [&](float innerW) {
                ui.row("create.outTop")
                    .width(innerW)
                    .height(eui::SizeValue::wrapContent())
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        ui.text("create.outTitle")
                            .text("执行状态与终端回显")
                            .fontSize(17.5f)
                            .lineHeight(22.0f)
                            .fontWeight(600)
                            .color(p.text)
                            .build();
                        ui.row("create.outSp").flexGrow(1.0f).height(1.0f).content([] {}).build();
                        ui_kit::StatusBadge(ui, "create.statusBadge", p, 152.0f, 28.0f,
                                            state.statusText,
                                            state.statusTone == 1 ? 0xF058u : (state.statusTone == 3 ? 0xF057u : 0xF05Au),
                                            ui_kit::ToneColors(p, state.statusTone));
                    })
                    .build();

                const std::string outputDisplay = state.consoleOutput.empty()
                    ? "准备就绪：点击“立即创建系统链接”后，此处将显示 Windows 命令原始输出。"
                    : state.consoleOutput;

                ui.stack("create.consoleBox")
                    .width(innerW)
                    .height(164.0f)
                    .content([&] {
                        ui.rect("create.consoleBox.bg")
                            .size(innerW, 164.0f)
                            .color(p.consoleBg)
                            .radius(10.0f)
                            .border(1.0f, p.border)
                            .build();
                        ui.text("create.consoleText")
                            .x(12.0f)
                            .y(10.0f)
                            .size(std::max(80.0f, innerW - 24.0f), 144.0f)
                            .maxWidth(std::max(80.0f, innerW - 24.0f))
                            .wrap(true)
                            .fontFamily("monospace")
                            .fontSize(14.5f)
                            .lineHeight(20.5f)
                            .color(state.consoleOutput.empty() ? p.subtleText : p.consoleText)
                            .text(outputDisplay)
                            .build();
                    })
                    .build();

                const bool hasLink = !win::CleanPathInput(state.linkPath).empty();
                ui.row("create.quickFooter")
                    .width(innerW)
                    .height(36.0f)
                    .gap(8.0f)
                    .content([&] {
                        ui_kit::ActionButton(ui, "create.openLinkDir", p, 148.0f, 36.0f,
                                             "在资源管理器定位", 0xF07Cu,
                                             ui_kit::ButtonKind::Secondary,
                                             [&] { win::RevealInExplorer(state.linkPath); },
                                             !hasLink, 13.5f);

                        ui_kit::ActionButton(ui, "create.inspectJump", p, 132.0f, 36.0f,
                                             "去体检该链接", 0xF002u,
                                             ui_kit::ButtonKind::InfoAccent,
                                             [&] {
                                                 actions::InspectPathNow(state, state.linkPath);
                                                 state.activeTab = 1;
                                             },
                                             !hasLink, 13.5f);

                        ui.row("create.qfSp").flexGrow(1.0f).height(1.0f).content([] {}).build();

                        ui_kit::ActionButton(ui, "create.copyOut", p, 96.0f, 36.0f,
                                             "复制回显", 0xF0C5u,
                                             ui_kit::ButtonKind::Subtle,
                                             [&] {
                                                 if (win::CopyTextToClipboard(outputDisplay)) {
                                                     ui_kit::TriggerToast(state, "已复制", "终端输出内容已复制到剪贴板。", 1, 0xF0C5u);
                                                 }
                                             },
                                             state.consoleOutput.empty(), 13.5f);
                    })
                    .build();
            });
        })
        .build();
}

} // namespace detail

inline void ComposeCreatePage(eui::Ui& ui,
                              AppState& state,
                              const ui_kit::UiPalette& p,
                              float width,
                              float height) {
    const ValidationResult& val = actions::EnsureValidation(state);
    const std::string scrollKey =
        std::to_string(static_cast<int>(state.operationMode)) + ":" +
        std::to_string(static_cast<int>(val.level)) + ":" +
        std::to_string(static_cast<int>(val.quickFix)) + ":" +
        std::to_string(val.message.size()) + ":" +
        std::to_string(state.linkPath.size()) + ":" +
        std::to_string(state.targetPath.size()) + ":" +
        std::to_string(state.consoleOutput.size());

    components::scrollView(ui, "page.create.scroll")
        .theme(p.tokens)
        .size(width, height)
        .offset(state.createScrollOffset)
        .step(64.0f)
        .gap(16.0f)
        .transition(0.08f, eui::Ease::OutCubic)
        .contentKey(scrollKey)
        .onChange([&](float offset) { state.createScrollOffset = offset; })
        .content([&](eui::Ui& innerUi, float innerW, float /*viewportH*/) {
            const bool twoCols = innerW >= 980.0f;
            if (twoCols) {
                const float gap = 16.0f;
                const float leftW = std::floor((innerW - gap) * 0.53f);
                const float rightW = std::max(260.0f, innerW - gap - leftW);
                innerUi.row("create.twoCols")
                    .width(innerW)
                    .height(eui::SizeValue::wrapContent())
                    .gap(gap)
                    .alignItems(eui::Align::START)
                    .content([&] {
                        detail::DrawLeftColumn(innerUi, state, p, leftW);
                        detail::DrawRightColumn(innerUi, state, p, rightW);
                    })
                    .build();
            } else {
                innerUi.column("create.oneCol")
                    .width(innerW)
                    .height(eui::SizeValue::wrapContent())
                    .gap(14.0f)
                    .content([&] {
                        detail::DrawLeftColumn(innerUi, state, p, innerW);
                        detail::DrawRightColumn(innerUi, state, p, innerW);
                    })
                    .build();
            }
        })
        .build();
}

} // namespace fl::pages
