#pragma once

#include "eui_neo.h"
#include "linker_types.h"

#include <algorithm>
#include <functional>
#include <string>

namespace fl::ui_kit {

struct SemanticColor {
    eui::Color fg;
    eui::Color bg;
    eui::Color border;
};

struct UiPalette {
    components::theme::ThemeColorTokens tokens;
    eui::Color canvas;
    eui::Color headerBg;
    eui::Color cardBg;
    eui::Color cardAltBg;
    eui::Color accent;
    eui::Color accentText;
    eui::Color accentSoft;
    eui::Color text;
    eui::Color mutedText;
    eui::Color subtleText;
    eui::Color border;
    eui::Color borderStrong;
    eui::Color consoleBg;
    eui::Color consoleText;
    SemanticColor info;
    SemanticColor success;
    SemanticColor warning;
    SemanticColor danger;
    core::Shadow cardShadow;
    eui::Transition anim;
};

inline eui::Color rgb255(int r, int g, int b, float a = 1.0f) {
    return {r / 255.0f, g / 255.0f, b / 255.0f, a};
}

inline core::Shadow NoShadow() {
    return core::Shadow{false, {0.0f, 0.0f}, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f}, false};
}

inline UiPalette MakePalette(bool dark) {
    UiPalette p;
    p.anim = eui::Transition::make(0.11f, eui::Ease::OutCubic);
    if (dark) {
        p.tokens = components::theme::dark();
        p.tokens.background = rgb255(11, 16, 24);
        p.tokens.primary = rgb255(0, 120, 212);
        p.tokens.surface = rgb255(19, 27, 41);
        p.tokens.surfaceHover = rgb255(28, 39, 58);
        p.tokens.surfaceActive = rgb255(37, 51, 74);
        p.tokens.text = rgb255(241, 245, 249);
        p.tokens.border = rgb255(148, 163, 184, 0.24f);

        p.canvas = p.tokens.background;
        p.headerBg = rgb255(15, 22, 34, 0.98f);
        p.cardBg = p.tokens.surface;
        p.cardAltBg = rgb255(24, 34, 52);
        p.accent = p.tokens.primary;
        p.accentText = rgb255(56, 189, 248);
        p.accentSoft = rgb255(14, 165, 233, 0.14f);
        p.text = p.tokens.text;
        p.mutedText = rgb255(168, 181, 199);
        p.subtleText = rgb255(118, 134, 156);
        p.border = p.tokens.border;
        p.borderStrong = rgb255(56, 189, 248, 0.48f);
        p.consoleBg = rgb255(8, 12, 20);
        p.consoleText = rgb255(198, 235, 255);

        p.info = {rgb255(56, 189, 248), rgb255(14, 165, 233, 0.13f), rgb255(56, 189, 248, 0.35f)};
        p.success = {rgb255(52, 211, 153), rgb255(16, 185, 129, 0.13f), rgb255(52, 211, 153, 0.35f)};
        p.warning = {rgb255(251, 191, 36), rgb255(245, 158, 11, 0.13f), rgb255(251, 191, 36, 0.36f)};
        p.danger = {rgb255(248, 113, 113), rgb255(239, 68, 68, 0.14f), rgb255(248, 113, 113, 0.38f)};
        p.cardShadow = NoShadow();
    } else {
        p.tokens = components::theme::light();
        p.tokens.background = rgb255(243, 246, 250);
        p.tokens.primary = rgb255(0, 103, 192);
        p.tokens.surface = rgb255(255, 255, 255);
        p.tokens.surfaceHover = rgb255(237, 242, 248);
        p.tokens.surfaceActive = rgb255(225, 233, 243);
        p.tokens.text = rgb255(15, 23, 42);
        p.tokens.border = rgb255(15, 23, 42, 0.14f);

        p.canvas = p.tokens.background;
        p.headerBg = rgb255(255, 255, 255, 0.98f);
        p.cardBg = p.tokens.surface;
        p.cardAltBg = rgb255(248, 250, 252);
        p.accent = p.tokens.primary;
        p.accentText = rgb255(2, 132, 199);
        p.accentSoft = rgb255(0, 103, 192, 0.10f);
        p.text = p.tokens.text;
        p.mutedText = rgb255(71, 85, 105);
        p.subtleText = rgb255(100, 116, 139);
        p.border = p.tokens.border;
        p.borderStrong = rgb255(0, 103, 192, 0.42f);
        p.consoleBg = rgb255(241, 245, 249);
        p.consoleText = rgb255(15, 23, 42);

        p.info = {rgb255(2, 132, 199), rgb255(2, 132, 199, 0.09f), rgb255(2, 132, 199, 0.28f)};
        p.success = {rgb255(5, 150, 105), rgb255(16, 185, 129, 0.10f), rgb255(5, 150, 105, 0.28f)};
        p.warning = {rgb255(180, 83, 9), rgb255(245, 158, 11, 0.10f), rgb255(180, 83, 9, 0.28f)};
        p.danger = {rgb255(220, 38, 38), rgb255(239, 68, 68, 0.09f), rgb255(220, 38, 38, 0.28f)};
        p.cardShadow = {true, {0.0f, 2.0f}, 8.0f, 0.0f, {0.06f, 0.09f, 0.16f, 0.06f}, false};
    }

    // Compensate Microsoft YaHei's tall ascender-descender EM ratio so controls and tables render crisply
    auto& typo = p.tokens.metrics.typography;
    typo.micro = 13.0f;
    typo.caption = 14.5f;
    typo.hint = 15.0f;
    typo.label = 15.5f;
    typo.option = 16.0f;
    typo.body = 16.5f;
    typo.input = 17.0f;
    typo.control = 18.0f;
    typo.cardTitle = 19.5f;
    typo.subtitle = 20.0f;
    typo.title = 22.0f;
    typo.heading = 24.5f;
    return p;
}

