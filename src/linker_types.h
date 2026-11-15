#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fl {

enum class LinkType {
    Directory = 0,   // /D
    Junction = 1,    // /J
    HardLink = 2,    // /H
    FileSymlink = 3  // (no flag)
};

enum class OperationMode {
    CreateOnly = 0,
    MoveAndLink = 1
};

enum class ValidationLevel {
    Info = 0,
    Valid = 1,
    Warning = 2,
    Error = 3
};

enum class FixAction {
    None = 0,
    SwitchToDirectory,
    SwitchToJunction,
    SwitchToHardLink,
    SwitchToFileSymlink,
    EnableAutoCreateParent,
    AppendLinkFolderNameToTarget,
    AppendTargetNameToLink
};

struct ValidationResult {
    ValidationLevel level = ValidationLevel::Info;
    std::string message;
    FixAction quickFix = FixAction::None;
    std::string quickFixLabel;
    bool canExecute = false;
    bool requiresConfirm = false;
};

struct HistoryItem {
    std::string timestamp;
    std::string type;
    std::string linkPath;
    std::string targetPath;
    std::string result;
    bool success = false;
};

struct InspectedLink {
    std::string path;
    std::string kindCode;      // "/D", "/J", "/H", "FILE", "NORMAL", "MISSING"
    std::string kindLabel;     // Human readable label
    std::string targetPath;
    bool exists = false;
    bool isDirectory = false;
    bool isReparsePoint = false;
    bool isRelative = false;
    bool targetExists = false;
    unsigned long hardLinkCount = 0;
    std::vector<std::string> hardLinkSiblings;
    std::string detailNote;
};

struct CommandRunResult {
    bool success = false;
    unsigned long exitCode = 1;
    std::string commandDisplay;
    std::string output;
    std::string summary;
    std::string resolvedLinkPath;
    std::string resolvedTargetPath;
    std::string linkTypeCode;
};

struct AppSettings {
    bool darkTheme = true;
    int defaultLinkType = static_cast<int>(LinkType::Junction);
    bool autoCreateParent = true;
};

enum class ConfirmAction {
    None = 0,
    MoveAndLink,
    UnlinkItem,
    ClearHistory
};

struct AppState {
    // Theme & Navigation
    bool darkTheme = true;
    int activeTab = 0; // 0=Create, 1=Inspect, 2=History, 3=Guide

    // Create Page state
    OperationMode operationMode = OperationMode::CreateOnly;
    LinkType linkType = LinkType::Junction;
    std::string linkPath;
    std::string targetPath;
    bool autoCreateParent = true;
    std::string statusText = "就绪";
    int statusTone = 0; // 0=neutral, 1=success, 2=warning, 3=error
    std::string consoleOutput;

    // Validation cache (avoids running filesystem syscalls on every compose/scroll frame)
    ValidationResult cachedValidation;
    bool validationDirty = true;

    // Privilege cache
    bool isAdmin = false;
    bool isDevMode = false;

    // Inspect / Scanner Page state
    std::string inspectQueryPath;
    bool inspectIncludeHardLinks = true;
    int inspectMaxDepth = 4;
    InspectedLink inspectedSingle;
    bool hasInspectedSingle = false;
    std::vector<InspectedLink> scannedLinks;
    int selectedInspectIndex = -1;
    std::string inspectStatusText = "输入或选择路径后，可深入剖析单个链接属性或扫描整个文件夹下的符号链接。";

    // History Page state
    std::vector<HistoryItem> history;
    std::string historyFilterText;
    int historyCategoryFilter = 0; // 0=All, 1=/D, 2=/J, 3=/H, 4=FILE, 5=Success, 6=Fail
    std::vector<int> filteredHistoryIndices;
    int selectedHistoryIndex = -1;

    // Confirmation Dialog state
    bool confirmOpen = false;
    ConfirmAction confirmAction = ConfirmAction::None;
    std::string confirmTitle;
    std::string confirmMessage;
    std::string confirmPrimaryLabel = "确认执行";
    std::string pendingUnlinkPath;

    // Toast notification state
    bool toastVisible = false;
    std::string toastTitle;
    std::string toastMessage;
    int toastTone = 1; // 0=info, 1=success, 2=warning, 3=error
};

inline const char* linkTypeFlag(LinkType type) {
    switch (type) {
        case LinkType::Directory: return "/D";
        case LinkType::Junction: return "/J";
        case LinkType::HardLink: return "/H";
        case LinkType::FileSymlink: return "";
    }
    return "/J";
}

inline const char* linkTypeHistoryCode(LinkType type) {
    switch (type) {
        case LinkType::Directory: return "/D";
        case LinkType::Junction: return "/J";
        case LinkType::HardLink: return "/H";
        case LinkType::FileSymlink: return "FILE";
    }
    return "/J";
}

inline std::string linkTypeDisplayName(LinkType type) {
    switch (type) {
        case LinkType::Directory: return "目录符号链接 (/D)";
        case LinkType::Junction: return "目录联接 (/J)";
        case LinkType::HardLink: return "文件硬链接 (/H)";
        case LinkType::FileSymlink: return "文件符号链接";
    }
    return "目录联接 (/J)";
}

} // namespace fl
