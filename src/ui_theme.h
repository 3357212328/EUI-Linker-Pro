#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "components/button.h"
#include "components/card.h"
#include "components/scrollview.h"
#include "components/theme.h"
#include "components/toast.h"
#include "core/dsl.h"
#include "linker_types.h"

namespace fl::ui_kit {

struct SemanticColor {
    eui::Color fg;
    eui::Color bg;
    eui::Color border;
};

inline constexpr eui::Color kTransparent{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr eui::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};

inline constexpr eui::Color Rgba(float r, float g, float b, float a = 1.0f) {
    return eui::Color(r, g, b, a);
}

struct UiPalette {
    bool dark = false;
    components::theme::ThemeColorTokens tokens;
    eui::Transition anim = eui::Transition::make(0.18f, eui::Ease::OutCubic);
    eui::Transition slowAnim = eui::Transition::make(0.24f, eui::Ease::OutExpo);

    // Core canvas & surface hierarchy
    eui::Color windowBg;
    eui::Color sidebarBg;
    eui::Color cardBg;
    eui::Color cardAltBg;
    eui::Color pillBg;
    eui::Color pillHover;
    eui::Color pillActive;
    eui::Color navHover;
    eui::Color navActive;

    // Hairlines & shadows
    eui::Color border;
    eui::Color hairline;
    eui::Color hairlineStrong;
    eui::Color shadow;
    eui::Color accentShadow;

    // Typography hierarchy
    eui::Color ink;
    eui::Color secondaryText;
    eui::Color mutedText;
    eui::Color faintText;
    eui::Color monoText;

    // Accent
    eui::Color accent;
    eui::Color accentHover;
    eui::Color accentPressed;
    eui::Color accentSoft;
    eui::Color accentBorder;

