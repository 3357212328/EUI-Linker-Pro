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
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace {

struct NavItemSpec {
    int id = 0;
    uint32_t icon = 0;
    const char* label = "";
    const char* hint = "";
};

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

    const int desiredW = static_cast<int>(1220.0f * scale + 0.5f);
    const int desiredH = static_cast<int>(800.0f * scale + 0.5f);
    const int desiredMinW = static_cast<int>(900.0f * scale + 0.5f);
    const int desiredMinH = static_cast<int>(640.0f * scale + 0.5f);

    outW = std::min(desiredW, static_cast<int>(screenW * 0.90f));
    outH = std::min(desiredH, static_cast<int>(screenH * 0.90f));
    outMinW = std::min(desiredMinW, outW);
    outMinH = std::min(desiredMinH, outH);
}

void DrawSidebarNavItem(eui::Ui& ui,
                        const NavItemSpec& item,
                        bool active,
                        bool compact,
                        float width,
                        float yPos,
                        const fl::ui_kit::UiPalette& p,
                        int countBadge,
                        const std::function<void()>& onSelect) {
    const float itemH = 42.0f;
    const std::string itemId = "nav.item." + std::to_string(item.id);
    const eui::Color fg = active ? p.accent : p.secondaryText;

    ui.stack(itemId)
        .position(12.0f, yPos)
        .size(width, itemH)
        .visualStateFrom(itemId, 0.985f)
        .transition(p.anim)
        .onClick(onSelect)
        .content([&] {
            ui.rect(itemId + ".bg")
                .size(width, itemH)
                .radius(10.0f)
                .color(active ? p.navActive : fl::ui_kit::kTransparent)
                .hoverColor(active ? p.navActive : p.navHover)
                .pressedColor(p.pillBg)
                .visualStateFrom(itemId, 1.0f)
                .transition(p.anim)
                .animate(eui::AnimProperty::Color)
                .build();

            if (compact) {
                ui.text(itemId + ".ic")
                    .position(0.0f, 0.0f)
                    .size(width, itemH)
                    .icon(item.icon)
                    .fontSize(16.5f)
                    .color(fg)
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .verticalAlign(eui::VerticalAlign::Center)
                    .build();
            } else {
                const bool showBadge = countBadge > 0;
                const float badgeW = showBadge ? 34.0f : 0.0f;
                const float labelW = std::max(40.0f, width - 54.0f - (showBadge ? (badgeW + 8.0f) : 0.0f));

                ui.row(itemId + ".row")
                    .position(14.0f, 0.0f)
                    .size(width - 24.0f, itemH)
                    .gap(11.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        ui.text(itemId + ".ic")
                            .size(18.0f, itemH)
                            .icon(item.icon)
                            .fontSize(15.5f)
                            .color(fg)
                            .horizontalAlign(eui::HorizontalAlign::Center)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();

                        ui.text(itemId + ".lbl")
                            .size(labelW, itemH)
                            .text(item.label)
                            .fontSize(14.0f)
                            .fontWeight(active ? 650 : 550)
                            .color(active ? p.ink : p.secondaryText)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();

                        if (showBadge) {
                            const std::string badgeText = countBadge > 99 ? "99+" : std::to_string(countBadge);
                            ui.stack(itemId + ".badge")
                                .size(badgeW, 22.0f)
                                .content([&] {
                                    ui.rect(itemId + ".badgeBg")
                                        .size(badgeW, 22.0f)
                                        .radius(11.0f)
                                        .color(active ? p.accentSoft : p.pillBg)
                                        .build();
                                    ui.text(itemId + ".badgeTxt")
                                        .size(badgeW, 22.0f)
                                        .text(badgeText)
                                        .fontSize(11.5f)
                                        .fontWeight(650)
                                        .color(active ? p.accent : p.mutedText)
                                        .horizontalAlign(eui::HorizontalAlign::Center)
                                        .verticalAlign(eui::VerticalAlign::Center)
                                        .build();
                                })
                                .build();
                        }
                    })
                    .build();
            }
        })
        .build();

    if (compact) {
        components::tooltip(ui, itemId + ".tip")
            .source(itemId)
            .value(std::string(item.label) + " (" + item.hint + ")")
            .anchor(12.0f + width * 0.5f, yPos + itemH)
            .bounds(width, itemH)
            .theme(p.tokens)
            .zIndex(900)
            .build();
    }
}

} // namespace

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = [] {
        int winW = 1220;
        int winH = 800;
        int minW = 900;
        int minH = 640;
        computeInitialWindowMetrics(winW, winH, minW, minH);

        const fl::AppSettings startupSettings = fl::win::LoadSettings();
        const eui::Color initClear = startupSettings.darkTheme
            ? eui::Color{0.047f, 0.059f, 0.078f, 1.0f}
            : eui::Color{0.961f, 0.969f, 0.980f, 1.0f};

        return DslAppConfig()
            .title("Fluent Linker Pro - NTFS 符号链接 · 搬家 · 体检工作台")
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

    if (state.activeTab == 0) {
        fl::actions::EnsureValidation(state);
    }

    const bool compactNav = screen.width < 940.0f;
    const float navW = compactNav ? 74.0f : 232.0f;
    const float navItemW = navW - 24.0f;
    const float contentW = std::max(360.0f, screen.width - navW);
    const float contentH = std::max(360.0f, screen.height);

    // Base studio window surface
    ui.rect("shell.bg")
        .size(screen.width, screen.height)
        .color(p.windowBg)
        .transition(p.anim)
        .animate(eui::AnimProperty::Color)
        .build();

    // Left EUI-NEO Sidebar Navigation Rail
    ui.stack("shell.sidebar")
        .size(navW, screen.height)
        .content([&] {
            ui.rect("sidebar.bg")
                .size(navW, screen.height)
                .color(p.sidebarBg)
                .transition(p.anim)
                .animate(eui::AnimProperty::Color)
                .build();

            ui.rect("sidebar.divider")
                .position(navW - 1.0f, 0.0f)
                .size(1.0f, screen.height)
                .color(p.hairline)
                .build();

            const std::string brandLogoPath = eui::platform::resolveResourcePath("assets/icon.png");

            if (compactNav) {
                ui.stack("sidebar.logoBox")
                    .position((navW - 36.0f) * 0.5f, 20.0f)
                    .size(36.0f, 36.0f)
                    .content([&] {
                        ui.image("sidebar.logoImg")
                            .size(36.0f, 36.0f)
                            .radius(10.0f)
                            .source(brandLogoPath)
                            .build();
                    })
                    .build();
            } else {
                ui.row("sidebar.brand")
                    .position(18.0f, 22.0f)
                    .size(navW - 32.0f, 44.0f)
                    .gap(11.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        ui.image("sidebar.logoImg")
                            .size(34.0f, 34.0f)
                            .radius(9.0f)
                            .source(brandLogoPath)
                            .build();

                        ui.column("sidebar.brandTxt")
                            .size(navW - 80.0f, 40.0f)
                            .gap(2.0f)
                            .content([&] {
                                ui.text("sidebar.title")
                                    .size(navW - 80.0f, 22.0f)
                                    .text("Fluent Linker")
                                    .fontSize(16.0f)
                                    .fontWeight(700)
                                    .color(p.ink)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .build();
                                ui.text("sidebar.subtitle")
                                    .size(navW - 80.0f, 16.0f)
                                    .text("Pro · NTFS 链接管家")
                                    .fontSize(11.5f)
                                    .color(p.mutedText)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .build();
                            })
                            .build();
                    })
                    .build();
            }

            if (!compactNav) {
                ui.text("sidebar.sectionLbl")
                    .position(20.0f, 84.0f)
                    .size(navW - 40.0f, 18.0f)
                    .text("工作区")
                    .fontSize(11.5f)
                    .fontWeight(650)
                    .color(p.faintText)
                    .verticalAlign(eui::VerticalAlign::Center)
                    .build();
            }

            static constexpr NavItemSpec kNavItems[] = {
                {0, 0xF0FEu, "新建与迁移", "Ctrl+1"},
                {1, 0xF002u, "体检与扫描", "Ctrl+2"},
                {2, 0xF1DAu, "执行记录",   "Ctrl+3"},
                {3, 0xF059u, "环境与指南", "Ctrl+4"},
            };

            const float listTop = compactNav ? 88.0f : 110.0f;
            const float stepH = 46.0f;

            // Smoothly sliding active indicator bar
            ui.rect("nav.activeBar")
                .position(5.0f, listTop + static_cast<float>(state.activeTab) * stepH + 10.0f)
                .size(3.5f, 22.0f)
                .radius(2.0f)
                .color(p.accent)
                .transition(p.slowAnim)
                .animate(eui::AnimProperty::Frame | eui::AnimProperty::Color)
                .build();

            for (int i = 0; i < 4; ++i) {
                const auto& item = kNavItems[i];
                DrawSidebarNavItem(
                    ui, item, state.activeTab == item.id, compactNav,
                    navItemW, listTop + static_cast<float>(i) * stepH,
                    p, item.id == 2 ? static_cast<int>(state.history.size()) : 0,
                    [&state, tabId = item.id] {
                        state.activeTab = tabId;
                        fl::actions::RefreshPrivileges(state);
                    });
            }

            // Bottom Footer Controls
            const float footerH = 92.0f;
            const float footerY = std::max(330.0f, screen.height - footerH - 16.0f);
            ui.rect("sidebar.footerDiv")
                .position(14.0f, footerY - 12.0f)
                .size(navW - 28.0f, 1.0f)
                .color(p.hairline)
                .build();

            ui.column("sidebar.footer")
                .position(12.0f, footerY)
                .size(navItemW, footerH)
                .gap(8.0f)
                .content([&] {
                    const fl::ui_kit::SemanticColor privSem = state.isAdmin
                        ? p.success
                        : (state.isDevMode ? p.info : p.warning);
                    const std::string privLabel = state.isAdmin
                        ? "管理员权限就绪"
                        : (state.isDevMode ? "开发者免提权" : "建议提权 · 点此处理");

                    ui.stack("sidebar.privPill")
                        .size(navItemW, 38.0f)
                        .visualStateFrom("sidebar.privPill", 0.98f)
                        .transition(p.anim)
                        .onClick([&] {
                            if (state.isAdmin) {
                                fl::actions::RefreshPrivileges(state);
                                fl::ui_kit::TriggerToast(state, "权限已刷新", "当前已以完整管理员权限运行。", 1, 0xF058u);
                            } else if (state.isDevMode) {
                                state.activeTab = 3;
                            } else {
                                if (!fl::win::RelaunchAsAdmin()) {
                                    fl::ui_kit::TriggerToast(state, "已取消提权", "可前往“环境与指南”开启 Windows 开发者模式。", 2, 0xF071u);
                                }
                            }
                        })
                        .content([&] {
                            ui.rect("sidebar.privBg")
                                .size(navItemW, 38.0f)
                                .radius(10.0f)
                                .color(privSem.bg)
                                .hoverColor(fl::ui_kit::Rgba(privSem.fg.r, privSem.fg.g, privSem.fg.b, p.dark ? 0.22f : 0.16f))
                                .visualStateFrom("sidebar.privPill", 1.0f)
                                .transition(p.anim)
                                .animate(eui::AnimProperty::Color)
                                .build();

                            if (compactNav) {
                                ui.rect("sidebar.privDotOnly")
                                    .position((navItemW - 10.0f) * 0.5f, 14.0f)
                                    .size(10.0f, 10.0f)
                                    .radius(5.0f)
                                    .color(privSem.fg)
                                    .build();
                            } else {
                                ui.row("sidebar.privRow")
                                    .position(12.0f, 0.0f)
                                    .size(navItemW - 20.0f, 38.0f)
                                    .gap(9.0f)
                                    .alignItems(eui::Align::CENTER)
                                    .content([&] {
                                        ui.rect("sidebar.privDot")
                                            .size(8.0f, 8.0f)
                                            .radius(4.0f)
                                            .color(privSem.fg)
                                            .build();
                                        ui.text("sidebar.privTxt")
                                            .size(navItemW - 38.0f, 38.0f)
                                            .text(privLabel)
                                            .fontSize(12.5f)
                                            .fontWeight(600)
                                            .color(privSem.fg)
                                            .verticalAlign(eui::VerticalAlign::Center)
                                            .build();
                                    })
                                    .build();
                            }
                        })
                        .build();

                    if (compactNav) {
                        components::tooltip(ui, "sidebar.privTip")
                            .source("sidebar.privPill")
                            .value(privLabel)
                            .anchor(12.0f + navItemW * 0.5f, footerY)
                            .bounds(navItemW, 38.0f)
                            .theme(p.tokens)
                            .zIndex(900)
                            .build();
                    }

                    fl::ui_kit::ActionButton(
                        ui, "sidebar.themeBtn", p, navItemW, 38.0f,
                        compactNav ? "" : (state.darkTheme ? "切换浅色模式" : "切换深色模式"),
                        state.darkTheme ? 0xF185u : 0xF186u,
                        fl::ui_kit::ButtonKind::Secondary,
                        [&] {
                            state.darkTheme = !state.darkTheme;
                            fl::actions::PersistSettings(state);
                        },
                        false, 13.0f);
                })
                .build();
        })
        .build();

    // Right Page Canvas Host with EUI-NEO Loader KeepAlive per page
    ui.stack("shell.workspace")
        .position(navW, 0.0f)
        .size(contentW, contentH)
        .content([&] {
            ui.loader("page.create")
                .active(state.activeTab == 0)
                .keepAlive()
                .content([&] {
                    fl::ComposeCreatePage(ui, state, p, contentW, contentH);
                })
                .build();

            ui.loader("page.inspect")
                .active(state.activeTab == 1)
                .keepAlive()
                .content([&] {
                    fl::ComposeInspectPage(ui, state, p, contentW, contentH);
                })
                .build();

            ui.loader("page.history")
                .active(state.activeTab == 2)
                .keepAlive()
                .content([&] {
                    fl::ComposeHistoryPage(ui, state, p, contentW, contentH);
                })
                .build();

            ui.loader("page.guide")
                .active(state.activeTab == 3)
                .keepAlive()
                .content([&] {
                    fl::ComposeGuidePage(ui, state, p, contentW, contentH);
                })
                .build();
        })
        .build();

    // Global Confirmation Dialog
    components::dialog(ui, "app.confirmDialog")
        .theme(p.tokens)
        .open(state.confirmOpen)
        .screen(screen.width, screen.height)
        .size(540.0f, 290.0f)
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
