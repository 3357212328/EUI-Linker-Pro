#pragma once

#include "app.h"
#include "linker_types.h"
#include "win_linker.h"

#include <dwmapi.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace fl {

namespace detail {

inline BOOL CALLBACK EnumThreadMainHwnd(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }
    if (GetWindowTextLengthW(hwnd) <= 0) {
        return TRUE;
    }
    RECT r{};
    if (!GetClientRect(hwnd, &r) || (r.right - r.left) < 200 || (r.bottom - r.top) < 150) {
        return TRUE;
    }
    *reinterpret_cast<HWND*>(lParam) = hwnd;
    return FALSE;
}

inline HWND FindCurrentThreadMainWindow() {
    HWND found = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), EnumThreadMainHwnd, reinterpret_cast<LPARAM>(&found));
    return found;
}

inline void ApplyWindowsTitlebarDarkMode(bool dark) {
    const HWND hwnd = FindCurrentThreadMainWindow();
    if (!hwnd) {
        return;
    }
    const BOOL useDark = dark ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 10 2004+ / Windows 11)
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
}

inline std::string Utf8FromSlint(const slint::SharedString& s) {
    return std::string(s.data(), s.size());
}

inline slint::SharedString SlintFromUtf8(const std::string& s) {
    return slint::SharedString(s.c_str());
}

} // namespace detail

class AppController {
public:
    explicit AppController(slint::ComponentHandle<AppWindow> window)
        : ui_(std::move(window)) {
        initializeState();
        bindCallbacks();
        syncTheme();
        syncPrivileges();
        syncCreateState();
        syncInspectState();
        syncHistoryState();
    }

    ~AppController() {
        alive_->store(false, std::memory_order_relaxed);
        if (scanCancel_) {
            scanCancel_->store(true, std::memory_order_relaxed);
        }
    }

    void onWindowShown() {
        syncTheme();
    }

private:
    slint::ComponentHandle<AppWindow> ui_;
    AppState state_;
    slint::Timer toastTimer_;
    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
    std::shared_ptr<std::atomic_bool> scanCancel_;
    std::string lastCreatedLink_;
    bool createBusy_ = false;
    bool inspectScanning_ = false;

    void initializeState() {
        const AppSettings settings = win::LoadSettings();
        state_.darkTheme = settings.darkTheme;
        state_.linkType = static_cast<LinkType>(std::clamp(settings.defaultLinkType, 0, 3));
        state_.autoCreateParent = settings.autoCreateParent;
        state_.history = win::LoadHistoryWithMigration();
        rebuildFilteredHistoryIndices();
    }

    void persistSettings() const {
        AppSettings s;
        s.darkTheme = state_.darkTheme;
        s.defaultLinkType = static_cast<int>(state_.linkType);
        s.autoCreateParent = state_.autoCreateParent;
        win::SaveSettings(s);
    }