    // Semantic feedback
    SemanticColor info;
    SemanticColor success;
    SemanticColor warning;
    SemanticColor danger;
};

inline UiPalette MakePalette(bool dark) {
    UiPalette p;
    p.dark = dark;
    p.tokens = dark ? components::theme::dark() : components::theme::light();

    if (dark) {
        p.windowBg       = Rgba(0.047f, 0.059f, 0.078f);       // #0C0F14 Deep graphite canvas
        p.sidebarBg      = Rgba(0.067f, 0.082f, 0.110f);       // #11151C Elevated rail
        p.cardBg         = Rgba(0.086f, 0.106f, 0.141f);       // #161B24 Studio card
        p.cardAltBg      = Rgba(0.110f, 0.133f, 0.176f);       // #1C222D Inset / well
        p.pillBg         = Rgba(0.133f, 0.161f, 0.212f);       // #222936 Soft pill
        p.pillHover      = Rgba(0.169f, 0.204f, 0.267f);       // #2B3444
        p.pillActive     = Rgba(0.204f, 0.247f, 0.322f);       // #343F52
        p.navHover       = Rgba(1.0f, 1.0f, 1.0f, 0.055f);
        p.navActive      = Rgba(0.231f, 0.510f, 0.965f, 0.16f);

        p.border         = Rgba(1.0f, 1.0f, 1.0f, 0.085f);
        p.hairline       = Rgba(1.0f, 1.0f, 1.0f, 0.070f);
        p.hairlineStrong = Rgba(1.0f, 1.0f, 1.0f, 0.135f);
        p.shadow         = Rgba(0.0f, 0.0f, 0.0f, 0.36f);
        p.accentShadow   = Rgba(0.145f, 0.388f, 0.922f, 0.36f);

        p.ink            = Rgba(0.945f, 0.961f, 0.976f);       // #F1F5F9
        p.secondaryText  = Rgba(0.675f, 0.725f, 0.800f);       // #ACB9CC
        p.mutedText      = Rgba(0.494f, 0.549f, 0.635f);       // #7E8CA2
        p.faintText      = Rgba(0.353f, 0.400f, 0.478f);       // #5A667A
        p.monoText       = Rgba(0.847f, 0.898f, 0.969f);       // #D8E5F7

        p.accent         = Rgba(0.231f, 0.510f, 0.965f);       // #3B82F6
        p.accentHover    = Rgba(0.310f, 0.569f, 0.980f);       // #4F91FA
        p.accentPressed  = Rgba(0.145f, 0.388f, 0.922f);       // #2563EB
        p.accentSoft     = Rgba(0.231f, 0.510f, 0.965f, 0.16f);
        p.accentBorder   = Rgba(0.231f, 0.510f, 0.965f, 0.34f);

        p.info    = { Rgba(0.420f, 0.725f, 0.988f),
                      Rgba(0.231f, 0.510f, 0.965f, 0.14f),
                      Rgba(0.231f, 0.510f, 0.965f, 0.28f) };
        p.success = { Rgba(0.290f, 0.871f, 0.502f),
                      Rgba(0.133f, 0.773f, 0.369f, 0.14f),
                      Rgba(0.133f, 0.773f, 0.369f, 0.28f) };
        p.warning = { Rgba(0.984f, 0.749f, 0.286f),
                      Rgba(0.961f, 0.620f, 0.043f, 0.15f),
                      Rgba(0.961f, 0.620f, 0.043f, 0.30f) };
        p.danger  = { Rgba(0.988f, 0.525f, 0.525f),
                      Rgba(0.937f, 0.267f, 0.267f, 0.15f),
                      Rgba(0.937f, 0.267f, 0.267f, 0.30f) };
    } else {
        p.windowBg       = Rgba(0.961f, 0.969f, 0.980f);       // #F5F7FA Clean daylight canvas
        p.sidebarBg      = Rgba(1.000f, 1.000f, 1.000f);       // #FFFFFF Pure crisp sidebar
        p.cardBg         = Rgba(1.000f, 1.000f, 1.000f);       // #FFFFFF Elevated card
        p.cardAltBg      = Rgba(0.945f, 0.957f, 0.973f);       // #F1F4F8 Inset / code bar
        p.pillBg         = Rgba(0.933f, 0.949f, 0.969f);       // #EEF2F7 Soft pill
        p.pillHover      = Rgba(0.890f, 0.914f, 0.949f);       // #E3E9F2
        p.pillActive     = Rgba(0.843f, 0.878f, 0.925f);       // #D7E0EC
        p.navHover       = Rgba(0.059f, 0.090f, 0.165f, 0.045f);
        p.navActive      = Rgba(0.145f, 0.388f, 0.922f, 0.095f);

        p.border         = Rgba(0.059f, 0.090f, 0.165f, 0.085f);
        p.hairline       = Rgba(0.059f, 0.090f, 0.165f, 0.068f);
        p.hairlineStrong = Rgba(0.059f, 0.090f, 0.165f, 0.135f);
        p.shadow         = Rgba(0.059f, 0.090f, 0.165f, 0.065f);
        p.accentShadow   = Rgba(0.145f, 0.388f, 0.922f, 0.25f);

        p.ink            = Rgba(0.059f, 0.090f, 0.165f);       // #0F172A
        p.secondaryText  = Rgba(0.278f, 0.337f, 0.412f);       // #475569
        p.mutedText      = Rgba(0.392f, 0.455f, 0.545f);       // #64748B
        p.faintText      = Rgba(0.580f, 0.639f, 0.722f);       // #94A3B8
        p.monoText       = Rgba(0.118f, 0.161f, 0.231f);       // #1E293B

        p.accent         = Rgba(0.145f, 0.388f, 0.922f);       // #2563EB
        p.accentHover    = Rgba(0.114f, 0.306f, 0.847f);       // #1D4ED8
        p.accentPressed  = Rgba(0.118f, 0.251f, 0.686f);       // #1E40AF
        p.accentSoft     = Rgba(0.145f, 0.388f, 0.922f, 0.095f);
        p.accentBorder   = Rgba(0.145f, 0.388f, 0.922f, 0.24f);

        p.info    = { Rgba(0.098f, 0.349f, 0.769f),
                      Rgba(0.145f, 0.388f, 0.922f, 0.09f),
                      Rgba(0.145f, 0.388f, 0.922f, 0.22f) };
        p.success = { Rgba(0.086f, 0.486f, 0.239f),
                      Rgba(0.086f, 0.639f, 0.290f, 0.10f),
                      Rgba(0.086f, 0.639f, 0.290f, 0.24f) };
        p.warning = { Rgba(0.690f, 0.369f, 0.039f),
                      Rgba(0.961f, 0.620f, 0.043f, 0.12f),
                      Rgba(0.961f, 0.620f, 0.043f, 0.28f) };
        p.danger  = { Rgba(0.769f, 0.149f, 0.149f),
                      Rgba(0.863f, 0.149f, 0.149f, 0.10f),
                      Rgba(0.863f, 0.149f, 0.149f, 0.25f) };
    }

    // Harmonize EUI-NEO built-in ThemeColorTokens with our studio palette
    p.tokens.dark          = dark;
    p.tokens.background    = p.windowBg;
    p.tokens.primary       = p.accent;
    p.tokens.surface       = p.cardBg;
    p.tokens.surfaceHover  = p.pillBg;
    p.tokens.surfaceActive = p.pillHover;
    p.tokens.text          = p.ink;
    p.tokens.border        = p.hairlineStrong;

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
                         int tone = 0,
                         uint32_t icon = 0xF05Au) {
    state.toastTitle = title;
    state.toastMessage = message;
    state.toastTone = tone;
    state.toastIcon = icon;
    state.toastVisible = true;
}

inline float CenteredWidth(float availW, float maxW = 840.0f, float minPad = 32.0f) {
    const float usable = std::max(320.0f, availW - minPad * 2.0f);
    return std::min(maxW, usable);
}

enum class ButtonKind {
    Primary,
    Secondary,
    Ghost,
    Subtle, // Alias for Ghost
    Danger,
    InfoAccent
};

// Modern EUI-NEO pill/capsule button with crisp centering at any width.
inline void ActionButton(eui::Ui& ui,
                         const std::string& id,
                         const UiPalette& p,
                         float width,
                         float height,
                         const std::string& label,
                         uint32_t iconCp,
                         ButtonKind kind,
                         const std::function<void()>& onClick,
                         bool disabled = false,
                         float fontSize = 13.5f) {
    eui::Color bg;
    eui::Color bgHover;
    eui::Color bgPress;
    eui::Color fg;
    eui::Color borderCol = kTransparent;
    float borderW = 0.0f;
    float shadowBlur = 0.0f;
    float shadowDy = 0.0f;
    eui::Color shadowCol = kTransparent;

    switch (kind) {
        case ButtonKind::Primary:
            bg = p.accent;
            bgHover = p.accentHover;
            bgPress = p.accentPressed;
            fg = kWhite;
            shadowBlur = 14.0f;
            shadowDy = 4.0f;
            shadowCol = p.accentShadow;
            break;
        case ButtonKind::Secondary:
            bg = p.pillBg;
            bgHover = p.pillHover;
            bgPress = p.pillActive;
            fg = p.ink;
            borderW = 1.0f;
            borderCol = p.hairline;
            break;
        case ButtonKind::Danger:
            bg = p.danger.bg;
            bgHover = Rgba(p.danger.fg.r, p.danger.fg.g, p.danger.fg.b, p.dark ? 0.24f : 0.16f);
            bgPress = Rgba(p.danger.fg.r, p.danger.fg.g, p.danger.fg.b, p.dark ? 0.30f : 0.22f);
            fg = p.danger.fg;
            borderW = 1.0f;
            borderCol = p.danger.border;
            break;
        case ButtonKind::InfoAccent:
            bg = p.accentSoft;
            bgHover = Rgba(p.accent.r, p.accent.g, p.accent.b, p.dark ? 0.24f : 0.15f);
            bgPress = Rgba(p.accent.r, p.accent.g, p.accent.b, p.dark ? 0.30f : 0.21f);
            fg = p.accent;
            break;
        case ButtonKind::Ghost:
        case ButtonKind::Subtle:
            bg = kTransparent;
            bgHover = p.navHover;
            bgPress = p.pillBg;
            fg = p.secondaryText;
            break;
    }

    if (disabled) {
        fg = p.faintText;
        bg = (kind == ButtonKind::Ghost || kind == ButtonKind::Subtle) ? kTransparent : p.cardAltBg;
        bgHover = bg;
        bgPress = bg;
        borderCol = p.hairline;
        borderW = (kind == ButtonKind::Ghost || kind == ButtonKind::Subtle) ? 0.0f : 1.0f;
        shadowBlur = 0.0f;
    }

    auto btn = ui.stack(id)
        .size(width, height)
        .visualStateFrom(id, disabled ? 1.0f : 0.97f)
        .transition(p.anim);
    if (!disabled && onClick) {
        btn.onClick(onClick);
    }

    btn.content([&] {
        auto box = ui.rect(id + ".bg")
            .size(width, height)
            .radius(height >= 42.0f ? 11.0f : 9.0f)
            .color(bg)
            .border(borderW, borderCol)
            .hoverColor(bgHover)
            .pressedColor(bgPress)
            .visualStateFrom(id, 1.0f)
            .transition(p.anim)
            .animate(eui::AnimProperty::Color | eui::AnimProperty::Border);
        if (shadowBlur > 0.0f) {
            box.shadow(shadowBlur, 0.0f, shadowDy, shadowCol);
        }
        box.build();

        const float iconW = iconCp != 0 ? (fontSize + 2.0f) : 0.0f;
        const float gap = (iconCp != 0 && !label.empty()) ? 7.0f : 0.0f;
        const float textW = label.empty()
            ? 0.0f
            : std::min(width - iconW - gap - 14.0f, static_cast<float>(label.size()) * (fontSize * 0.56f) + 6.0f);
        const float contentW = std::max(0.0f, iconW + gap + textW);
        const float startX = std::max(6.0f, (width - contentW) * 0.5f);

        ui.row(id + ".row")
            .position(startX, 0.0f)
            .size(std::max(10.0f, width - startX * 2.0f), height)
            .gap(gap)
            .alignItems(eui::Align::CENTER)
            .content([&] {
                if (iconCp != 0) {
                    ui.text(id + ".icon")
                        .size(iconW, height)
                        .icon(iconCp)
                        .fontSize(fontSize)
                        .color(fg)
                        .horizontalAlign(eui::HorizontalAlign::Center)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                }
                if (!label.empty()) {
                    ui.text(id + ".lbl")
                        .size(std::max(12.0f, width - startX * 2.0f - iconW - gap), height)
                        .text(label)
                        .fontSize(fontSize)
                        .fontWeight(kind == ButtonKind::Primary ? 650 : 600)
                        .color(fg)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                }
            })
            .build();
    }).build();
}

// Primary EUI-NEO native hero button (>= 136px wide).
inline void HeroButton(eui::Ui& ui,
                       const std::string& id,
                       const UiPalette& p,
                       float width,
                       float height,
                       const std::string& label,
                       uint32_t iconCp,
                       const std::function<void()>& onClick,
                       bool disabled = false,
                       float fontSize = 15.0f) {
    components::button(ui, id)
        .theme(p.tokens, !disabled)
        .size(width, height)
        .icon(iconCp)
        .text(label)
        .fontSize(fontSize)
        .colors(disabled ? p.cardAltBg : p.accent,
                disabled ? p.cardAltBg : p.accentHover,
                disabled ? p.cardAltBg : p.accentPressed)
        .textColor(disabled ? p.faintText : kWhite)
        .iconColor(disabled ? p.faintText : kWhite)
        .radius(12.0f)
        .border(disabled ? 1.0f : 0.0f, disabled ? p.hairline : kTransparent)
        .shadow(disabled ? 0.0f : 16.0f, 0.0f, disabled ? 0.0f : 5.0f,
                disabled ? kTransparent : p.accentShadow)
        .pressScale(disabled ? 1.0f : 0.97f)
        .transition(p.anim)
        .disabled(disabled)
        .onClick(onClick)
        .build();
}

// Clean typographic page header without boxed icon clutter.
inline void PageHeader(eui::Ui& ui,
                       const std::string& id,
                       float width,
                       const std::string& title,
                       const std::string& subtitle,
                       const UiPalette& p,
                       float rightSlotW = 0.0f,
                       const std::function<void()>& rightSlot = nullptr) {
    const float leftW = rightSlot && rightSlotW > 0.0f ? std::max(220.0f, width - rightSlotW - 16.0f) : width;
    ui.row(id)
        .size(width, 54.0f)
        .gap(16.0f)
        .alignItems(eui::Align::CENTER)
        .content([&] {
            ui.column(id + ".text")
                .size(leftW, 54.0f)
                .gap(4.0f)
                .content([&] {
                    ui.text(id + ".title")
                        .size(leftW, 30.0f)
                        .text(title)
                        .fontSize(24.0f)
                        .fontWeight(700)
                        .color(p.ink)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                    ui.text(id + ".sub")
                        .size(leftW, 20.0f)
                        .text(subtitle)
                        .fontSize(13.5f)
                        .color(p.mutedText)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                })
                .build();

            if (rightSlot && rightSlotW > 0.0f) {
                ui.stack(id + ".right")
                    .size(rightSlotW, 54.0f)
                    .alignItems(eui::Align::END)
                    .content([&] { rightSlot(); })
                    .build();
            }
        })
        .build();
}

// Shared centered scroll container scaffold used cleanly across all four pages.
inline void CenteredScrollPage(eui::Ui& ui,
                               const std::string& id,
                               const UiPalette& p,
                               float viewW,
                               float viewH,
                               float maxStudioW,
                               float minSidePad,
                               float& scrollOffset,
                               const std::string& contentKey,
                               const std::function<void(eui::Ui& innerUi, float studioW)>& bodyFn) {
    components::scrollView(ui, id)
        .theme(p.tokens)
        .size(viewW, viewH)
        .offset(scrollOffset)
        .step(56.0f)
        .gap(0.0f)
        .contentKey(contentKey)
        .onChange([&scrollOffset](float nextOffset) {
            scrollOffset = nextOffset;
        })
        .content([=](eui::Ui& innerUi, float innerW, float /*viewportH*/) {
            const float studioW = CenteredWidth(innerW, maxStudioW, minSidePad);
            const float leftPad = std::max(0.0f, (innerW - studioW) * 0.5f);

            innerUi.column(id + ".padWrap")
                .width(innerW)
                .gap(0.0f)
                .content([&] {
                    innerUi.rect(id + ".topPad").size(innerW, 26.0f).color(kTransparent).build();

                    innerUi.row(id + ".centerRow")
                        .width(innerW)
                        .content([&] {
                            if (leftPad > 0.5f) {
                                innerUi.rect(id + ".leftSpacer").size(leftPad, 1.0f).color(kTransparent).build();
                            }

                            innerUi.column(id + ".studio")
                                .width(studioW)
                                .gap(18.0f)
                                .content([&] { bodyFn(innerUi, studioW); })
                                .build();
                        })
                        .build();

                    innerUi.rect(id + ".botPad").size(innerW, 34.0f).color(kTransparent).build();
                })
                .build();
        })
        .build();
}

// Crisp ambient EUI-NEO section card with generous studio padding & shadow.
inline void SectionCard(eui::Ui& ui,
                        const std::string& id,
                        const UiPalette& p,
                        float width,
                        float padding,
                        float gap,
                        const std::function<void(float innerW)>& contentFn,
                        float radius = 16.0f) {
    const float innerW = std::max(80.0f, width - padding * 2.0f);
    components::card(ui, id)
        .theme(p.tokens)
        .width(width)
        .wrapContentHeight()
        .padding(padding)
        .color(p.cardBg)
        .radius(radius)
        .border(1.0f, p.hairline)
        .shadow(eui::Shadow{true, {0.0f, 5.0f}, 20.0f, 0.0f, p.shadow, false})
        .content([&] {
            ui.column(id + ".col")
                .width(innerW)
                .gap(gap)
                .content([&] { contentFn(innerW); })
                .build();
        })
        .build();
}

// Clean section header with optional right caption.
inline void SectionTitle(eui::Ui& ui,
                         const std::string& id,
                         const UiPalette& p,
                         float width,
                         const std::string& title,
                         const std::string& subtitle = "") {
    if (subtitle.empty()) {
        ui.text(id)
            .size(width, 22.0f)
            .text(title)
            .fontSize(15.0f)
            .fontWeight(700)
            .color(p.ink)
            .verticalAlign(eui::VerticalAlign::Center)
            .build();
        return;
    }
    ui.row(id)
        .size(width, 22.0f)
        .gap(12.0f)
        .alignItems(eui::Align::CENTER)
        .content([&] {
            const float titleW = std::min(width * 0.48f, static_cast<float>(title.size()) * 9.5f + 16.0f);
            ui.text(id + ".t")
                .size(titleW, 22.0f)
                .text(title)
                .fontSize(15.0f)
                .fontWeight(700)
                .color(p.ink)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
            ui.text(id + ".sub")
                .size(std::max(40.0f, width - titleW - 12.0f), 22.0f)
                .text(subtitle)
                .fontSize(12.5f)
                .color(p.mutedText)
                .horizontalAlign(eui::HorizontalAlign::Right)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();
}

inline void Hairline(eui::Ui& ui, const std::string& id, const UiPalette& p, float width) {
    ui.rect(id)
        .size(width, 1.0f)
        .color(p.hairline)
        .build();
}

// Refined borderless status pill with glowing 7px dot.
inline void StatusBadge(eui::Ui& ui,
                        const std::string& id,
                        const UiPalette& p,
                        float width,
                        float height,
                        const std::string& label,
                        uint32_t /*iconCp*/,
                        const SemanticColor& sem,
                        float fontSize = 12.5f) {
    (void)p;
    ui.stack(id)
        .size(width, height)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, height)
                .radius(height * 0.5f)
                .color(sem.bg)
                .build();
            ui.row(id + ".row")
                .position(11.0f, 0.0f)
                .size(std::max(10.0f, width - 22.0f), height)
                .gap(7.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.rect(id + ".dot")
                        .size(7.0f, 7.0f)
                        .radius(3.5f)
                        .color(sem.fg)
                        .build();
                    ui.text(id + ".lbl")
                        .size(std::max(10.0f, width - 30.0f), height)
                        .text(label)
                        .fontSize(fontSize)
                        .fontWeight(600)
                        .color(sem.fg)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                })
                .build();
        })
        .build();
}