inline SemanticColor ToneColors(const UiPalette& p, int tone) {
    switch (tone) {
        case 1: return p.success;
        case 2: return p.warning;
        case 3: return p.danger;
        default: return p.info;
    }
}

inline void TriggerToast(AppState& state,
                         const std::string& title,
                         const std::string& message,
                         int tone = 1,
                         unsigned int icon = 0xF058u) {
    state.toastTitle = title;
    state.toastMessage = message;
    state.toastTone = tone;
    state.toastIcon = icon;
    state.toastVisible = true;
}

template <typename ComposeFn>
inline void SectionCard(eui::Ui& ui,
                        const std::string& id,
                        const UiPalette& p,
                        float width,
                        float inset,
                        float gap,
                        ComposeFn&& body) {
    const float innerW = std::max(0.0f, width - inset * 2.0f);
    components::card(ui, id)
        .theme(p.tokens)
        .width(width)
        .wrapContentHeight()
        .padding(inset)
        .color(p.cardBg)
        .radius(14.0f)
        .border(1.0f, p.border)
        .shadow(p.cardShadow)
        .content([&] {
            ui.column(id + ".col")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .gap(gap)
                .content([&] {
                    body(innerW);
                })
                .build();
        })
        .build();
}

inline void SectionHeader(eui::Ui& ui,
                          const std::string& id,
                          const UiPalette& p,
                          unsigned int iconCp,
                          const std::string& title,
                          const std::string& subtitle = {}) {
    ui.row(id)
        .width(eui::SizeValue::fill())
        .height(eui::SizeValue::wrapContent())
        .gap(12.0f)
        .alignItems(eui::Align::CENTER)
        .content([&] {
            ui.stack(id + ".badge")
                .size(34.0f, 34.0f)
                .content([&] {
                    ui.rect(id + ".badge.bg")
                        .size(34.0f, 34.0f)
                        .radius(9.0f)
                        .color(p.accentSoft)
                        .border(1.0f, p.borderStrong)
                        .build();
                    ui.text(id + ".badge.icon")
                        .size(34.0f, 34.0f)
                        .icon(iconCp)
                        .fontSize(15.5f)
                        .lineHeight(18.0f)
                        .color(p.accentText)
                        .horizontalAlign(eui::HorizontalAlign::Center)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                })
                .build();

            ui.column(id + ".titles")
                .flexGrow(1.0f)
                .height(eui::SizeValue::wrapContent())
                .gap(3.0f)
                .content([&] {
                    ui.text(id + ".title")
                        .text(title)
                        .fontSize(19.0f)
                        .lineHeight(24.0f)
                        .fontWeight(600)
                        .color(p.text)
                        .build();
                    if (!subtitle.empty()) {
                        ui.text(id + ".subtitle")
                            .text(subtitle)
                            .fontSize(14.5f)
                            .lineHeight(20.0f)
                            .color(p.mutedText)
                            .build();
                    }
                })
                .build();
        })
        .build();
}

