#include "app_actions.h"
#include "eui_neo.h"
#include "linker_types.h"
#include "pages/create_page.h"
#include "pages/guide_page.h"
#include "pages/history_page.h"
#include "pages/inspect_page.h"
#include "ui_theme.h"
#include "win_linker.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {

fl::AppState& globalState() {
    static fl::AppState instance;
    return instance;
}

float queryPrimaryDpiScale() {
    float scale = 1.0f;
    if (HDC hdc = GetDC(nullptr)) {
        const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
        if (dpi >= 96) {
            scale = std::clamp(static_cast<float>(dpi) / 96.0f, 1.0f, 3.0f);
        }
    }
    return scale;
}

void computeInitialWindowMetrics(int& outW, int& outH, int& outMinW, int& outMinH) {
    const float scale = queryPrimaryDpiScale();
    const int screenW = std::max(800, GetSystemMetrics(SM_CXSCREEN));
    const int screenH = std::max(600, GetSystemMetrics(SM_CYSCREEN));

    const int desiredW = static_cast<int>(1240.0f * scale + 0.5f);
    const int desiredH = static_cast<int>(820.0f * scale + 0.5f);
    const int desiredMinW = static_cast<int>(860.0f * scale + 0.5f);
    const int desiredMinH = static_cast<int>(640.0f * scale + 0.5f);

    outW = std::min(desiredW, static_cast<int>(screenW * 0.92f));
    outH = std::min(desiredH, static_cast<int>(screenH * 0.92f));
    outMinW = std::min(desiredMinW, outW);
    outMinH = std::min(desiredMinH, outH);
}

} // namespace

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = [] {
        int winW = 1240;
        int winH = 820;
        int minW = 860;
        int minH = 640;
        computeInitialWindowMetrics(winW, winH, minW, minH);

        const fl::AppSettings startupSettings = fl::win::LoadSettings();
        const eui::Color initClear = startupSettings.darkTheme
            ? eui::Color{0.043f, 0.063f, 0.094f, 1.0f}
            : eui::Color{0.953f, 0.965f, 0.980f, 1.0f};

        return DslAppConfig()
            .title("Fluent Linker Pro - NTFS 符号链接与搬家工作台")
            .pageId("fluent_linker.main")
            .appId("com.fluentlinker.pro")
            .clearColor(initClear)
            .windowSize(winW, winH)
            .minWindowSize(minW, minH)
            .fps(0.0)
            .showDebugStatsInTitle(false)
            .showDebugOverlay(false)
            .iconPath("assets/icon.png")
            .textFont("Microsoft YaHei")
            .onStart([] {
                fl::actions::InitializeAppState(globalState());
                fl::actions::EnsureValidation(globalState());
            })
            .onShutdown([] {
                app::async::cancel("fl.scan");
                fl::actions::PersistSettings(globalState());
            })
            .onKeyEvent([](const eui::KeyEvent& event) {
                if (!event.isDown()) {
                    return;
                }
                auto& state = globalState();
                if (event.modifiers.shortcut()) {
                    if (event.key == eui::InputKey::Digit1) {
                        state.activeTab = 0;
                    } else if (event.key == eui::InputKey::Digit2) {
                        state.activeTab = 1;
                    } else if (event.key == eui::InputKey::Digit3) {
                        state.activeTab = 2;
                    } else if (event.key == eui::InputKey::Digit4) {
                        state.activeTab = 3;
                    } else if (event.key == eui::InputKey::Enter && state.activeTab == 0 && !state.confirmOpen) {
                        fl::actions::ExecuteOrConfirmCreate(state);
                    }
                }
            });
    }();
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    auto& state = globalState();
    static const fl::ui_kit::UiPalette darkPalette = fl::ui_kit::MakePalette(true);
    static const fl::ui_kit::UiPalette lightPalette = fl::ui_kit::MakePalette(false);
    const fl::ui_kit::UiPalette& p = state.darkTheme ? darkPalette : lightPalette;

    // Full-window animated theme background
    ui.rect("app.canvas")
        .size(screen.width, screen.height)
        .color(p.canvas)
        .transition(p.anim)
        .build();

    const float headerH = 78.0f;
    const float padX = 20.0f;
    const float padY = 16.0f;
    const float contentW = std::max(320.0f, screen.width - padX * 2.0f);
    const float contentH = std::max(240.0f, screen.height - headerH - padY * 2.0f);

    ui.column("app.shell")
        .size(screen.width, screen.height)
        .content([&] {
            // Top Navigation & Status Header
            ui.stack("app.header")
                .size(screen.width, headerH)
                .content([&] {
                    ui.rect("app.header.bg")
                        .size(screen.width, headerH)
                        .color(p.headerBg)
                        .transition(p.anim)
                        .build();

                    ui.rect("app.header.divider")
                        .y(headerH - 1.0f)
                        .size(screen.width, 1.0f)
                        .color(p.border)
                        .build();

                    const bool compactHeader = screen.width < 1040.0f;
                    ui.row("app.header.row")
                        .size(screen.width, headerH)
                        .padding(0.0f, padX, 0.0f, padX)
                        .gap(compactHeader ? 8.0f : 14.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            // Brand Logo & Title
                            ui.row("app.brand")
                                .height(46.0f)
                                .gap(10.0f)
                                .alignItems(eui::Align::CENTER)
                                .content([&] {
                                    ui.image("app.brand.logo")
                                        .source(eui::platform::resolveResourcePath("assets/icon.png"))
                                        .size(36.0f, 36.0f)
                                        .radius(8.0f)
                                        .contain()
                                        .build();

                                    ui.column("app.brand.titles")
                                        .height(eui::SizeValue::wrapContent())
                                        .gap(2.0f)
                                        .content([&] {
                                            ui.text("app.brand.name")
                                                .text("Fluent Linker Pro")
                                                .fontSize(compactHeader ? 17.0f : 19.0f)
                                                .lineHeight(23.0f)
                                                .fontWeight(700)
                                                .color(p.text)
                                                .build();
                                            if (!compactHeader) {
                                                ui.text("app.brand.sub")
                                                    .text("NTFS 符号链接 · 搬家 · 体检工具")
                                                    .fontSize(13.5f)
                                                    .lineHeight(17.0f)
                                                    .color(p.mutedText)
                                                    .build();
                                            }
                                        })
                                        .build();
                                })
                                .build();

                            ui.row("app.header.spL").flexGrow(1.0f).height(1.0f).content([] {}).build();

                            // 4-tab Top Segmented Navigation
                            const float navW = std::clamp(screen.width - (compactHeader ? 480.0f : 610.0f), 330.0f, 480.0f);
                            components::segmented(ui, "app.nav")
                                .theme(p.tokens)
                                .size(navW, 42.0f)
                                .items({"创建与搬家", "链接体检", "执行历史", "原理指南"})
                                .selected(state.activeTab)
                                .fontSize(15.0f)
                                .transition(p.anim)
                                .onChange([&](int idx) {
                                    state.activeTab = idx;
                                })
                                .build();

                            ui.row("app.header.spR").flexGrow(1.0f).height(1.0f).content([] {}).build();

                            // Privilege Pill + Theme Switcher
                            const float privW = compactHeader ? 114.0f : 136.0f;
                            if (state.isAdmin) {
                                fl::ui_kit::StatusBadge(ui, "app.privBadge", p, privW, 32.0f,
                                                        "管理员模式", 0xF3EDu, p.success);
                            } else if (state.isDevMode) {
                                fl::ui_kit::StatusBadge(ui, "app.privBadge", p, privW, 32.0f,
                                                        compactHeader ? "免提权" : "开发者免提权", 0xF058u, p.info,
                                                        [&] {
                                                            if (fl::win::RelaunchAsAdmin()) {
                                                                std::exit(0);
                                                            }
                                                        });
                            } else {
                                fl::ui_kit::StatusBadge(ui, "app.privBadge", p, privW, 32.0f,
                                                        compactHeader ? "提权" : "提权管理员", 0xF3EDu, p.warning,
                                                        [&] {
                                                            if (fl::win::RelaunchAsAdmin()) {
                                                                std::exit(0);
                                                            } else {
                                                                fl::ui_kit::TriggerToast(state, "已取消提权",
                                                                                         "标准用户仍可免提权使用 /J 与 /H 链接。", 0, 0xF05Au);
                                                            }
                                                        });
                            }

                            fl::ui_kit::ActionButton(ui, "app.themeToggle", p, compactHeader ? 82.0f : 92.0f, 36.0f,
                                                     state.darkTheme ? "浅色" : "深色",
                                                     state.darkTheme ? 0xF185u : 0xF186u,
                                                     fl::ui_kit::ButtonKind::Secondary,
                                                     [&] {
                                                         state.darkTheme = !state.darkTheme;
                                                         fl::actions::PersistSettings(state);
                                                     },
                                                     false, 14.5f);
                        })
                        .build();
                })
                .build();

            // Active Page Body
            ui.stack("app.body")
                .size(screen.width, screen.height - headerH)
                .padding(padY, padX, padY, padX)
                .content([&] {
                    switch (state.activeTab) {
                        case 0:
                            fl::pages::ComposeCreatePage(ui, state, p, contentW, contentH);
                            break;
                        case 1:
                            fl::pages::ComposeInspectPage(ui, state, p, contentW, contentH);
                            break;
                        case 2:
                            fl::pages::ComposeHistoryPage(ui, state, p, contentW, contentH);
                            break;
                        case 3:
                        default:
                            fl::pages::ComposeGuidePage(ui, state, p, contentW, contentH);
                            break;
                    }
                })
                .build();
        })
        .build();

    // Global Confirmation Dialog
    components::dialog(ui, "app.confirmDialog")
        .theme(p.tokens)
        .open(state.confirmOpen)
        .screen(screen.width, screen.height)
        .size(560.0f, 300.0f)
        .title(state.confirmTitle)
        .message(state.confirmMessage)
        .primaryText(state.confirmPrimaryLabel)
        .secondaryText("取消")
        .onPrimary([&] { fl::actions::ExecuteConfirmedDialogAction(state); })
        .onSecondary([&] {
            state.confirmOpen = false;
            state.confirmAction = fl::ConfirmAction::None;
        })
        .onOpenChange([&](bool v) {
            state.confirmOpen = v;
            if (!v) {
                state.confirmAction = fl::ConfirmAction::None;
            }
        })
        .zIndex(1050)
        .build();

    // Global Non-blocking Toast Notification
    const auto toastSem = fl::ui_kit::ToneColors(p, state.toastTone);
    components::ToastStyle toastStyle(p.tokens);
    toastStyle.accent = toastSem.fg;
    toastStyle.border = toastSem.border;

    components::toast(ui, "app.toast")
        .theme(p.tokens)
        .style(toastStyle)
        .screen(screen.width, screen.height)
        .size(410.0f, 96.0f)
        .visible(state.toastVisible)
        .title(state.toastTitle)
        .message(state.toastMessage)
        .icon(state.toastIcon)
        .duration(3.2f)
        .onDismiss([&] { state.toastVisible = false; })
        .onAutoDismiss([&] { state.toastVisible = false; })
        .zIndex(1100)
        .build();
}

} // namespace app