    void showToast(const std::string& title, const std::string& message, int tone) {
        ui_->set_toast_title(detail::SlintFromUtf8(title));
        ui_->set_toast_message(detail::SlintFromUtf8(message));
        ui_->set_toast_tone(tone);
        ui_->set_toast_visible(true);

        slint::ComponentWeakHandle<AppWindow> weak(ui_);
        toastTimer_.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(3200), [weak]() {
            if (auto locked = weak.lock()) {
                (*locked)->set_toast_visible(false);
            }
        });
    }

    void syncTheme() {
        ui_->invoke_apply_theme(state_.darkTheme);
        detail::ApplyWindowsTitlebarDarkMode(state_.darkTheme);
    }

    void syncPrivileges() {
        state_.isAdmin = win::IsRunningAsAdmin();
        state_.isDevMode = win::IsDeveloperModeEnabled();
        ui_->set_is_admin(state_.isAdmin);
        ui_->set_is_dev_mode(state_.isDevMode);
    }

    void pullCreateInputsFromUi() {
        state_.operationMode = static_cast<OperationMode>(
            std::clamp(ui_->get_create_operation_mode(), 0, 1));
        state_.linkType = static_cast<LinkType>(
            std::clamp(ui_->get_create_link_type(), 0, 3));
        state_.linkPath = detail::Utf8FromSlint(ui_->get_create_link_path());
        state_.targetPath = detail::Utf8FromSlint(ui_->get_create_target_path());
        state_.autoCreateParent = ui_->get_create_auto_create_parent();
    }

    void syncCreateState() {
        const ValidationResult val = win::ValidateCreateInputs(state_);

        ui_->set_create_operation_mode(static_cast<int>(state_.operationMode));
        ui_->set_create_link_type(static_cast<int>(state_.linkType));
        ui_->set_create_link_path(detail::SlintFromUtf8(state_.linkPath));
        ui_->set_create_target_path(detail::SlintFromUtf8(state_.targetPath));
        ui_->set_create_auto_create_parent(state_.autoCreateParent);
        ui_->set_create_is_busy(createBusy_);

        ui_->set_validation_level(static_cast<int>(val.level));
        ui_->set_validation_message(detail::SlintFromUtf8(val.message));
        ui_->set_quick_fix_label(detail::SlintFromUtf8(val.quickFixLabel));
        ui_->set_can_execute(val.canExecute);

        std::string preview = win::BuildMklinkCommand(state_.linkType, state_.linkPath, state_.targetPath);
        if (state_.operationMode == OperationMode::MoveAndLink) {
            const std::string src = state_.linkPath.empty() ? "<原目录>" : win::CleanPathInput(state_.linkPath);
            const std::string dst = state_.targetPath.empty() ? "<目标目录>" : win::CleanPathInput(state_.targetPath);
            preview = "1) robocopy \"" + src + "\" \"" + dst + "\" /E /COPYALL /MOVE  &&  2) " + preview;
        }
        ui_->set_command_preview(detail::SlintFromUtf8(preview));
        ui_->set_console_output(detail::SlintFromUtf8(state_.consoleOutput));
        ui_->set_last_created_link(detail::SlintFromUtf8(lastCreatedLink_));

        cachedQuickFix_ = val.quickFix;
    }

    void syncInspectState() {
        ui_->set_inspect_query_path(detail::SlintFromUtf8(state_.inspectQueryPath));
        ui_->set_inspect_include_hard_links(state_.inspectIncludeHardLinks);
        ui_->set_inspect_max_depth(state_.inspectMaxDepth);
        ui_->set_inspect_is_scanning(inspectScanning_);
        ui_->set_inspect_status_text(detail::SlintFromUtf8(state_.inspectStatusText));

        ui_->set_inspect_has_single(state_.hasInspectedSingle);
        if (state_.hasInspectedSingle) {
            const auto& s = state_.inspectedSingle;
            ui_->set_inspect_single_path(detail::SlintFromUtf8(s.path));
            ui_->set_inspect_single_kind_code(detail::SlintFromUtf8(s.kindCode));
            ui_->set_inspect_single_kind_label(detail::SlintFromUtf8(s.kindLabel));
            ui_->set_inspect_single_target(detail::SlintFromUtf8(s.targetPath));
            ui_->set_inspect_single_detail(detail::SlintFromUtf8(s.detailNote));
            ui_->set_inspect_single_healthy(s.targetExists);
            const bool canUnlink = s.exists && (s.isReparsePoint || s.hardLinkCount > 1);
            ui_->set_inspect_single_can_unlink(canUnlink);

            std::vector<slint::SharedString> sibs;
            sibs.reserve(s.hardLinkSiblings.size());
            for (const auto& path : s.hardLinkSiblings) {
                sibs.push_back(detail::SlintFromUtf8(path));
            }
            ui_->set_inspect_single_siblings(
                std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(sibs)));
        }

        std::vector<InspectItemUi> rows;
        rows.reserve(state_.scannedLinks.size());
        for (const auto& item : state_.scannedLinks) {
            InspectItemUi row;
            row.path = detail::SlintFromUtf8(item.path);
            row.kind_code = detail::SlintFromUtf8(item.kindCode);
            row.kind_label = detail::SlintFromUtf8(item.kindLabel);
            row.target_path = detail::SlintFromUtf8(item.targetPath);
            row.detail_note = detail::SlintFromUtf8(item.detailNote);
            row.target_healthy = item.targetExists;
            row.is_reparse = item.isReparsePoint;
            row.is_relative = item.isRelative;
            row.hard_link_count = static_cast<int>(item.hardLinkCount);
            rows.push_back(std::move(row));
        }
        ui_->set_inspect_scan_rows(
            std::make_shared<slint::VectorModel<InspectItemUi>>(std::move(rows)));
    }

    void rebuildFilteredHistoryIndices() {
        state_.filteredHistoryIndices.clear();
        for (int i = static_cast<int>(state_.history.size()) - 1; i >= 0; --i) {
            const auto& item = state_.history[static_cast<std::size_t>(i)];
            bool catMatch = true;
            switch (state_.historyCategoryFilter) {
                case 1: catMatch = win::ContainsCaseInsensitive(item.type, "/D"); break;
                case 2: catMatch = win::ContainsCaseInsensitive(item.type, "/J"); break;
                case 3: catMatch = win::ContainsCaseInsensitive(item.type, "/H"); break;
                case 4: catMatch = win::ContainsCaseInsensitive(item.type, "FILE"); break;
                case 5: catMatch = item.success; break;
                case 6: catMatch = !item.success; break;
                default: break;
            }
            if (!catMatch) {
                continue;
            }
            if (!state_.historyFilterText.empty()) {
                const bool matched =
                    win::ContainsCaseInsensitive(item.timestamp, state_.historyFilterText) ||
                    win::ContainsCaseInsensitive(item.type, state_.historyFilterText) ||
                    win::ContainsCaseInsensitive(item.linkPath, state_.historyFilterText) ||
                    win::ContainsCaseInsensitive(item.targetPath, state_.historyFilterText) ||
                    win::ContainsCaseInsensitive(item.result, state_.historyFilterText);
                if (!matched) {
                    continue;
                }
            }
            state_.filteredHistoryIndices.push_back(i);
        }
    }

    void syncHistoryState() {
        rebuildFilteredHistoryIndices();
        ui_->set_history_total_count(static_cast<int>(state_.history.size()));
        ui_->set_history_filter_text(detail::SlintFromUtf8(state_.historyFilterText));
        ui_->set_history_category_filter(state_.historyCategoryFilter);

        std::vector<HistoryItemUi> rows;
        rows.reserve(state_.filteredHistoryIndices.size());
        for (int srcIdx : state_.filteredHistoryIndices) {
            const auto& h = state_.history[static_cast<std::size_t>(srcIdx)];
            HistoryItemUi row;
            row.source_index = srcIdx;
            row.timestamp = detail::SlintFromUtf8(h.timestamp);
            row.type_code = detail::SlintFromUtf8(h.type);
            row.link_path = detail::SlintFromUtf8(h.linkPath);
            row.target_path = detail::SlintFromUtf8(h.targetPath);
            row.result = detail::SlintFromUtf8(h.result);
            row.success = h.success;
            rows.push_back(std::move(row));
        }
        ui_->set_history_rows(
            std::make_shared<slint::VectorModel<HistoryItemUi>>(std::move(rows)));
    }

    void openConfirmation(ConfirmAction action, const std::string& title,
                          const std::string& message, const std::string& primaryLabel) {
        state_.confirmAction = action;
        state_.confirmTitle = title;
        state_.confirmMessage = message;
        state_.confirmPrimaryLabel = primaryLabel;
        ui_->set_confirm_title(detail::SlintFromUtf8(title));
        ui_->set_confirm_message(detail::SlintFromUtf8(message));
        ui_->set_confirm_primary_label(detail::SlintFromUtf8(primaryLabel));
        ui_->set_confirm_open(true);
    }

    void launchCreateWorker() {
        if (createBusy_) {
            return;
        }
        createBusy_ = true;
        const OperationMode mode = state_.operationMode;
        const LinkType type = state_.linkType;
        const std::string linkPath = state_.linkPath;
        const std::string targetPath = state_.targetPath;
        const bool autoParent = state_.autoCreateParent;

        state_.consoleOutput =
            (mode == OperationMode::MoveAndLink
                ? "正在迁移数据并建立链接...\nrobocopy \"" + win::CleanPathInput(linkPath) +
                  "\" -> \"" + win::CleanPathInput(targetPath) + "\"\n"
                : "") +
            win::BuildMklinkCommand(type, linkPath, targetPath) + "\n\n正在等待 Windows 完成操作...";
        syncCreateState();

        slint::ComponentWeakHandle<AppWindow> weak(ui_);
        auto alive = alive_;
        std::thread([this, alive, weak, mode, type, linkPath, targetPath, autoParent]() {
            CommandRunResult result = (mode == OperationMode::MoveAndLink)
                ? win::ExecuteMoveAndLinkSync(type, linkPath, targetPath, autoParent)
                : win::ExecuteMklinkSync(type, linkPath, targetPath, autoParent);

            slint::invoke_from_event_loop([this, alive, weak, mode, res = std::move(result)]() mutable {
                if (!alive->load(std::memory_order_relaxed) || !weak.lock()) {
                    return;
                }
                createBusy_ = false;
                state_.consoleOutput = "$ " + res.commandDisplay + "\n\n" +
                    (res.output.empty() ? "(无控制台输出)" : res.output);

                HistoryItem hist;
                hist.timestamp = win::FormatTimestampNow();
                hist.type = res.linkTypeCode;
                hist.linkPath = res.resolvedLinkPath;
                hist.targetPath = res.resolvedTargetPath;
                hist.result = res.summary;
                hist.success = res.success;
                win::AppendHistoryEntry(hist);
                state_.history.push_back(std::move(hist));
                state_.historyCategoryFilter = 0;

                if (res.success) {
                    lastCreatedLink_ = res.resolvedLinkPath;
                    if (mode == OperationMode::MoveAndLink) {
                        state_.operationMode = OperationMode::CreateOnly;
                    }
                    showToast(res.summary, res.resolvedLinkPath, 1);
                } else {
                    lastCreatedLink_.clear();
                    showToast(res.summary, "请查看下方执行详情了解权限或路径原因。", 3);
                }
                syncPrivileges();
                syncCreateState();
                syncHistoryState();
            });
        }).detach();
    }

    void applyQuickFixAction() {
        pullCreateInputsFromUi();
        switch (cachedQuickFix_) {
            case FixAction::SwitchToDirectory:
                state_.linkType = LinkType::Directory;
                persistSettings();
                break;
            case FixAction::SwitchToJunction:
                state_.linkType = LinkType::Junction;
                persistSettings();
                break;
            case FixAction::SwitchToHardLink:
                state_.linkType = LinkType::HardLink;
                persistSettings();
                break;
            case FixAction::SwitchToFileSymlink:
                state_.linkType = LinkType::FileSymlink;
                persistSettings();
                break;
            case FixAction::EnableAutoCreateParent:
                state_.autoCreateParent = true;
                persistSettings();
                break;
            case FixAction::AppendTargetNameToLink: {
                const std::string cleanLink = win::CleanPathInput(state_.linkPath);
                const std::string cleanTarget = win::CleanPathInput(state_.targetPath);
                const auto name = std::filesystem::path(win::Utf8ToWide(cleanTarget)).filename();
                if (!cleanLink.empty() && !name.empty()) {
                    state_.linkPath = win::WideToUtf8(
                        (std::filesystem::path(win::Utf8ToWide(cleanLink)) / name).wstring());
                }
                break;
            }
            case FixAction::AppendLinkFolderNameToTarget: {
                const std::string cleanLink = win::CleanPathInput(state_.linkPath);
                const std::string cleanTarget = win::CleanPathInput(state_.targetPath);
                const auto name = std::filesystem::path(win::Utf8ToWide(cleanLink)).filename();
                if (!cleanTarget.empty() && !name.empty()) {
                    state_.targetPath = win::WideToUtf8(
                        (std::filesystem::path(win::Utf8ToWide(cleanTarget)) / name).wstring());
                }
                break;
            }
            case FixAction::None:
                break;
        }
        syncCreateState();
    }

    void inspectSingleNow(const std::string& rawPath) {
        const std::string clean = win::CleanPathInput(rawPath);
        if (clean.empty()) {
            showToast("提示", "请先输入或选择需要体检的路径。", 2);
            return;
        }
        state_.inspectQueryPath = clean;
        state_.inspectedSingle = win::InspectSinglePath(clean);
        state_.hasInspectedSingle = true;
        state_.inspectStatusText = "单路径剖析完成：" + state_.inspectedSingle.kindLabel;
        syncInspectState();
    }

    void startDirectoryScanNow() {
        const std::string clean = win::CleanPathInput(detail::Utf8FromSlint(ui_->get_inspect_query_path()));
        state_.inspectQueryPath = clean;
        state_.inspectIncludeHardLinks = ui_->get_inspect_include_hard_links();
        state_.inspectMaxDepth = ui_->get_inspect_max_depth();

        if (clean.empty()) {
            showToast("提示", "请先选择一个存在的文件夹再开始扫描。", 2);
            return;
        }
        DWORD attrs = INVALID_FILE_ATTRIBUTES;
        if (!win::PathExistsNoFollow(win::Utf8ToWide(clean), &attrs) ||
            (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            showToast("路径无效", "目录扫描仅支持已存在的文件夹路径。", 3);
            return;
        }

        if (scanCancel_) {
            scanCancel_->store(true, std::memory_order_relaxed);
        }
        scanCancel_ = std::make_shared<std::atomic_bool>(false);
        auto cancelToken = scanCancel_;
        inspectScanning_ = true;
        state_.inspectStatusText = "正在后台深度扫描目录中的链接...";
        syncInspectState();

        const bool includeHard = state_.inspectIncludeHardLinks;
        const int maxDepth = state_.inspectMaxDepth;
        slint::ComponentWeakHandle<AppWindow> weak(ui_);
        auto alive = alive_;

        std::thread([this, alive, weak, clean, includeHard, maxDepth, cancelToken]() {
            auto found = win::ScanDirectoryForLinks(clean, includeHard, maxDepth, cancelToken);
            const bool canceled = cancelToken->load(std::memory_order_relaxed);

            slint::invoke_from_event_loop([this, alive, weak, canceled, results = std::move(found)]() mutable {
                if (!alive->load(std::memory_order_relaxed) || !weak.lock()) {
                    return;
                }
                inspectScanning_ = false;
                if (canceled) {
                    state_.inspectStatusText = "已取消扫描，保留此前部分结果。";
                } else {
                    state_.scannedLinks = std::move(results);
                    state_.inspectStatusText = state_.scannedLinks.empty()
                        ? "扫描完成：当前深度范围内未发现符号链接、目录联接或硬链接。"
                        : "扫描完成：共发现 " + std::to_string(state_.scannedLinks.size()) + " 项链接记录。";
                }
                syncInspectState();
            });
        }).detach();
    }

    void requestUnlinkForPath(const std::string& rawPath) {
        const InspectedLink info = win::InspectSinglePath(rawPath);
        if (!info.exists || info.kindCode == "NORMAL" || info.kindCode == "MISSING" || info.kindCode == "REPARSE") {
            showToast("已安全拦截", "只允许解除明确识别为符号链接、目录联接或硬链接的入口。", 2);
            return;
        }
        state_.pendingUnlinkPath = info.path;
        openConfirmation(
            ConfirmAction::UnlinkItem,
            "确认安全解除链接",
            "即将仅移除链接入口 [" + info.kindLabel + "]：\n" + info.path +
                "\n\n实际指向的数据目标 (" +
                (info.targetPath.empty() ? "同分区硬链接兄妹文件" : info.targetPath) +
                ") 不会被修改或删除。",
            "确认解除链接");
    }

    void executeConfirmedAction() {
        const ConfirmAction action = state_.confirmAction;
        state_.confirmAction = ConfirmAction::None;
        ui_->set_confirm_open(false);

        if (action == ConfirmAction::MoveAndLink) {
            pullCreateInputsFromUi();
            launchCreateWorker();
        } else if (action == ConfirmAction::UnlinkItem) {
            const std::string targetPath = state_.pendingUnlinkPath;
            state_.pendingUnlinkPath.clear();
            std::string msg;
            if (win::SafeUnlinkPath(targetPath, msg)) {
                if (state_.hasInspectedSingle &&
                    _stricmp(state_.inspectedSingle.path.c_str(), targetPath.c_str()) == 0) {
                    state_.inspectedSingle = win::InspectSinglePath(targetPath);
                }
                const auto it = std::remove_if(
                    state_.scannedLinks.begin(), state_.scannedLinks.end(),
                    [&](const InspectedLink& item) {
                        return _stricmp(item.path.c_str(), targetPath.c_str()) == 0;
                    });
                state_.scannedLinks.erase(it, state_.scannedLinks.end());
                state_.inspectStatusText = msg;
                syncInspectState();
                showToast("已安全解除链接", msg, 1);
            } else {
                showToast("解除链接失败", msg, 3);
            }
        } else if (action == ConfirmAction::ClearHistory) {
            state_.history.clear();
            win::WriteAllHistory(win::GetHistoryFilePath(), state_.history);
            syncHistoryState();
            showToast("已清空记录", "本地操作历史已重置。", 0);
        }
    }

    void bindCallbacks() {
        ui_->on_toggle_theme([this]() {
            state_.darkTheme = !state_.darkTheme;
            persistSettings();
            syncTheme();
        });

        ui_->on_tab_changed([this](int tab) {
            state_.activeTab = tab;
            syncPrivileges();
            syncCreateState();
        });

        ui_->on_create_mode_changed([this](int mode) {
            state_.operationMode = static_cast<OperationMode>(std::clamp(mode, 0, 1));
            pullCreateInputsFromUi();
            if (state_.operationMode == OperationMode::MoveAndLink &&
                (state_.linkType == LinkType::HardLink || state_.linkType == LinkType::FileSymlink)) {
                state_.linkType = LinkType::Junction;
                persistSettings();
            }
            syncCreateState();
        });

        ui_->on_create_link_type_changed([this](int t) {
            state_.linkType = static_cast<LinkType>(std::clamp(t, 0, 3));
            persistSettings();
            pullCreateInputsFromUi();
            syncCreateState();
        });

        ui_->on_create_parameters_edited([this]() {
            pullCreateInputsFromUi();
            syncCreateState();
        });

        ui_->on_create_swap_paths([this]() {
            pullCreateInputsFromUi();
            std::swap(state_.linkPath, state_.targetPath);
            syncCreateState();
        });

        ui_->on_create_browse_link_save([this]() {
            pullCreateInputsFromUi();
            const bool folderStyle = (state_.linkType == LinkType::Directory ||
                                      state_.linkType == LinkType::Junction);
            if (win::ChooseNewLinkPath(state_.linkPath, folderStyle)) {
                syncCreateState();
            }
        });

        ui_->on_create_browse_link_folder([this]() {
            pullCreateInputsFromUi();
            const std::string picked = win::BrowseDirectory("选择文件夹路径", state_.linkPath);
            if (!picked.empty()) {
                state_.linkPath = picked;
                syncCreateState();
            }
        });

        ui_->on_create_browse_link_file([this]() {
            pullCreateInputsFromUi();
            const std::string picked = win::BrowseFile("选择文件路径", state_.linkPath);
            if (!picked.empty()) {
                state_.linkPath = picked;
                syncCreateState();
            }
        });

        ui_->on_create_browse_target_folder([this]() {
            pullCreateInputsFromUi();
            const std::string picked = win::BrowseDirectory("选择真实目标文件夹", state_.targetPath);
            if (!picked.empty()) {
                state_.targetPath = picked;
                syncCreateState();
            }
        });

        ui_->on_create_browse_target_file([this]() {
            pullCreateInputsFromUi();
            const std::string picked = win::BrowseFile("选择真实目标文件", state_.targetPath);
            if (!picked.empty()) {
                state_.targetPath = picked;
                syncCreateState();
            }
        });

        ui_->on_create_apply_quick_fix([this]() {
            applyQuickFixAction();
        });

        ui_->on_create_execute([this]() {
            pullCreateInputsFromUi();
            const ValidationResult val = win::ValidateCreateInputs(state_);
            if (!val.canExecute) {
                showToast("参数待完善", val.message, 2);
                return;
            }
            if (state_.operationMode == OperationMode::MoveAndLink) {
                const std::string src = win::CleanPathInput(state_.linkPath);
                const std::string dst = win::CleanPathInput(state_.targetPath);
                openConfirmation(
                    ConfirmAction::MoveAndLink,
                    "确认执行安全搬家并建链",
                    "即将把原文件夹完整迁移到目标盘：\n" + src +
                        "\n\n迁移至：\n" + dst +
                        "\n\n完成后会在原位置创建 " + linkTypeDisplayName(state_.linkType) +
                        " 保持兼容。请确认无其他程序正在占用该目录。",
                    "确认迁移并建链");
                return;
            }
            launchCreateWorker();
        });

        ui_->on_create_copy_command([this]() {
            const std::string cmd = detail::Utf8FromSlint(ui_->get_command_preview());
            if (win::CopyTextToClipboard(cmd)) {
                showToast("已复制命令", "完整命令已复制到剪贴板。", 1);
            }
        });

        ui_->on_create_fill_example([this]() {
            pullCreateInputsFromUi();
            if (state_.linkType == LinkType::HardLink || state_.linkType == LinkType::FileSymlink) {
                state_.linkPath = "C:\\GameMods\\SharedConfig_Link.json";
                state_.targetPath = "D:\\MasterConfigs\\SharedConfig.json";
            } else if (state_.operationMode == OperationMode::MoveAndLink) {
                state_.linkPath = "C:\\Users\\Public\\DemoAppCache";
                state_.targetPath = "D:\\MovedData\\DemoAppCache";
            } else {
                state_.linkPath = "C:\\Workspace\\ProjectAssets";
                state_.targetPath = "D:\\Storage\\ProjectAssets";
            }
            syncCreateState();
        });

        ui_->on_create_clear_inputs([this]() {
            state_.linkPath.clear();
            state_.targetPath.clear();
            state_.consoleOutput.clear();
            lastCreatedLink_.clear();
            syncCreateState();
        });

        ui_->on_create_reveal_created([this]() {
            if (!lastCreatedLink_.empty()) {
                win::RevealInExplorer(lastCreatedLink_);
            }
        });

        ui_->on_create_inspect_created([this]() {
            if (!lastCreatedLink_.empty()) {
                ui_->set_active_tab(1);
                state_.activeTab = 1;
                inspectSingleNow(lastCreatedLink_);
            }
        });

        // Inspect Callbacks
        ui_->on_inspect_browse_folder([this]() {
            const std::string cur = detail::Utf8FromSlint(ui_->get_inspect_query_path());
            const std::string picked = win::BrowseDirectory("选择需要体检或扫描的文件夹", cur);
            if (!picked.empty()) {
                state_.inspectQueryPath = picked;
                syncInspectState();
            }
        });

        ui_->on_inspect_browse_file([this]() {
            const std::string cur = detail::Utf8FromSlint(ui_->get_inspect_query_path());
            const std::string picked = win::BrowseFile("选择需要剖析的文件", cur);
            if (!picked.empty()) {
                inspectSingleNow(picked);
            }
        });

        ui_->on_inspect_single([this]() {
            inspectSingleNow(detail::Utf8FromSlint(ui_->get_inspect_query_path()));
        });

        ui_->on_inspect_start_scan([this]() {
            startDirectoryScanNow();
        });

        ui_->on_inspect_cancel_scan([this]() {
            if (scanCancel_) {
                scanCancel_->store(true, std::memory_order_relaxed);
            }
        });

        ui_->on_inspect_specific([this](slint::SharedString p) {
            inspectSingleNow(detail::Utf8FromSlint(p));
        });

        ui_->on_inspect_request_unlink([this](slint::SharedString p) {
            requestUnlinkForPath(detail::Utf8FromSlint(p));
        });

        // History Callbacks
        ui_->on_history_filter_changed([this]() {
            state_.historyFilterText = detail::Utf8FromSlint(ui_->get_history_filter_text());
            state_.historyCategoryFilter = ui_->get_history_category_filter();
            syncHistoryState();
        });

        ui_->on_history_request_clear([this]() {
            openConfirmation(
                ConfirmAction::ClearHistory,
                "清空所有执行记录",
                "此操作将清除本地历史记录文件中的所有条目（不会影响已经创建的实际文件系统链接）。",
                "确认清空");
        });

        ui_->on_history_backfill([this](int srcIdx) {
            if (srcIdx < 0 || srcIdx >= static_cast<int>(state_.history.size())) {
                return;
            }
            const auto& item = state_.history[static_cast<std::size_t>(srcIdx)];
            if (win::ContainsCaseInsensitive(item.type, "/D")) {
                state_.linkType = LinkType::Directory;
            } else if (win::ContainsCaseInsensitive(item.type, "/H")) {
                state_.linkType = LinkType::HardLink;
            } else if (win::ContainsCaseInsensitive(item.type, "FILE")) {
                state_.linkType = LinkType::FileSymlink;
            } else {
                state_.linkType = LinkType::Junction;
            }
            state_.operationMode = OperationMode::CreateOnly;
            state_.linkPath = item.linkPath;
            state_.targetPath = item.targetPath;
            state_.activeTab = 0;
            ui_->set_active_tab(0);
            syncCreateState();
            showToast("已回填参数", "已切换回新建页并填入历史路径与链接类型。", 0);
        });

        ui_->on_history_inspect_item([this](slint::SharedString p) {
            state_.activeTab = 1;
            ui_->set_active_tab(1);
            inspectSingleNow(detail::Utf8FromSlint(p));
        });

        // Guide / Shared Callbacks
        ui_->on_relaunch_admin([this]() {
            if (win::RelaunchAsAdmin()) {
                slint::quit_event_loop();
            } else {
                showToast("提权未完成", "UAC 提权请求被取消或未获授权。", 2);
            }
        });

        ui_->on_open_dev_settings([this]() {
            win::OpenWindowsDeveloperSettings();
        });

        ui_->on_refresh_privileges([this]() {
            syncPrivileges();
            syncCreateState();
            showToast("状态已刷新", "已重新读取当前 Windows 权限与开发者模式状态。", 0);
        });

        ui_->on_reveal_path([this](slint::SharedString p) {
            if (!win::RevealInExplorer(detail::Utf8FromSlint(p))) {
                showToast("定位失败", "指定路径及其上级目录目前均不存在。", 2);
            }
        });

        ui_->on_copy_text([this](slint::SharedString t) {
            if (win::CopyTextToClipboard(detail::Utf8FromSlint(t))) {
                showToast("已复制", "内容已复制到剪贴板。", 1);
            }
        });

        ui_->on_confirm_accepted([this]() {
            executeConfirmedAction();
        });

        ui_->on_confirm_canceled([this]() {
            state_.confirmAction = ConfirmAction::None;
            state_.pendingUnlinkPath.clear();
            ui_->set_confirm_open(false);
        });

        ui_->on_dismiss_toast([this]() {
            ui_->set_toast_visible(false);
        });
    }

    FixAction cachedQuickFix_ = FixAction::None;
};

} // namespace fl
