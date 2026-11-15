#pragma once

#include "eui_neo.h"
#include "linker_types.h"
#include "ui_theme.h"
#include "win_linker.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fl::actions {

inline std::string CompactPathForTable(const std::string& path, std::size_t maxLen = 48) {
    if (path.size() <= maxLen || maxLen < 12) {
        return path;
    }
    const std::size_t keepEnd = maxLen - 14;
    return path.substr(0, 11) + "..." + path.substr(path.size() - keepEnd);
}

inline void PersistSettings(const AppState& state) {
    AppSettings s;
    s.darkTheme = state.darkTheme;
    s.defaultLinkType = static_cast<int>(state.linkType);
    s.autoCreateParent = state.autoCreateParent;
    win::SaveSettings(s);
}

inline void RefreshPrivileges(AppState& state) {
    state.isAdmin = win::IsRunningAsAdmin();
    state.isDevMode = win::IsDeveloperModeEnabled();
}

inline void RebuildHistoryTable(AppState& state) {
    auto rows = std::make_shared<components::DataTableRows>();
    state.filteredHistoryIndices.clear();

    const int previouslySelectedOrig = (state.selectedHistoryIndex >= 0 &&
                                        state.selectedHistoryIndex < static_cast<int>(state.history.size()))
        ? state.selectedHistoryIndex
        : -1;
    int newSelectedOrig = -1;

    for (int i = static_cast<int>(state.history.size()) - 1; i >= 0; --i) {
        const auto& item = state.history[static_cast<std::size_t>(i)];

        bool catMatch = true;
        switch (state.historyCategoryFilter) {
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

        if (!state.historyFilterText.empty()) {
            const bool textMatch =
                win::ContainsCaseInsensitive(item.timestamp, state.historyFilterText) ||
                win::ContainsCaseInsensitive(item.type, state.historyFilterText) ||
                win::ContainsCaseInsensitive(item.linkPath, state.historyFilterText) ||
                win::ContainsCaseInsensitive(item.targetPath, state.historyFilterText) ||
                win::ContainsCaseInsensitive(item.result, state.historyFilterText);
            if (!textMatch) {
                continue;
            }
        }

        state.filteredHistoryIndices.push_back(i);
        if (i == previouslySelectedOrig) {
            newSelectedOrig = i;
        }
        rows->push_back({
            item.timestamp,
            item.type,
            CompactPathForTable(item.linkPath, 44),
            CompactPathForTable(item.targetPath, 44),
            item.result
        });
    }

    if (newSelectedOrig >= 0) {
        state.selectedHistoryIndex = newSelectedOrig;
    } else if (!state.filteredHistoryIndices.empty()) {
        state.selectedHistoryIndex = state.filteredHistoryIndices.front();
    } else {
        state.selectedHistoryIndex = -1;
    }

    state.historyTableRows = std::move(rows);
    ++state.historyTableRevision;
}

inline void RebuildInspectTable(AppState& state) {
    auto rows = std::make_shared<components::DataTableRows>();
    for (const auto& item : state.scannedLinks) {
        const std::string health = item.targetExists ? "正常" : "目标缺失";
        const std::string targetDesc = item.kindCode == "/H"
            ? (std::to_string(item.hardLinkCount) + " 个入口 | " +
               (item.targetPath.empty() ? "-" : CompactPathForTable(item.targetPath, 42)))
            : (item.targetPath.empty() ? "-" : CompactPathForTable(item.targetPath, 48));
        rows->push_back({
            item.kindCode,
            health,
            CompactPathForTable(item.path, 48),
            targetDesc
        });
    }

    if (state.scannedLinks.empty()) {
        state.selectedInspectIndex = -1;
    } else if (state.selectedInspectIndex < 0 ||
               state.selectedInspectIndex >= static_cast<int>(state.scannedLinks.size())) {
        state.selectedInspectIndex = 0;
    }

    state.inspectTableRows = std::move(rows);
    ++state.inspectTableRevision;
}

inline void InitializeAppState(AppState& state) {
    const AppSettings settings = win::LoadSettings();
    state.darkTheme = settings.darkTheme;
    state.linkType = static_cast<LinkType>(std::clamp(settings.defaultLinkType, 0, 3));
    state.autoCreateParent = settings.autoCreateParent;
    RefreshPrivileges(state);
    state.history = win::LoadHistoryWithMigration();
    RebuildHistoryTable(state);
    RebuildInspectTable(state);
}

inline void RecordOperationResult(AppState& state, const CommandRunResult& res) {
    HistoryItem item;
    item.timestamp = win::FormatTimestampNow();
    item.type = res.linkTypeCode;
    item.linkPath = res.resolvedLinkPath;
    item.targetPath = res.resolvedTargetPath;
    item.result = res.summary;
    item.success = res.success;

    win::AppendHistoryEntry(item);
    state.history.push_back(std::move(item));
    state.historyCategoryFilter = 0;
    RebuildHistoryTable(state);
}

inline void LaunchCreateExecution(AppState& state, OperationMode mode, LinkType type,
                                  const std::string& linkPath, const std::string& targetPath,
                                  bool autoCreateParent) {
    if (app::async::running("fl.create")) {
        return;
    }

    state.statusText = mode == OperationMode::MoveAndLink ? "正在迁移文件夹并创建链接..." : "正在创建链接...";
    state.statusTone = 0;
    state.consoleOutput = "执行命令:\n" +
        (mode == OperationMode::MoveAndLink
            ? "[IFileOperation Move] \"" + win::CleanPathInput(linkPath) + "\" -> \"" + win::CleanPathInput(targetPath) + "\"\n"
            : "") +
        win::BuildMklinkCommand(type, linkPath, targetPath) + "\n\n等待 Windows 响应...";

    AppState* statePtr = &state;
    app::async::runOnce(
        "fl.create",
        [mode, type, linkPath, targetPath, autoCreateParent]() -> CommandRunResult {
            if (mode == OperationMode::MoveAndLink) {
                return win::ExecuteMoveAndLinkSync(type, linkPath, targetPath, autoCreateParent);
            }
            return win::ExecuteMklinkSync(type, linkPath, targetPath, autoCreateParent);
        },
        [statePtr, mode](const app::async::Result<CommandRunResult>& asyncRes) {
            if (!asyncRes.ok) {
                statePtr->statusText = "执行异常";
                statePtr->statusTone = 3;
                statePtr->consoleOutput = asyncRes.error;
                ui_kit::TriggerToast(*statePtr, "执行异常", asyncRes.error, 3, 0xF057u);
                return;
            }
            const CommandRunResult& r = asyncRes.value;
            statePtr->statusText = r.summary;
            statePtr->statusTone = r.success ? 1 : 3;
            statePtr->consoleOutput = "$ " + r.commandDisplay + "\n\n" +
                (r.output.empty() ? "(无控制台输出)" : r.output);

            RecordOperationResult(*statePtr, r);

            if (r.success) {
                if (mode == OperationMode::MoveAndLink) {
                    statePtr->operationMode = OperationMode::CreateOnly;
                }
                ui_kit::TriggerToast(*statePtr, r.summary, r.resolvedLinkPath, 1, 0xF058u);
            } else {
                ui_kit::TriggerToast(*statePtr, r.summary, "请查看右侧终端回显排查权限或路径占用问题。", 3, 0xF057u);
            }
        });
}

inline void ExecuteOrConfirmCreate(AppState& state) {
    const ValidationResult val = win::ValidateCreateInputs(state);
    if (!val.canExecute) {
        ui_kit::TriggerToast(state, "无法执行", val.message, 2, 0xF071u);
        return;
    }

    if (state.operationMode == OperationMode::MoveAndLink) {
        const std::string srcClean = win::CleanPathInput(state.linkPath);
        const std::string dstClean = win::CleanPathInput(state.targetPath);
        state.confirmAction = ConfirmAction::MoveAndLink;
        state.confirmTitle = "确认执行目录搬家并建链";
        state.confirmMessage =
            "即将把现有文件夹:\n" + srcClean +
            "\n\n完整迁移至新位置:\n" + dstClean +
            "\n\n并在原位置建立 " + linkTypeDisplayName(state.linkType) +
            " 指向新路径。请先确保没有程序正在读写该目录内的文件。";
        state.confirmPrimaryLabel = "确认迁移并建链";
        state.confirmOpen = true;
        return;
    }

    LaunchCreateExecution(state, state.operationMode, state.linkType,
                          state.linkPath, state.targetPath, state.autoCreateParent);
}

inline void ApplyQuickFix(AppState& state, FixAction action) {
    switch (action) {
        case FixAction::SwitchToDirectory:
            state.linkType = LinkType::Directory;
            PersistSettings(state);
            break;
        case FixAction::SwitchToJunction:
            state.linkType = LinkType::Junction;
            PersistSettings(state);
            break;
        case FixAction::SwitchToHardLink:
            state.linkType = LinkType::HardLink;
            PersistSettings(state);
            break;
        case FixAction::SwitchToFileSymlink:
            state.linkType = LinkType::FileSymlink;
            PersistSettings(state);
            break;
        case FixAction::EnableAutoCreateParent:
            state.autoCreateParent = true;
            PersistSettings(state);
            break;
        case FixAction::AppendTargetNameToLink: {
            const std::string cleanLink = win::CleanPathInput(state.linkPath);
            const std::string cleanTarget = win::CleanPathInput(state.targetPath);
            const auto name = std::filesystem::path(win::Utf8ToWide(cleanTarget)).filename();
            if (!cleanLink.empty() && !name.empty()) {
                const std::filesystem::path combined = std::filesystem::path(win::Utf8ToWide(cleanLink)) / name;
                state.linkPath = win::WideToUtf8(combined.wstring());
            }
            break;
        }
        case FixAction::AppendLinkFolderNameToTarget: {
            const std::string cleanLink = win::CleanPathInput(state.linkPath);
            const std::string cleanTarget = win::CleanPathInput(state.targetPath);
            const auto name = std::filesystem::path(win::Utf8ToWide(cleanLink)).filename();
            if (!cleanTarget.empty() && !name.empty()) {
                const std::filesystem::path combined = std::filesystem::path(win::Utf8ToWide(cleanTarget)) / name;
                state.targetPath = win::WideToUtf8(combined.wstring());
            }
            break;
        }
        case FixAction::None:
            break;
    }
}

inline void InspectPathNow(AppState& state, const std::string& rawPath) {
    const std::string clean = win::CleanPathInput(rawPath);
    if (clean.empty()) {
        ui_kit::TriggerToast(state, "提示", "请先输入或选择需要分析的路径。", 2, 0xF05Au);
        return;
    }
    state.inspectQueryPath = clean;
    state.inspectedSingle = win::InspectSinglePath(clean);
    state.hasInspectedSingle = true;
    state.inspectStatusText = "已完成单路径分析：" + state.inspectedSingle.kindLabel;
}

inline void StartDirectoryScan(AppState& state) {
    const std::string clean = win::CleanPathInput(state.inspectQueryPath);
    if (clean.empty()) {
        ui_kit::TriggerToast(state, "提示", "请先选择一个存在的文件夹再开始扫描。", 2, 0xF05Au);
        return;
    }
    const std::wstring wclean = win::Utf8ToWide(clean);
    DWORD attrs = INVALID_FILE_ATTRIBUTES;
    if (!win::PathExistsNoFollow(wclean, &attrs) || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        ui_kit::TriggerToast(state, "路径无效", "递归扫描仅支持已存在的文件夹路径。", 3, 0xF057u);
        return;
    }

    state.inspectQueryPath = clean;
    state.inspectStatusText = "正在扫描目录中的符号链接与硬链接...";
    const bool includeHard = state.inspectIncludeHardLinks;
    const int depth = state.inspectMaxDepth;
    AppState* statePtr = &state;

    app::async::restart(
        "fl.scan",
        [clean, includeHard, depth](const core::async::CancelToken& cancel) {
            return win::ScanDirectoryForLinks(clean, includeHard, depth, cancel);
        },
        [statePtr, clean](const app::async::Result<std::vector<InspectedLink>>& res) {
            if (!res.ok) {
                statePtr->inspectStatusText = "扫描未能完成";
                return;
            }
            statePtr->scannedLinks = res.value;
            statePtr->selectedInspectIndex = statePtr->scannedLinks.empty() ? -1 : 0;
            RebuildInspectTable(*statePtr);
            if (statePtr->scannedLinks.empty()) {
                statePtr->inspectStatusText = "扫描完成：在当前深度范围内未发现符号链接、联接或硬链接。";
            } else {
                statePtr->inspectStatusText = "扫描完成：共发现 " +
                    std::to_string(statePtr->scannedLinks.size()) + " 个链接项目。";
            }
        });
}

inline void RequestUnlinkItem(AppState& state, const InspectedLink& targetLink) {
    if (!targetLink.exists || targetLink.kindCode == "NORMAL" || targetLink.kindCode == "MISSING" || targetLink.kindCode == "REPARSE") {
        ui_kit::TriggerToast(state, "已安全拦截", "只允许解除明确识别为符号链接、目录联接或硬链接的入口。", 2, 0xF071u);
        return;
    }
    state.pendingUnlinkPath = targetLink.path;
    state.confirmAction = ConfirmAction::UnlinkItem;
    state.confirmTitle = "确认安全解除链接";
    state.confirmMessage =
        "即将移除链接入口 [" + targetLink.kindLabel + "]:\n" + targetLink.path +
        "\n\n系统将仅移除此链接入口，指向的真实目标位置 (" +
        (targetLink.targetPath.empty() ? "兄妹硬链接共享数据" : targetLink.targetPath) +
        ") 中的数据完全不受任何影响。";
    state.confirmPrimaryLabel = "确认解除链接";
    state.confirmOpen = true;
}

inline void ExecuteConfirmedDialogAction(AppState& state) {
    const ConfirmAction action = state.confirmAction;
    state.confirmOpen = false;
    state.confirmAction = ConfirmAction::None;

    if (action == ConfirmAction::MoveAndLink) {
        LaunchCreateExecution(state, OperationMode::MoveAndLink, state.linkType,
                              state.linkPath, state.targetPath, state.autoCreateParent);
    } else if (action == ConfirmAction::UnlinkItem) {
        std::string msg;
        const std::string targetPath = state.pendingUnlinkPath;
        state.pendingUnlinkPath.clear();
        if (win::SafeUnlinkPath(targetPath, msg)) {
            if (state.hasInspectedSingle && _stricmp(state.inspectedSingle.path.c_str(), targetPath.c_str()) == 0) {
                state.inspectedSingle = win::InspectSinglePath(targetPath);
            }
            const auto it = std::remove_if(state.scannedLinks.begin(), state.scannedLinks.end(),
                                           [&](const InspectedLink& item) {
                                               return _stricmp(item.path.c_str(), targetPath.c_str()) == 0;
                                           });
            if (it != state.scannedLinks.end()) {
                state.scannedLinks.erase(it, state.scannedLinks.end());
                RebuildInspectTable(state);
            }
            state.inspectStatusText = msg;
            ui_kit::TriggerToast(state, "已解除链接", msg, 1, 0xF058u);
        } else {
            ui_kit::TriggerToast(state, "解除失败", msg, 3, 0xF057u);
        }
    } else if (action == ConfirmAction::ClearHistory) {
        state.history.clear();
        win::WriteAllHistory(win::GetHistoryFilePath(), state.history);
        RebuildHistoryTable(state);
        ui_kit::TriggerToast(state, "已清空历史记录", "本地操作日志已重置。", 0, 0xF05Au);
    }
}

} // namespace fl::actions