// Clean inline preflight / alert banner with optional compact action pill.
inline void BannerBox(eui::Ui& ui,
                      const std::string& id,
                      const UiPalette& p,
                      float width,
                      const SemanticColor& sem,
                      uint32_t iconCp,
                      const std::string& message,
                      const std::string& fixLabel = "",
                      const std::function<void()>& onFix = nullptr) {
    const bool hasFix = !fixLabel.empty() && onFix;
    const float fixW = hasFix ? std::min(156.0f, std::max(98.0f, static_cast<float>(fixLabel.size()) * 6.5f + 26.0f)) : 0.0f;
    const float textW = std::max(80.0f, width - 54.0f - (hasFix ? (fixW + 12.0f) : 0.0f));
    const float charsPerLine = std::max(24.0f, textW / 12.0f);
    const int approxLines = std::max(1, static_cast<int>(std::ceil(static_cast<float>(message.size()) / (charsPerLine * 2.15f))));
    const float boxH = std::max(46.0f, static_cast<float>(approxLines) * 20.0f + 24.0f);

    ui.stack(id)
        .size(width, boxH)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, boxH)
                .radius(12.0f)
                .color(sem.bg)
                .border(1.0f, sem.border)
                .transition(p.anim)
                .animate(eui::AnimProperty::Color | eui::AnimProperty::Border)
                .build();

            ui.row(id + ".row")
                .position(14.0f, 10.0f)
                .size(width - 28.0f, boxH - 20.0f)
                .gap(10.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.text(id + ".ic")
                        .size(18.0f, 20.0f)
                        .icon(iconCp)
                        .fontSize(15.5f)
                        .color(sem.fg)
                        .horizontalAlign(eui::HorizontalAlign::Center)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();

                    ui.text(id + ".msg")
                        .size(textW, boxH - 20.0f)
                        .text(message)
                        .fontSize(13.5f)
                        .lineHeight(19.5f)
                        .fontWeight(500)
                        .color(p.ink)
                        .wrap(true)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();

                    if (hasFix) {
                        ui.stack(id + ".fix")
                            .size(fixW, 32.0f)
                            .visualStateFrom(id + ".fix", 0.97f)
                            .transition(p.anim)
                            .onClick(onFix)
                            .content([&] {
                                ui.rect(id + ".fixBg")
                                    .size(fixW, 32.0f)
                                    .radius(8.0f)
                                    .color(p.cardBg)
                                    .hoverColor(p.pillHover)
                                    .border(1.0f, sem.border)
                                    .visualStateFrom(id + ".fix", 1.0f)
                                    .transition(p.anim)
                                    .animate(eui::AnimProperty::Color)
                                    .build();
                                ui.text(id + ".fixLbl")
                                    .size(fixW, 32.0f)
                                    .text(fixLabel)
                                    .fontSize(12.5f)
                                    .fontWeight(650)
                                    .color(sem.fg)
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
}

// Clean shared monospace command/path code well with optional right action button.
inline void CodeBar(eui::Ui& ui,
                    const std::string& id,
                    const UiPalette& p,
                    float width,
                    const std::string& codeText,
                    const std::string& actionLabel = "",
                    uint32_t actionIcon = 0xF0C5u,
                    const std::function<void()>& onAction = nullptr,
                    bool actionDisabled = false) {
    const bool hasAct = !actionLabel.empty() && onAction;
    const float actW = hasAct ? 78.0f : 0.0f;
    const float barH = 42.0f;
    const float textW = std::max(80.0f, width - 26.0f - (hasAct ? actW + 8.0f : 0.0f));

    ui.stack(id)
        .size(width, barH)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, barH)
                .radius(10.0f)
                .color(p.cardAltBg)
                .build();

            ui.row(id + ".row")
                .position(13.0f, 5.0f)
                .size(width - 20.0f, barH - 10.0f)
                .gap(8.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.text(id + ".code")
                        .size(textW, barH - 10.0f)
                        .text(codeText)
                        .fontFamily("monospace")
                        .fontSize(12.5f)
                        .color(p.monoText)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();

                    if (hasAct) {
                        ActionButton(ui, id + ".btn", p, actW, 30.0f,
                                     actionLabel, actionIcon, ButtonKind::Ghost,
                                     onAction, actionDisabled, 12.5f);
                    }
                })
                .build();
        })
        .build();
}