enum class ButtonKind {
    Primary,
    Secondary,
    Subtle,
    Danger,
    InfoAccent
};

inline void ActionButton(eui::Ui& ui,
                         const std::string& id,
                         const UiPalette& p,
                         float width,
                         float height,
                         const std::string& label,
                         unsigned int iconCp,
                         ButtonKind kind,
                         std::function<void()> onClick,
                         bool disabled = false,
                         float fontSize = 15.0f) {
    eui::Color normal = p.cardAltBg;
    eui::Color hover = p.tokens.surfaceHover;
    eui::Color pressed = p.tokens.surfaceActive;
    eui::Color fg = p.text;
    eui::Color borderCol = p.border;
    float borderW = 1.0f;

    switch (kind) {
        case ButtonKind::Primary:
            normal = p.accent;
            hover = components::theme::buttonHover(p.tokens, normal);
            pressed = components::theme::buttonPressed(p.tokens, normal);
            fg = {1.0f, 1.0f, 1.0f, 1.0f};
            borderW = 0.0f;
            break;
        case ButtonKind::Secondary:
            normal = p.tokens.dark ? rgb255(28, 39, 58) : rgb255(241, 245, 249);
            hover = p.tokens.surfaceHover;
            pressed = p.tokens.surfaceActive;
            fg = p.text;
            borderCol = p.border;
            break;
        case ButtonKind::Subtle:
            normal = {0.0f, 0.0f, 0.0f, 0.0f};
            hover = p.tokens.surfaceHover;
            pressed = p.tokens.surfaceActive;
            fg = p.mutedText;
            borderCol = p.border;
            break;
        case ButtonKind::Danger:
            normal = p.danger.bg;
            hover = components::theme::withAlpha(p.danger.fg, p.tokens.dark ? 0.25f : 0.18f);
            pressed = components::theme::withAlpha(p.danger.fg, p.tokens.dark ? 0.34f : 0.25f);
            fg = p.danger.fg;
            borderCol = p.danger.border;
            break;
        case ButtonKind::InfoAccent:
            normal = p.info.bg;
            hover = components::theme::withAlpha(p.info.fg, p.tokens.dark ? 0.24f : 0.16f);
            pressed = components::theme::withAlpha(p.info.fg, p.tokens.dark ? 0.32f : 0.24f);
            fg = p.info.fg;
            borderCol = p.info.border;
            break;
    }

    const float opacity = disabled ? 0.45f : 1.0f;
    ui.stack(id)
        .size(width, height)
        .visualStateFrom(id + ".bg", 0.985f)
        .opacity(opacity)
        .content([&] {
            auto bg = ui.rect(id + ".bg")
                .size(width, height)
                .states(normal, hover, pressed)
                .radius(8.0f)
                .border(borderW, borderCol)
                .transition(p.anim)
                .disabled(disabled)
                .accessible(core::AccessibleRole::Button, label.empty() ? "Action" : label);
            if (onClick && !disabled) {
                bg.onClick(std::move(onClick));
            }
            bg.build();

            ui.row(id + ".row")
                .size(width, height)
                .gap(label.empty() || iconCp == 0 ? 0.0f : 6.0f)
                .align(eui::Align::CENTER, eui::Align::CENTER)
                .content([&] {
                    if (iconCp != 0) {
                        ui.text(id + ".icon")
                            .icon(iconCp)
                            .fontSize(fontSize * 0.94f)
                            .lineHeight(fontSize * 1.15f)
                            .color(fg)
                            .build();
                    }
                    if (!label.empty()) {
                        ui.text(id + ".text")
                            .text(label)
                            .fontSize(fontSize)
                            .lineHeight(fontSize * 1.2f)
                            .fontWeight(kind == ButtonKind::Primary ? 600 : 500)
                            .color(fg)
                            .build();
                    }
                })
                .build();
        })
        .build();
}

