#include "qt_backend.h"
#include "win_linker.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QWindow>

#include <dwmapi.h>
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <thread>
#include <utility>

namespace fl {

namespace {

QVariantMap inspectLinkToMap(const InspectedLink& item) {
    QVariantMap m;
    m.insert(QStringLiteral("path"), QString::fromStdString(item.path));
    m.insert(QStringLiteral("kindCode"), QString::fromStdString(item.kindCode));
    m.insert(QStringLiteral("kindLabel"), QString::fromStdString(item.kindLabel));
    m.insert(QStringLiteral("targetPath"), QString::fromStdString(item.targetPath));
    m.insert(QStringLiteral("detailNote"), QString::fromStdString(item.detailNote));
    m.insert(QStringLiteral("targetHealthy"), item.targetExists);
    m.insert(QStringLiteral("isReparse"), item.isReparsePoint);
    m.insert(QStringLiteral("isRelative"), item.isRelative);
    m.insert(QStringLiteral("hardLinkCount"), static_cast<int>(item.hardLinkCount));
    m.insert(QStringLiteral("canUnlink"), item.exists && (item.isReparsePoint || item.hardLinkCount > 1));

    QStringList siblings;
    siblings.reserve(static_cast<int>(item.hardLinkSiblings.size()));
    for (const auto& s : item.hardLinkSiblings) {
        siblings.push_back(QString::fromStdString(s));
    }
    m.insert(QStringLiteral("siblings"), siblings);
    return m;
}

} // namespace

QtLinkBackend::QtLinkBackend(QObject* parent)
    : QObject(parent) {
    const AppSettings settings = win::LoadSettings();
    state_.darkTheme = settings.darkTheme;
    state_.linkType = static_cast<LinkType>(std::clamp(settings.defaultLinkType, 0, 3));
    state_.autoCreateParent = settings.autoCreateParent;
    state_.history = win::LoadHistoryWithMigration();

    refreshPrivileges();
    refreshValidation();
    rebuildHistoryModel();
    rebuildInspectModels();
}

QtLinkBackend::~QtLinkBackend() {
    alive_->store(false, std::memory_order_relaxed);
    if (scanCancel_) {
        scanCancel_->store(true, std::memory_order_relaxed);
    }
}

void QtLinkBackend::attachWindow(QWindow* window) {
    mainWindow_ = window;
    applyWindowTitlebarTheme();
}

void QtLinkBackend::applyWindowTitlebarTheme() {
    if (!mainWindow_) {
        return;
    }
    const HWND hwnd = reinterpret_cast<HWND>(mainWindow_->winId());
    if (!hwnd) {
        return;
    }
    const BOOL useDark = state_.darkTheme ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
}

void QtLinkBackend::persistSettings() const {
    AppSettings s;
    s.darkTheme = state_.darkTheme;
    s.defaultLinkType = static_cast<int>(state_.linkType);
    s.autoCreateParent = state_.autoCreateParent;
    win::SaveSettings(s);
}

void QtLinkBackend::setDarkMode(bool dark) {
    if (state_.darkTheme == dark) {
        return;
    }
    state_.darkTheme = dark;
    persistSettings();
    applyWindowTitlebarTheme();
    emit darkModeChanged();
}

void QtLinkBackend::toggleTheme() {
    setDarkMode(!state_.darkTheme);
}

void QtLinkBackend::setActiveTab(int tab) {
    const int clamped = std::clamp(tab, 0, 3);
    if (state_.activeTab == clamped) {
        return;
    }
    state_.activeTab = clamped;
    emit activeTabChanged();
    refreshPrivileges();
}

void QtLinkBackend::setOperationMode(int mode) {
    const auto nextMode = static_cast<OperationMode>(std::clamp(mode, 0, 1));
    if (state_.operationMode == nextMode) {
        return;
    }
    state_.operationMode = nextMode;
    if (state_.operationMode == OperationMode::MoveAndLink &&
        (state_.linkType == LinkType::HardLink || state_.linkType == LinkType::FileSymlink)) {
        state_.linkType = LinkType::Junction;
        persistSettings();
    }
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::setLinkType(int type) {
    const auto nextType = static_cast<LinkType>(std::clamp(type, 0, 3));
    if (state_.linkType == nextType) {
        return;
    }
    state_.linkType = nextType;
    persistSettings();
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::setLinkPath(const QString& path) {
    const std::string utf8 = path.toStdString();
    if (state_.linkPath == utf8) {
        return;
    }
    state_.linkPath = utf8;
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::setTargetPath(const QString& path) {
    const std::string utf8 = path.toStdString();
    if (state_.targetPath == utf8) {
        return;
    }
    state_.targetPath = utf8;
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::setAutoCreateParent(bool enabled) {
    if (state_.autoCreateParent == enabled) {
        return;
    }
    state_.autoCreateParent = enabled;
    persistSettings();
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::setInspectQueryPath(const QString& path) {
    const std::string utf8 = path.toStdString();
    if (state_.inspectQueryPath == utf8) {
        return;
    }
    state_.inspectQueryPath = utf8;
    emit inspectStateChanged();
}

void QtLinkBackend::setInspectIncludeHardLinks(bool enabled) {
    if (state_.inspectIncludeHardLinks == enabled) {
        return;
    }
    state_.inspectIncludeHardLinks = enabled;
    emit inspectStateChanged();
}

void QtLinkBackend::setInspectMaxDepth(int depth) {
    if (state_.inspectMaxDepth == depth) {
        return;
    }
    state_.inspectMaxDepth = depth;
    emit inspectStateChanged();
}

void QtLinkBackend::setHistoryFilterText(const QString& text) {
    const std::string utf8 = text.toStdString();
    if (state_.historyFilterText == utf8) {
        return;
    }
    state_.historyFilterText = utf8;
    rebuildHistoryModel();
}

void QtLinkBackend::setHistoryCategoryFilter(int category) {
    if (state_.historyCategoryFilter == category) {
        return;
    }
    state_.historyCategoryFilter = category;
    rebuildHistoryModel();
}

void QtLinkBackend::refreshValidation() {
    cachedValidation_ = win::ValidateCreateInputs(state_);
    std::string preview = win::BuildMklinkCommand(state_.linkType, state_.linkPath, state_.targetPath);
    if (state_.operationMode == OperationMode::MoveAndLink) {
        const std::string src = state_.linkPath.empty() ? "<原目录>" : win::CleanPathInput(state_.linkPath);
        const std::string dst = state_.targetPath.empty() ? "<目标目录>" : win::CleanPathInput(state_.targetPath);
        preview = "1) robocopy \"" + src + "\" \"" + dst + "\" /E /COPYALL /MOVE  &&  2) " + preview;
    }
    commandPreview_ = QString::fromStdString(preview);
    emit validationChanged();
}

void QtLinkBackend::swapPaths() {
    std::swap(state_.linkPath, state_.targetPath);
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::fillExample() {
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
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::clearInputs() {
    state_.linkPath.clear();
    state_.targetPath.clear();
    state_.consoleOutput.clear();
    lastCreatedLink_.clear();
    emit createStateChanged();
    emit consoleOutputChanged();
    refreshValidation();
}

void QtLinkBackend::browseLinkSave() {
    const bool folderStyle = (state_.linkType == LinkType::Directory ||
                              state_.linkType == LinkType::Junction);
    if (win::ChooseNewLinkPath(state_.linkPath, folderStyle)) {
        emit createStateChanged();
        refreshValidation();
    }
}

void QtLinkBackend::browseLinkFolder() {
    const std::string picked = win::BrowseDirectory("选择文件夹路径", state_.linkPath);
    if (!picked.empty()) {
        state_.linkPath = picked;
        emit createStateChanged();
        refreshValidation();
    }
}

void QtLinkBackend::browseLinkFile() {
    const std::string picked = win::BrowseFile("选择文件路径", state_.linkPath);
    if (!picked.empty()) {
        state_.linkPath = picked;
        emit createStateChanged();
        refreshValidation();
    }
}

void QtLinkBackend::browseTargetFolder() {
    const std::string picked = win::BrowseDirectory("选择真实目标文件夹", state_.targetPath);
    if (!picked.empty()) {
        state_.targetPath = picked;
        emit createStateChanged();
        refreshValidation();
    }
}

void QtLinkBackend::browseTargetFile() {
    const std::string picked = win::BrowseFile("选择真实目标文件", state_.targetPath);
    if (!picked.empty()) {
        state_.targetPath = picked;
        emit createStateChanged();
        refreshValidation();
    }
}

void QtLinkBackend::applyQuickFix() {
    switch (cachedValidation_.quickFix) {
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
    emit createStateChanged();
    refreshValidation();
}

void QtLinkBackend::executeCreateAction() {
    refreshValidation();
    if (!cachedValidation_.canExecute) {
        emit toastRequested(QStringLiteral("参数待完善"),
                            QString::fromStdString(cachedValidation_.message), 2);
        return;
    }
    if (state_.operationMode == OperationMode::MoveAndLink) {
        const std::string src = win::CleanPathInput(state_.linkPath);
        const std::string dst = win::CleanPathInput(state_.targetPath);
        state_.confirmAction = ConfirmAction::MoveAndLink;
        const std::string msg =
            "即将把原文件夹完整迁移到目标盘：\n" + src +
            "\n\n迁移至：\n" + dst +
            "\n\n完成后会在原位置创建 " + linkTypeDisplayName(state_.linkType) +
            " 保持兼容。请确认无其他程序正在占用该目录。";
        emit confirmRequested(QStringLiteral("确认执行安全搬家并建链"),
                              QString::fromStdString(msg),
                              QStringLiteral("确认迁移并建链"));
        return;
    }
    launchCreateWorker();
}

void QtLinkBackend::launchCreateWorker() {
    if (createBusy_) {
        return;
    }
    createBusy_ = true;
    emit createBusyChanged();

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
    emit consoleOutputChanged();

    auto alive = alive_;
    std::thread([this, alive, mode, type, linkPath, targetPath, autoParent]() {
        CommandRunResult result = (mode == OperationMode::MoveAndLink)
            ? win::ExecuteMoveAndLinkSync(type, linkPath, targetPath, autoParent)
            : win::ExecuteMklinkSync(type, linkPath, targetPath, autoParent);

        QMetaObject::invokeMethod(this, [this, alive, mode, res = std::move(result)]() mutable {
            if (!alive->load(std::memory_order_relaxed)) {
                return;
            }
            createBusy_ = false;
            emit createBusyChanged();

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
                    emit createStateChanged();
                }
                emit toastRequested(QString::fromStdString(res.summary),
                                    QString::fromStdString(res.resolvedLinkPath), 1);
            } else {
                lastCreatedLink_.clear();
                emit toastRequested(QString::fromStdString(res.summary),
                                    QStringLiteral("请查看下方执行详情排查权限或路径原因。"), 3);
            }

            emit consoleOutputChanged();
            refreshPrivileges();
            refreshValidation();
            rebuildHistoryModel();
        }, Qt::QueuedConnection);
    }).detach();
}

void QtLinkBackend::copyCommand() {
    if (win::CopyTextToClipboard(commandPreview_.toStdString())) {
        emit toastRequested(QStringLiteral("已复制命令"),
                            QStringLiteral("完整命令已复制到剪贴板。"), 1);
    }
}

void QtLinkBackend::revealCreatedLink() {
    if (!lastCreatedLink_.empty()) {
        win::RevealInExplorer(lastCreatedLink_);
    }
}

void QtLinkBackend::inspectCreatedLink() {
    if (!lastCreatedLink_.empty()) {
        setActiveTab(1);
        inspectSpecificPath(QString::fromStdString(lastCreatedLink_));
    }
}

void QtLinkBackend::browseInspectFolder() {
    const std::string picked = win::BrowseDirectory("选择需要体检或扫描的文件夹", state_.inspectQueryPath);
    if (!picked.empty()) {
        state_.inspectQueryPath = picked;
        emit inspectStateChanged();
    }
}

void QtLinkBackend::browseInspectFile() {
    const std::string picked = win::BrowseFile("选择需要剖析的文件", state_.inspectQueryPath);
    if (!picked.empty()) {
        inspectSpecificPath(QString::fromStdString(picked));
    }
}

void QtLinkBackend::inspectSinglePathNow() {
    inspectSpecificPath(QString::fromStdString(state_.inspectQueryPath));
}

void QtLinkBackend::inspectSpecificPath(const QString& path) {
    const std::string clean = win::CleanPathInput(path.toStdString());
    if (clean.empty()) {
        emit toastRequested(QStringLiteral("提示"),
                            QStringLiteral("请先输入或选择需要体检的路径。"), 2);
        return;
    }
    state_.inspectQueryPath = clean;
    state_.inspectedSingle = win::InspectSinglePath(clean);
    state_.hasInspectedSingle = true;
    state_.inspectStatusText = "单路径剖析完成：" + state_.inspectedSingle.kindLabel;
    rebuildInspectModels();
    emit inspectStateChanged();
}

void QtLinkBackend::startDirectoryScan() {
    const std::string clean = win::CleanPathInput(state_.inspectQueryPath);
    if (clean.empty()) {
        emit toastRequested(QStringLiteral("提示"),
                            QStringLiteral("请先选择一个存在的文件夹再开始扫描。"), 2);
        return;
    }
    DWORD attrs = INVALID_FILE_ATTRIBUTES;
    if (!win::PathExistsNoFollow(win::Utf8ToWide(clean), &attrs) ||
        (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        emit toastRequested(QStringLiteral("路径无效"),
                            QStringLiteral("目录扫描仅支持已存在的文件夹路径。"), 3);
        return;
    }

    if (scanCancel_) {
        scanCancel_->store(true, std::memory_order_relaxed);
    }
    scanCancel_ = std::make_shared<std::atomic_bool>(false);
    auto cancelToken = scanCancel_;

    inspectScanning_ = true;
    state_.inspectStatusText = "正在后台深度扫描目录中的链接...";
    emit inspectScanningChanged();
    emit inspectStateChanged();

    const bool includeHard = state_.inspectIncludeHardLinks;
    const int maxDepth = state_.inspectMaxDepth;
    auto alive = alive_;

    std::thread([this, alive, clean, includeHard, maxDepth, cancelToken]() {
        auto found = win::ScanDirectoryForLinks(clean, includeHard, maxDepth, cancelToken);
        const bool canceled = cancelToken->load(std::memory_order_relaxed);

        QMetaObject::invokeMethod(this, [this, alive, canceled, results = std::move(found)]() mutable {
            if (!alive->load(std::memory_order_relaxed)) {
                return;
            }
            inspectScanning_ = false;
            emit inspectScanningChanged();

            if (canceled) {
                state_.inspectStatusText = "已取消扫描，保留此前结果。";
            } else {
                state_.scannedLinks = std::move(results);
                state_.inspectStatusText = state_.scannedLinks.empty()
                    ? "扫描完成：当前深度范围内未发现符号链接、目录联接或硬链接。"
                    : "扫描完成：共发现 " + std::to_string(state_.scannedLinks.size()) + " 项链接记录。";
                rebuildInspectModels();
            }
            emit inspectStateChanged();
        }, Qt::QueuedConnection);
    }).detach();
}

void QtLinkBackend::cancelDirectoryScan() {
    if (scanCancel_) {
        scanCancel_->store(true, std::memory_order_relaxed);
    }
}

void QtLinkBackend::requestUnlink(const QString& path) {
    const InspectedLink info = win::InspectSinglePath(path.toStdString());
    if (!info.exists || info.kindCode == "NORMAL" || info.kindCode == "MISSING" || info.kindCode == "REPARSE") {
        emit toastRequested(QStringLiteral("已安全拦截"),
                            QStringLiteral("只允许解除明确识别为符号链接、目录联接或硬链接的入口。"), 2);
        return;
    }
    state_.pendingUnlinkPath = info.path;
    state_.confirmAction = ConfirmAction::UnlinkItem;
    const std::string msg =
        "即将仅移除链接入口 [" + info.kindLabel + "]：\n" + info.path +
        "\n\n实际指向的数据目标 (" +
        (info.targetPath.empty() ? "同分区硬链接兄妹文件" : info.targetPath) +
        ") 不会被修改或删除。";
    emit confirmRequested(QStringLiteral("确认安全解除链接"),
                          QString::fromStdString(msg),
                          QStringLiteral("确认解除链接"));
}

void QtLinkBackend::rebuildInspectModels() {
    if (state_.hasInspectedSingle) {
        singleInspectMap_ = inspectLinkToMap(state_.inspectedSingle);
    } else {
        singleInspectMap_.clear();
    }
    scanResultList_.clear();
    scanResultList_.reserve(static_cast<int>(state_.scannedLinks.size()));
    for (const auto& item : state_.scannedLinks) {
        scanResultList_.push_back(inspectLinkToMap(item));
    }
    emit inspectResultsChanged();
}

void QtLinkBackend::requestClearHistory() {
    state_.confirmAction = ConfirmAction::ClearHistory;
    emit confirmRequested(QStringLiteral("清空所有执行记录"),
                          QStringLiteral("此操作将清除本地历史记录文件中的所有条目（不会影响已经创建的实际文件系统链接）。"),
                          QStringLiteral("确认清空"));
}

void QtLinkBackend::backfillHistory(int sourceIndex) {
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(state_.history.size())) {
        return;
    }
    const auto& item = state_.history[static_cast<std::size_t>(sourceIndex)];
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

    setActiveTab(0);
    emit createStateChanged();
    refreshValidation();
    emit toastRequested(QStringLiteral("已回填参数"),
                        QStringLiteral("已切换回新建页并填入历史路径与链接类型。"), 0);
}

void QtLinkBackend::inspectHistoryPath(const QString& path) {
    setActiveTab(1);
    inspectSpecificPath(path);
}

void QtLinkBackend::rebuildHistoryModel() {
    historyRowList_.clear();
    for (int i = static_cast<int>(state_.history.size()) - 1; i >= 0; --i) {
        const auto& h = state_.history[static_cast<std::size_t>(i)];
        bool catMatch = true;
        switch (state_.historyCategoryFilter) {
            case 1: catMatch = win::ContainsCaseInsensitive(h.type, "/D"); break;
            case 2: catMatch = win::ContainsCaseInsensitive(h.type, "/J"); break;
            case 3: catMatch = win::ContainsCaseInsensitive(h.type, "/H"); break;
            case 4: catMatch = win::ContainsCaseInsensitive(h.type, "FILE"); break;
            case 5: catMatch = h.success; break;
            case 6: catMatch = !h.success; break;
            default: break;
        }
        if (!catMatch) {
            continue;
        }
        if (!state_.historyFilterText.empty()) {
            const bool matched =
                win::ContainsCaseInsensitive(h.timestamp, state_.historyFilterText) ||
                win::ContainsCaseInsensitive(h.type, state_.historyFilterText) ||
                win::ContainsCaseInsensitive(h.linkPath, state_.historyFilterText) ||
                win::ContainsCaseInsensitive(h.targetPath, state_.historyFilterText) ||
                win::ContainsCaseInsensitive(h.result, state_.historyFilterText);
            if (!matched) {
                continue;
            }
        }
        QVariantMap row;
        row.insert(QStringLiteral("sourceIndex"), i);
        row.insert(QStringLiteral("timestamp"), QString::fromStdString(h.timestamp));
        row.insert(QStringLiteral("typeCode"), QString::fromStdString(h.type));
        row.insert(QStringLiteral("linkPath"), QString::fromStdString(h.linkPath));
        row.insert(QStringLiteral("targetPath"), QString::fromStdString(h.targetPath));
        row.insert(QStringLiteral("result"), QString::fromStdString(h.result));
        row.insert(QStringLiteral("success"), h.success);
        historyRowList_.push_back(std::move(row));
    }
    emit historyStateChanged();
}

void QtLinkBackend::relaunchAdmin() {
    if (win::RelaunchAsAdmin()) {
        QCoreApplication::quit();
    } else {
        emit toastRequested(QStringLiteral("提权未完成"),
                            QStringLiteral("UAC 提权请求被取消或未获授权。"), 2);
    }
}

void QtLinkBackend::openDeveloperSettings() {
    win::OpenWindowsDeveloperSettings();
}

void QtLinkBackend::refreshPrivileges() {
    state_.isAdmin = win::IsRunningAsAdmin();
    state_.isDevMode = win::IsDeveloperModeEnabled();
    emit privilegesChanged();
    refreshValidation();
}

void QtLinkBackend::revealPath(const QString& path) {
    if (!win::RevealInExplorer(path.toStdString())) {
        emit toastRequested(QStringLiteral("定位失败"),
                            QStringLiteral("指定路径及其上级目录目前均不存在。"), 2);
    }
}

void QtLinkBackend::copyText(const QString& text) {
    if (win::CopyTextToClipboard(text.toStdString())) {
        emit toastRequested(QStringLiteral("已复制"),
                            QStringLiteral("内容已复制到剪贴板。"), 1);
    }
}

void QtLinkBackend::confirmAccepted() {
    const ConfirmAction action = state_.confirmAction;
    state_.confirmAction = ConfirmAction::None;

    if (action == ConfirmAction::MoveAndLink) {
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
            rebuildInspectModels();
            emit inspectStateChanged();
            emit toastRequested(QStringLiteral("已安全解除链接"), QString::fromStdString(msg), 1);
        } else {
            emit toastRequested(QStringLiteral("解除链接失败"), QString::fromStdString(msg), 3);
        }
    } else if (action == ConfirmAction::ClearHistory) {
        state_.history.clear();
        win::WriteAllHistory(win::GetHistoryFilePath(), state_.history);
        rebuildHistoryModel();
        emit toastRequested(QStringLiteral("已清空记录"),
                            QStringLiteral("本地操作历史已重置。"), 0);
    }
}

void QtLinkBackend::confirmCanceled() {
    state_.confirmAction = ConfirmAction::None;
    state_.pendingUnlinkPath.clear();
}

} // namespace fl