// Shared key-value metadata panel used cleanly across Inspect and History inspector cards.
struct MetaFieldRow {
    std::string label;
    std::string value;
    bool mono = true;
    bool emphasized = false;
};

inline void MetaWell(eui::Ui& ui,
                     const std::string& id,
                     const UiPalette& p,
                     float width,
                     const std::vector<MetaFieldRow>& rows) {
    const float rowH = 22.0f;
    const float rowGap = 6.0f;
    const float padV = 12.0f;
    const float wellH = padV * 2.0f +
                        static_cast<float>(rows.size()) * rowH +
                        static_cast<float>(std::max<size_t>(1, rows.size()) - 1) * rowGap;
    const float innerW = std::max(80.0f, width - 28.0f);
    const float keyW = 86.0f;
    const float valW = std::max(80.0f, innerW - keyW - 10.0f);

    ui.stack(id)
        .size(width, wellH)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, wellH)
                .radius(12.0f)
                .color(p.cardAltBg)
                .build();

            ui.column(id + ".col")
                .position(14.0f, padV)
                .size(innerW, wellH - padV * 2.0f)
                .gap(rowGap)
                .content([&] {
                    for (size_t i = 0; i < rows.size(); ++i) {
                        const auto& r = rows[i];
                        const std::string rid = id + ".r" + std::to_string(i);
                        ui.row(rid)
                            .size(innerW, rowH)
                            .gap(10.0f)
                            .alignItems(eui::Align::CENTER)
                            .content([&] {
                                ui.text(rid + ".k")
                                    .size(keyW, rowH)
                                    .text(r.label)
                                    .fontSize(12.5f)
                                    .fontWeight(600)
                                    .color(p.mutedText)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .build();

                                auto valText = ui.text(rid + ".v")
                                    .size(valW, rowH)
                                    .text(r.value)
                                    .fontSize(12.5f)
                                    .color(r.emphasized ? p.ink : p.secondaryText)
                                    .verticalAlign(eui::VerticalAlign::Center);
                                if (r.mono) {
                                    valText.fontFamily("monospace");
                                }
                                valText.build();
                            })
                            .build();
                    }
                })
                .build();
        })
        .build();
}

} // namespace fl::ui_kit