inline void StatusBadge(eui::Ui& ui,
                        const std::string& id,
                        const UiPalette& p,
                        float width,
                        float height,
                        const std::string& text,
                        unsigned int iconCp,
                        const SemanticColor& sem,
                        std::function<void()> onClick = {}) {
    const eui::Color hover = components::theme::withAlpha(sem.fg, p.tokens.dark ? 0.24f : 0.16f);
    const eui::Color pressed = components::theme::withAlpha(sem.fg, p.tokens.dark ? 0.32f : 0.24f);
    ui.stack(id)
        .size(width, height)
        .content([&] {
            auto bg = ui.rect(id + ".bg")
                .size(width, height)
                .radius(height * 0.5f)
                .border(1.0f, sem.border)
                .transition(p.anim);
            if (onClick) {
                bg.states(sem.bg, hover, pressed)
                  .accessible(core::AccessibleRole::Button, text)
                  .onClick(std::move(onClick));
            } else {
                bg.color(sem.bg);
            }
            bg.build();

            ui.row(id + ".row")
                .size(width, height)
                .gap(6.0f)
                .align(eui::Align::CENTER, eui::Align::CENTER)
                .content([&] {
                    if (iconCp != 0) {
                        ui.text(id + ".icon")
                            .icon(iconCp)
                            .fontSize(14.0f)
                            .lineHeight(16.0f)
                            .color(sem.fg)
                            .build();
                    }
                    ui.text(id + ".label")
                        .text(text)
                        .fontSize(14.0f)
                        .lineHeight(18.0f)
                        .fontWeight(600)
                        .color(sem.fg)
                        .build();
                })
                .build();
        })
        .build();
}

inline void BannerBox(eui::Ui& ui,
                      const std::string& id,
                      const UiPalette& p,
                      float width,
                      const SemanticColor& sem,
                      unsigned int iconCp,
                      const std::string& message,
                      const std::string& actionLabel = {},
                      std::function<void()> onAction = {}) {
    const bool hasAction = !actionLabel.empty() && static_cast<bool>(onAction);
    const float insetX = 14.0f;
    const float insetY = 12.0f;
    const float innerW = std::max(80.0f, width - insetX * 2.0f);
    const float actionW = hasAction ? 172.0f : 0.0f;
    const float msgW = std::max(60.0f, innerW - 30.0f - (hasAction ? actionW + 10.0f : 0.0f));

    components::card(ui, id)
        .theme(p.tokens)
        .width(width)
        .wrapContentHeight()
        .padding(insetY)
        .color(sem.bg)
        .radius(10.0f)
        .border(1.0f, sem.border)
        .shadow(NoShadow())
        .content([&] {
            ui.row(id + ".row")
                .width(innerW)
                .height(eui::SizeValue::wrapContent())
                .gap(10.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.text(id + ".icon")
                        .size(20.0f, 20.0f)
                        .icon(iconCp)
                        .fontSize(16.0f)
                        .lineHeight(18.0f)
                        .color(sem.fg)
                        .horizontalAlign(eui::HorizontalAlign::Center)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();

                    ui.text(id + ".msg")
                        .width(msgW)
                        .height(eui::SizeValue::wrapContent())
                        .maxWidth(msgW)
                        .wrap(true)
                        .text(message)
                        .fontSize(15.0f)
                        .lineHeight(21.0f)
                        .color(p.text)
                        .build();

                    if (hasAction) {
                        ActionButton(ui, id + ".fix", p, actionW, 34.0f,
                                     actionLabel, 0xF0E7u, ButtonKind::InfoAccent,
                                     std::move(onAction), false, 14.0f);
                    }
                })
                .build();
        })
        .build();
}

} // namespace fl::ui_kit
