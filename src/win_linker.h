#pragma once

#include "linker_types.h"
#include "core/platform/async.h"
#include "eui/platform.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winioctl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fl::win {

namespace fs = std::filesystem;

inline std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (sizeNeeded <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(sizeNeeded), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &wide[0], sizeNeeded);
    return wide;
}

inline std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                               nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(sizeNeeded), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                        &utf8[0], sizeNeeded, nullptr, nullptr);
    return utf8;
}

inline std::string ToLowerAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

inline bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    const std::string h = ToLowerAscii(haystack);
    const std::string n = ToLowerAscii(needle);
    return h.find(n) != std::string::npos;
}

inline std::string CleanPathInput(std::string input) {
    const auto isSpace = [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };
    while (!input.empty() && isSpace(static_cast<unsigned char>(input.front()))) {
        input.erase(input.begin());
    }
    while (!input.empty() && isSpace(static_cast<unsigned char>(input.back()))) {
        input.pop_back();
    }
    if (input.size() >= 2 &&
        ((input.front() == '"' && input.back() == '"') ||
         (input.front() == '\'' && input.back() == '\''))) {
        input = input.substr(1, input.size() - 2);
    }
    while (!input.empty() && isSpace(static_cast<unsigned char>(input.front()))) {
        input.erase(input.begin());
    }
    while (!input.empty() && isSpace(static_cast<unsigned char>(input.back()))) {
        input.pop_back();
    }
    // Trim trailing slash unless root drive like "C:\"
    std::wstring w = Utf8ToWide(input);
    while (w.size() > 3 && (w.back() == L'\\' || w.back() == L'/')) {
        w.pop_back();
    }
    if (w.empty()) {
        return {};
    }
    std::replace(w.begin(), w.end(), L'/', L'\\');
    return WideToUtf8(w);
}

inline std::wstring CleanPathInputW(const std::string& utf8) {
    return Utf8ToWide(CleanPathInput(utf8));
}

inline fs::path ToFsPath(const std::string& utf8) {
    return fs::path(CleanPathInputW(utf8));
}

inline bool IsPathDirectory(const fs::path& p, std::error_code& ec) {
    const DWORD attrs = GetFileAttributesW(p.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        return false;
    }
    ec.clear();
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

inline bool PathExistsNoFollow(const std::wstring& wpath, DWORD* outAttrs = nullptr) {
    const DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (outAttrs) {
        *outAttrs = attrs;
    }
    return attrs != INVALID_FILE_ATTRIBUTES;
}

inline bool IsDirectoryTargetRequired(LinkType type) {
    return type == LinkType::Directory || type == LinkType::Junction;
}

inline std::wstring GetVolumeRootForPath(const std::wstring& wpath) {
    if (wpath.empty()) {
        return {};
    }
    wchar_t volumeRoot[MAX_PATH] = {};
    if (GetVolumePathNameW(wpath.c_str(), volumeRoot, MAX_PATH)) {
        return volumeRoot;
    }
    if (wpath.size() >= 2 && wpath[1] == L':') {
        return std::wstring(1, static_cast<wchar_t>(towupper(wpath[0]))) + L":\\";
    }
    return {};
}

inline bool ArePathsOnSameVolume(const std::wstring& a, const std::wstring& b) {
    std::wstring rootA = GetVolumeRootForPath(a);
    std::wstring rootB = GetVolumeRootForPath(b);
    if (rootA.empty() || rootB.empty()) {
        return true;
    }
    return _wcsicmp(rootA.c_str(), rootB.c_str()) == 0;
}

inline std::string GetVolumeFileSystem(const std::wstring& wpath) {
    std::wstring root = GetVolumeRootForPath(wpath);
    if (root.empty()) {
        return {};
    }
    wchar_t fsName[64] = {};
    if (GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fsName, 64)) {
        return WideToUtf8(fsName);
    }
    return {};
}

inline bool IsPathInsideDirectory(const fs::path& candidateChild, const fs::path& parentDir) {
    std::error_code ec;
    fs::path absChild = fs::weakly_canonical(candidateChild, ec);
    if (ec) absChild = fs::absolute(candidateChild, ec);
    fs::path absParent = fs::weakly_canonical(parentDir, ec);
    if (ec) absParent = fs::absolute(parentDir, ec);
    std::wstring c = absChild.wstring();
    std::wstring p = absParent.wstring();
    if (c.size() <= p.size()) {
        return false;
    }
    for (auto& ch : c) ch = static_cast<wchar_t>(towlower(ch));
    for (auto& ch : p) ch = static_cast<wchar_t>(towlower(ch));
    if (c.compare(0, p.size(), p) != 0) {
        return false;
    }
    return c[p.size()] == L'\\' || p.back() == L'\\';
}

inline bool IsRunningAsAdmin() {
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isMember);
        FreeSid(adminGroup);
    }
    return isMember != FALSE;
}

inline bool IsDeveloperModeEnabled() {
    DWORD value = 0;
    DWORD dataSize = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock",
        L"AllowDevelopmentWithoutDevLicense",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &dataSize);
    return status == ERROR_SUCCESS && value == 1;
}

inline bool RelaunchAsAdmin() {
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        return false;
    }
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

inline void OpenWindowsDeveloperSettings() {
    eui::platform::openUrl("ms-settings:developers");
}

inline std::string QuoteForCmd(const std::string& path) {
    return "\"" + CleanPathInput(path) + "\"";
}

inline std::string BuildMklinkCommand(LinkType type, const std::string& linkPath, const std::string& targetPath) {
    const std::string flag = linkTypeFlag(type);
    const std::string linkQ = linkPath.empty() ? "\"<链接路径>\"" : QuoteForCmd(linkPath);
    const std::string targetQ = targetPath.empty() ? "\"<目标路径>\"" : QuoteForCmd(targetPath);
    if (flag.empty()) {
        return "cmd /c mklink " + linkQ + " " + targetQ;
    }
    return "cmd /c mklink " + flag + " " + linkQ + " " + targetQ;
}

inline ValidationResult ValidateCreateInputs(const AppState& state) {
    ValidationResult res;
    const std::string link = CleanPathInput(state.linkPath);
    const std::string target = CleanPathInput(state.targetPath);

    if (link.empty() && target.empty()) {
        res.level = ValidationLevel::Info;
        res.message = state.operationMode == OperationMode::CreateOnly
            ? "请填写要新建的“链接位置”以及数据所在的“目标路径”。"
            : "搬家并建链模式：填写“现有源目录（将被迁移后原位建链）”与“新迁移目标位置”。";
        return res;
    }

    if (link.empty()) {
        res.level = ValidationLevel::Warning;
        res.message = state.operationMode == OperationMode::CreateOnly
            ? "请指定要创建的符号链接 / 联接路径。"
            : "请指定当前需要搬家的现有源文件夹路径。";
        return res;
    }

    if (target.empty()) {
        res.level = ValidationLevel::Warning;
        res.message = state.operationMode == OperationMode::CreateOnly
            ? "请指定指向的真实目标文件或文件夹路径。"
            : "请指定迁移目的地路径（需为一个尚不存在的新目录路径）。";
        return res;
    }

    const std::wstring wLink = Utf8ToWide(link);
    const std::wstring wTarget = Utf8ToWide(target);
    if (_wcsicmp(wLink.c_str(), wTarget.c_str()) == 0) {
        res.level = ValidationLevel::Error;
        res.message = "链接路径与目标路径完全相同，无法指向自身。";
        return res;
    }

    DWORD linkAttrs = INVALID_FILE_ATTRIBUTES;
    DWORD targetAttrs = INVALID_FILE_ATTRIBUTES;
    const bool linkExists = PathExistsNoFollow(wLink, &linkAttrs);
    const bool targetExists = PathExistsNoFollow(wTarget, &targetAttrs);

    if (state.operationMode == OperationMode::MoveAndLink) {
        if (!IsDirectoryTargetRequired(state.linkType)) {
            res.level = ValidationLevel::Error;
            res.message = "“目录搬家并建链”仅适用于目录联接 (/J) 或目录符号链接 (/D)。";
            res.quickFix = FixAction::SwitchToJunction;
            res.quickFixLabel = "切换为 /J 目录联接";
            return res;
        }
        if (!linkExists) {
            res.level = ValidationLevel::Error;
            res.message = "搬家模式要求“原位置 (Link)”是磁盘上已存在的真实文件夹。";
            return res;
        }
        if ((linkAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            res.level = ValidationLevel::Error;
            res.message = "原位置当前是一个文件而非文件夹，搬家模式仅支持整个文件夹迁移。";
            return res;
        }
        if ((linkAttrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            res.level = ValidationLevel::Error;
            res.message = "原位置已经是一个链接或重解析点，请勿重复迁移已有链接。";
            return res;
        }
        if (targetExists) {
            if ((targetAttrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                res.level = ValidationLevel::Warning;
                res.message = "目标目录已存在，迁移目标应为尚未创建的新路径。";
                res.quickFix = FixAction::AppendLinkFolderNameToTarget;
                res.quickFixLabel = "在目标下追加原文件夹名";
            } else {
                res.level = ValidationLevel::Error;
                res.message = "迁移目标位置已存在同名文件，无法迁入该路径。";
            }
            return res;
        }
        if (IsPathInsideDirectory(fs::path(wTarget), fs::path(wLink))) {
            res.level = ValidationLevel::Error;
            res.message = "迁移目标不能位于源文件夹内部，否则会导致递归死循环。";
            return res;
        }

        const fs::path targetParent = fs::path(wTarget).parent_path();
        DWORD parentAttrs = INVALID_FILE_ATTRIBUTES;
        if (!targetParent.empty() && !PathExistsNoFollow(targetParent.wstring(), &parentAttrs)) {
            if (!state.autoCreateParent) {
                res.level = ValidationLevel::Warning;
                res.message = "目标上层目录不存在，建议开启“自动创建父目录”。";
                res.quickFix = FixAction::EnableAutoCreateParent;
                res.quickFixLabel = "开启自动创建父目录";
                return res;
            }
        }

        res.level = ValidationLevel::Valid;
        res.canExecute = true;
        res.requiresConfirm = true;
        if (state.linkType == LinkType::Directory && !state.isAdmin && !state.isDevMode) {
            res.level = ValidationLevel::Warning;
            res.message = "目录校验通过，但 /D 符号链接通常需要管理员或开发者模式，可改用免提权的 /J 联接。";
            res.quickFix = FixAction::SwitchToJunction;
            res.quickFixLabel = "改用 /J 免提权";
        } else {
            res.message = "校验通过：将把源文件夹完整迁移至目标位置，并在原位置自动建立重定向链接。";
        }
        return res;
    }

    // Standard CreateOnly validation
    if (!targetExists) {
        res.level = ValidationLevel::Error;
        res.message = "目标路径在磁盘上不存在，请先确认真实文件或文件夹位置。";
        return res;
    }

    const bool targetIsDir = (targetAttrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const bool modeWantsDir = IsDirectoryTargetRequired(state.linkType);
    if (targetIsDir && !modeWantsDir) {
        res.level = ValidationLevel::Error;
        res.message = "目标是一个文件夹，但当前选择了文件链接模式。";
        res.quickFix = FixAction::SwitchToJunction;
        res.quickFixLabel = "一键切换为 /J 目录联接";
        return res;
    }
    if (!targetIsDir && modeWantsDir) {
        const bool sameVol = ArePathsOnSameVolume(wLink, wTarget);
        res.level = ValidationLevel::Error;
        res.message = "目标是一个文件，但当前选择了目录链接模式。";
        if (sameVol) {
            res.quickFix = FixAction::SwitchToHardLink;
            res.quickFixLabel = "一键切换为 /H 硬链接";
        } else {
            res.quickFix = FixAction::SwitchToFileSymlink;
            res.quickFixLabel = "一键切换为文件软链";
        }
        return res;
    }

    if (linkExists) {
        if ((linkAttrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            res.level = ValidationLevel::Warning;
            res.message = "链接路径指向一个已存在的文件夹，mklink 要求占位路径尚未存在（可将目标名称追加到其下）。";
            res.quickFix = FixAction::AppendTargetNameToLink;
            res.quickFixLabel = "在路径末尾追加目标名称";
        } else {
            res.level = ValidationLevel::Error;
            res.message = "链接路径已存在同名文件或链接，请更换名称或先在“链接管理”中安全移除。";
        }
        return res;
    }

    const fs::path linkParent = fs::path(wLink).parent_path();
    if (!linkParent.empty() && !PathExistsNoFollow(linkParent.wstring())) {
        if (!state.autoCreateParent) {
            res.level = ValidationLevel::Warning;
            res.message = "链接路径的上层文件夹不存在，请先创建或开启自动创建父目录。";
            res.quickFix = FixAction::EnableAutoCreateParent;
            res.quickFixLabel = "开启自动创建父目录";
            return res;
        }
    }

    if (state.linkType == LinkType::HardLink && !ArePathsOnSameVolume(wLink, wTarget)) {
        res.level = ValidationLevel::Error;
        res.message = "硬链接 (/H) 仅支持在同一磁盘分区内建立，当前链接与目标位于不同盘符。";
        res.quickFix = FixAction::SwitchToFileSymlink;
        res.quickFixLabel = "改用文件符号链接";
        return res;
    }

    if (state.linkType == LinkType::Junction || state.linkType == LinkType::HardLink) {
        const std::string linkFs = GetVolumeFileSystem(wLink);
        if (!linkFs.empty() && _stricmp(linkFs.c_str(), "NTFS") != 0 && _stricmp(linkFs.c_str(), "ReFS") != 0) {
            res.level = ValidationLevel::Error;
            res.message = "目标所在文件系统为 " + linkFs + "，硬链接与目录联接通常需要 NTFS 分区支持。";
            return res;
        }
    }

    res.canExecute = true;
    if ((state.linkType == LinkType::Directory || state.linkType == LinkType::FileSymlink) &&
        !state.isAdmin && !state.isDevMode) {
        res.level = ValidationLevel::Warning;
        res.message = state.linkType == LinkType::Directory
            ? "未检测到管理员权限或开发者模式，创建 /D 符号链接可能会被拒绝（建议以管理员启动或改用 /J）。"
            : "未检测到管理员权限或开发者模式，创建文件符号链接可能需要提升权限。";
        if (state.linkType == LinkType::Directory) {
            res.quickFix = FixAction::SwitchToJunction;
            res.quickFixLabel = "改用 /J 免提权";
        }
        return res;
    }

    res.level = ValidationLevel::Valid;
    res.message = "路径、卷宗类型与权限预检全部通过，可以立即创建链接。";
    return res;
}

inline std::string FormatTimestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
    localtime_s(&tmBuf, &nowTime);
    char buf[32] = {};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);
    return buf;
}

inline std::string TrimTrailingNewlines(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

inline std::string DecodeCmdOutput(const std::string& raw) {
    if (raw.empty()) {
        return {};
    }
    const UINT acp = GetACP();
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    if (wlen <= 0) {
        codePage = acp;
        wlen = MultiByteToWideChar(codePage, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
    }
    if (wlen <= 0) {
        return raw;
    }
    std::wstring wide(static_cast<std::size_t>(wlen), L'\0');
    MultiByteToWideChar(codePage, 0, raw.data(), static_cast<int>(raw.size()), &wide[0], wlen);
    return TrimTrailingNewlines(WideToUtf8(wide));
}

inline CommandRunResult ExecuteMklinkSync(LinkType type, const std::string& linkRaw, const std::string& targetRaw, bool autoCreateParent) {
    CommandRunResult result;
    result.resolvedLinkPath = CleanPathInput(linkRaw);
    result.resolvedTargetPath = CleanPathInput(targetRaw);
    result.linkTypeCode = linkTypeHistoryCode(type);
    result.commandDisplay = BuildMklinkCommand(type, result.resolvedLinkPath, result.resolvedTargetPath);

    if (autoCreateParent) {
        std::error_code ec;
        const fs::path parent = fs::path(Utf8ToWide(result.resolvedLinkPath)).parent_path();
        if (!parent.empty() && !fs::exists(parent, ec)) {
            if (!fs::create_directories(parent, ec)) {
                result.success = false;
                result.summary = "创建父目录失败";
                result.output = "无法自动创建链接所在上层目录: " + WideToUtf8(parent.wstring());
                return result;
            }
        }
    }

    const std::string flag = linkTypeFlag(type);
    std::wstring cmdLine = L"cmd.exe /c mklink ";
    if (!flag.empty()) {
        cmdLine += Utf8ToWide(flag) + L" ";
    }
    cmdLine += L"\"" + Utf8ToWide(result.resolvedLinkPath) + L"\" \"" + Utf8ToWide(result.resolvedTargetPath) + L"\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        result.success = false;
        result.summary = "管道创建失败";
        result.output = "CreatePipe failed.";
        return result;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    const BOOL created = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);

    if (!created) {
        CloseHandle(readPipe);
        result.success = false;
        result.summary = "进程启动失败";
        result.output = "CreateProcessW failed (err=" + std::to_string(GetLastError()) + ")";
        return result;
    }

    std::string rawOutput;
    char buffer[2048];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        rawOutput.append(buffer, buffer + bytesRead);
    }
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    result.exitCode = exitCode;
    result.output = DecodeCmdOutput(rawOutput);
    result.success = (exitCode == 0);
    result.summary = result.success ? "创建成功" : "执行失败 (Code " + std::to_string(exitCode) + ")";
    return result;
}

inline bool MoveDirectoryWithShell(const std::wstring& sourceDir, const std::wstring& destDir, std::string& outError) {
    std::error_code ec;
    const fs::path dstParent = fs::path(destDir).parent_path();
    const std::wstring dstName = fs::path(destDir).filename().wstring();
    if (dstName.empty()) {
        outError = "目标目录名称无效。";
        return false;
    }
    if (!dstParent.empty() && !fs::exists(dstParent, ec)) {
        if (!fs::create_directories(dstParent, ec)) {
            outError = "无法创建迁移目标的上层目录。";
            return false;
        }
    }

    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninit = SUCCEEDED(coHr);

    IFileOperation* fileOp = nullptr;
    IShellItem* srcItem = nullptr;
    IShellItem* dstFolderItem = nullptr;

    auto cleanup = [&]() {
        if (dstFolderItem) dstFolderItem->Release();
        if (srcItem) srcItem->Release();
        if (fileOp) fileOp->Release();
        if (shouldUninit) CoUninitialize();
    };

    HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOp));
    if (FAILED(hr)) {
        cleanup();
        outError = "无法初始化 Windows 文件迁移接口 (IFileOperation)。";
        return false;
    }

    fileOp->SetOperationFlags(FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOFX_NOCOPYHOOKS);

    hr = SHCreateItemFromParsingName(sourceDir.c_str(), nullptr, IID_PPV_ARGS(&srcItem));
    if (FAILED(hr)) {
        cleanup();
        outError = "无法打开源文件夹进行迁移。";
        return false;
    }

    const std::wstring parentStr = dstParent.empty() ? destDir : dstParent.wstring();
    hr = SHCreateItemFromParsingName(parentStr.c_str(), nullptr, IID_PPV_ARGS(&dstFolderItem));
    if (FAILED(hr)) {
        cleanup();
        outError = "无法访问迁移目标父目录。";
        return false;
    }

    hr = fileOp->MoveItem(srcItem, dstFolderItem, dstName.c_str(), nullptr);
    if (SUCCEEDED(hr)) {
        hr = fileOp->PerformOperations();
    }

    BOOL aborted = FALSE;
    fileOp->GetAnyOperationsAborted(&aborted);
    cleanup();

    if (aborted || FAILED(hr)) {
        outError = aborted ? "目录迁移已被用户取消或中止。" : "文件夹迁移失败（可能有文件正在被其他程序占用）。";
        return false;
    }

    if (PathExistsNoFollow(sourceDir)) {
        outError = "迁移后原路径仍残留文件或目录，出于安全考虑已中止建链。";
        return false;
    }
    return true;
}

inline CommandRunResult ExecuteMoveAndLinkSync(LinkType type, const std::string& originalDirRaw, const std::string& movedDestRaw, bool autoCreateParent) {
    CommandRunResult result;
    const std::string srcClean = CleanPathInput(originalDirRaw);
    const std::string dstClean = CleanPathInput(movedDestRaw);
    result.resolvedLinkPath = srcClean;
    result.resolvedTargetPath = dstClean;
    result.linkTypeCode = std::string(linkTypeHistoryCode(type)) + "+MOVE";
    result.commandDisplay = "[搬家] " + QuoteForCmd(srcClean) + " -> " + QuoteForCmd(dstClean)
        + "  &&  " + BuildMklinkCommand(type, srcClean, dstClean);

    std::string moveError;
    if (!MoveDirectoryWithShell(Utf8ToWide(srcClean), Utf8ToWide(dstClean), moveError)) {
        result.success = false;
        result.exitCode = 1;
        result.summary = "搬家中止";
        result.output = moveError;
        return result;
    }

    CommandRunResult linkRes = ExecuteMklinkSync(type, srcClean, dstClean, autoCreateParent);
    linkRes.commandDisplay = result.commandDisplay;
    linkRes.linkTypeCode = result.linkTypeCode;
    if (linkRes.success) {
        linkRes.summary = "搬家并建链完成";
        linkRes.output = "1) 源文件夹已安全迁移到:\n   " + dstClean + "\n2) " + linkRes.output;
    } else {
        linkRes.summary = "迁移成功但建链失败";
        linkRes.output = "源数据已完整迁移至:\n   " + dstClean + "\n但在原位建立链接时出错:\n   " + linkRes.output;
    }
    return linkRes;
}

namespace detail {

struct ReparseDataHeader {
    DWORD ReparseTag;
    WORD ReparseDataLength;
    WORD Reserved;
};

struct SymlinkReparseBody {
    WORD SubstituteNameOffset;
    WORD SubstituteNameLength;
    WORD PrintNameOffset;
    WORD PrintNameLength;
    ULONG Flags;
    WCHAR PathBuffer[1];
};

struct MountPointReparseBody {
    WORD SubstituteNameOffset;
    WORD SubstituteNameLength;
    WORD PrintNameOffset;
    WORD PrintNameLength;
    WCHAR PathBuffer[1];
};

inline std::wstring StripDevicePrefix(std::wstring s) {
    if (s.rfind(L"\\??\\", 0) == 0 || s.rfind(L"\\\\?\\", 0) == 0) {
        s = s.substr(4);
    }
    return s;
}

inline bool ReadReparseTarget(const std::wstring& path, bool& outIsSymlink, bool& outIsJunction,
                              bool& outRelative, std::wstring& outTarget) {
    outIsSymlink = false;
    outIsJunction = false;
    outRelative = false;
    outTarget.clear();

    HANDLE hFile = CreateFileW(path.c_str(),
                               0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr,
                               OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                               nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::vector<unsigned char> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE, 0);
    DWORD bytesReturned = 0;
    const BOOL ok = DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, nullptr, 0,
                                    buffer.data(), static_cast<DWORD>(buffer.size()),
                                    &bytesReturned, nullptr);
    CloseHandle(hFile);

    if (!ok || bytesReturned < sizeof(ReparseDataHeader)) {
        return false;
    }

    const auto* header = reinterpret_cast<const ReparseDataHeader*>(buffer.data());
    const unsigned char* bodyPtr = buffer.data() + sizeof(ReparseDataHeader);
    const std::size_t bodyBytes = bytesReturned - sizeof(ReparseDataHeader);

    auto extractSlice = [](const WCHAR* pathBuf, std::size_t maxWchars, WORD byteOffset, WORD byteLen) -> std::wstring {
        const std::size_t start = byteOffset / sizeof(WCHAR);
        const std::size_t len = byteLen / sizeof(WCHAR);
        if (len == 0 || start + len > maxWchars) {
            return {};
        }
        return std::wstring(pathBuf + start, len);
    };

    if (header->ReparseTag == IO_REPARSE_TAG_SYMLINK && bodyBytes >= sizeof(SymlinkReparseBody) - sizeof(WCHAR)) {
        outIsSymlink = true;
        const auto* sym = reinterpret_cast<const SymlinkReparseBody*>(bodyPtr);
        const std::size_t maxWchars = (bodyBytes - offsetof(SymlinkReparseBody, PathBuffer)) / sizeof(WCHAR);
        outRelative = (sym->Flags & 1u) != 0;
        std::wstring printName = extractSlice(sym->PathBuffer, maxWchars, sym->PrintNameOffset, sym->PrintNameLength);
        std::wstring subName = extractSlice(sym->PathBuffer, maxWchars, sym->SubstituteNameOffset, sym->SubstituteNameLength);
        outTarget = !printName.empty() ? printName : StripDevicePrefix(subName);
        return true;
    }

    if (header->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT && bodyBytes >= sizeof(MountPointReparseBody) - sizeof(WCHAR)) {
        outIsJunction = true;
        const auto* mnt = reinterpret_cast<const MountPointReparseBody*>(bodyPtr);
        const std::size_t maxWchars = (bodyBytes - offsetof(MountPointReparseBody, PathBuffer)) / sizeof(WCHAR);
        std::wstring printName = extractSlice(mnt->PathBuffer, maxWchars, mnt->PrintNameOffset, mnt->PrintNameLength);
        std::wstring subName = extractSlice(mnt->PathBuffer, maxWchars, mnt->SubstituteNameOffset, mnt->SubstituteNameLength);
        outTarget = !printName.empty() ? printName : StripDevicePrefix(subName);
        return true;
    }

    return false;
}

inline std::vector<std::string> EnumerateHardLinkSiblings(const std::wstring& filePath, unsigned long& outCount) {
    outCount = 0;
    std::vector<std::string> siblings;

    HANDLE hFile = CreateFileW(filePath.c_str(), 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info{};
        if (GetFileInformationByHandle(hFile, &info)) {
            outCount = info.nNumberOfLinks;
        }
        CloseHandle(hFile);
    }

    if (outCount <= 1) {
        return siblings;
    }

    std::wstring volumeRoot = GetVolumeRootForPath(filePath);
    if (!volumeRoot.empty() && volumeRoot.back() == L'\\') {
        volumeRoot.pop_back();
    }

    wchar_t nameBuf[MAX_PATH * 2] = {};
    DWORD nameLen = MAX_PATH * 2;
    HANDLE hFind = FindFirstFileNameW(filePath.c_str(), 0, &nameLen, nameBuf);
    if (hFind == INVALID_HANDLE_VALUE) {
        return siblings;
    }

    do {
        const std::wstring full = volumeRoot + std::wstring(nameBuf);
        if (_wcsicmp(full.c_str(), filePath.c_str()) != 0) {
            siblings.push_back(WideToUtf8(full));
        }
        nameLen = MAX_PATH * 2;
    } while (FindNextFileNameW(hFind, &nameLen, nameBuf) && siblings.size() < 24);

    FindClose(hFind);
    return siblings;
}

} // namespace detail

inline InspectedLink InspectSinglePath(const std::string& rawPath) {
    InspectedLink item;
    item.path = CleanPathInput(rawPath);
    if (item.path.empty()) {
        item.kindCode = "MISSING";
        item.kindLabel = "空路径";
        return item;
    }

    const std::wstring wpath = Utf8ToWide(item.path);
    const DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        item.kindCode = "MISSING";
        item.kindLabel = "路径不存在";
        item.detailNote = "指定路径在磁盘上不存在或无权访问。";
        return item;
    }

    item.exists = true;
    item.isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    item.isReparsePoint = (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

    if (item.isReparsePoint) {
        bool isSym = false;
        bool isJunc = false;
        bool isRel = false;
        std::wstring targetW;
        if (detail::ReadReparseTarget(wpath, isSym, isJunc, isRel, targetW)) {
            item.isRelative = isRel;
            item.targetPath = WideToUtf8(targetW);
            if (isSym) {
                item.kindCode = item.isDirectory ? "/D" : "FILE";
                item.kindLabel = item.isDirectory ? "目录符号链接 (/D)" : "文件符号链接";
            } else if (isJunc) {
                item.kindCode = "/J";
                item.kindLabel = "目录联接 (/J)";
            } else {
                item.kindCode = "REPARSE";
                item.kindLabel = "其他重解析点";
            }

            if (!targetW.empty()) {
                fs::path checkPath = fs::path(targetW);
                if (isRel) {
                    checkPath = fs::path(wpath).parent_path() / checkPath;
                }
                item.targetExists = PathExistsNoFollow(checkPath.wstring());
                item.detailNote = item.targetExists
                    ? (isRel ? "相对路径指向有效" : "目标存在且健康")
                    : "警告：目标路径已失联或被删除（悬空链接）";
            } else {
                item.detailNote = "属于系统重解析点（如 OneDrive/应用别名等），建议保留。";
            }
            return item;
        }

        item.kindCode = "REPARSE";
        item.kindLabel = "重解析点 (未识别标签)";
        item.detailNote = "包含重解析点属性，但非标准 Symlink / Junction。";
        return item;
    }

    if (!item.isDirectory) {
        item.hardLinkSiblings = detail::EnumerateHardLinkSiblings(wpath, item.hardLinkCount);
        if (item.hardLinkCount > 1) {
            item.kindCode = "/H";
            item.kindLabel = "文件硬链接 (/H)";
            item.targetExists = true;
            if (!item.hardLinkSiblings.empty()) {
                item.targetPath = item.hardLinkSiblings.front();
            }
            item.detailNote = "共享同一文件数据块，总计 " + std::to_string(item.hardLinkCount) + " 个硬链接入口。";
            return item;
        }
        item.kindCode = "NORMAL";
        item.kindLabel = "普通文件";
        item.detailNote = "常规磁盘文件（链接计数 = 1），并非重定向链接。";
        return item;
    }

    item.kindCode = "NORMAL";
    item.kindLabel = "普通文件夹";
    item.detailNote = "常规实体文件夹（不含重解析点属性）。";
    return item;
}

inline std::vector<InspectedLink> ScanDirectoryForLinks(
    const std::string& rootDirRaw,
    bool includeHardLinks,
    int maxDepth,
    const core::async::CancelToken& cancel = {}) {
    std::vector<InspectedLink> results;
    const std::string cleanRoot = CleanPathInput(rootDirRaw);
    if (cleanRoot.empty()) {
        return results;
    }

    struct QueueEntry {
        std::wstring path;
        int depth;
    };

    std::vector<QueueEntry> stack;
    stack.push_back({Utf8ToWide(cleanRoot), 0});

    constexpr std::size_t kMaxResults = 1500;
    while (!stack.empty() && results.size() < kMaxResults) {
        if (cancel.canceled()) {
            break;
        }
        QueueEntry cur = std::move(stack.back());
        stack.pop_back();

        WIN32_FIND_DATAW ffd{};
        const std::wstring pattern = cur.path + L"\\*";
        HANDLE hFind = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &ffd,
                                        FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (hFind == INVALID_HANDLE_VALUE) {
            continue;
        }

        do {
            if (cancel.canceled()) {
                break;
            }
            if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
                continue;
            }

            const std::wstring childW = cur.path + L"\\" + ffd.cFileName;
            const bool isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            const bool isReparse = (ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

            if (isReparse) {
                InspectedLink info = InspectSinglePath(WideToUtf8(childW));
                if (info.kindCode == "/D" || info.kindCode == "/J" || info.kindCode == "FILE") {
                    results.push_back(std::move(info));
                }
                // Do NOT recurse inside symlink/junction directories to avoid loops
                continue;
            }

            if (isDir) {
                if (cur.depth + 1 < maxDepth) {
                    stack.push_back({childW, cur.depth + 1});
                }
            } else if (includeHardLinks) {
                unsigned long linkCount = 0;
                HANDLE hFile = CreateFileW(childW.c_str(), 0,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    BY_HANDLE_FILE_INFORMATION fi{};
                    if (GetFileInformationByHandle(hFile, &fi)) {
                        linkCount = fi.nNumberOfLinks;
                    }
                    CloseHandle(hFile);
                }
                if (linkCount > 1) {
                    results.push_back(InspectSinglePath(WideToUtf8(childW)));
                }
            }
        } while (FindNextFileW(hFind, &ffd) && results.size() < kMaxResults);

        FindClose(hFind);
    }

    return results;
}

inline bool SafeUnlinkPath(const std::string& rawPath, std::string& outMessage) {
    const InspectedLink info = InspectSinglePath(rawPath);
    if (!info.exists) {
        outMessage = "路径不存在，无需解除。";
        return false;
    }

    const std::wstring wpath = Utf8ToWide(info.path);
    if (info.isReparsePoint && (info.kindCode == "/D" || info.kindCode == "/J" || info.kindCode == "FILE")) {
        if (info.isDirectory) {
            if (RemoveDirectoryW(wpath.c_str())) {
                outMessage = "已安全解除目录链接入口，真实目标文件夹内容完好无损。";
                return true;
            }
        } else {
            if (DeleteFileW(wpath.c_str())) {
                outMessage = "已安全解除文件符号链接入口，原始目标文件未受影响。";
                return true;
            }
        }
        outMessage = "解除链接失败，Win32 错误码: " + std::to_string(GetLastError());
        return false;
    }

    if (info.kindCode == "/H" && info.hardLinkCount > 1) {
        if (DeleteFileW(wpath.c_str())) {
            outMessage = "已移除该硬链接入口，其余兄弟硬链接路径及文件内容均保持完整。";
            return true;
        }
        outMessage = "移除硬链接失败，Win32 错误码: " + std::to_string(GetLastError());
        return false;
    }

    outMessage = "安全拦截：该路径属于普通文件或实体文件夹，已拒绝执行解除操作以防误删数据。";
    return false;
}

inline std::string ResolveExistingDirectoryHint(const std::string& rawPath) {
    const std::string clean = CleanPathInput(rawPath);
    if (clean.empty()) {
        return {};
    }
    DWORD attrs = INVALID_FILE_ATTRIBUTES;
    fs::path p = fs::path(Utf8ToWide(clean));
    if (PathExistsNoFollow(p.wstring(), &attrs) && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return clean;
    }
    const fs::path parent = p.parent_path();
    if (!parent.empty() && PathExistsNoFollow(parent.wstring(), &attrs) && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return WideToUtf8(parent.wstring());
    }
    return {};
}

inline std::string BrowseDirectory(const std::string& prompt, const std::string& initial = {}) {
    eui::platform::FileDialogOptions opts;
    opts.prompt = prompt;
    opts.initialDirectory = ResolveExistingDirectoryHint(initial);
    return eui::platform::chooseDirectory(opts);
}

inline std::string BrowseFile(const std::string& prompt, const std::string& initial = {}) {
    eui::platform::FileDialogOptions opts;
    opts.prompt = prompt;
    opts.initialDirectory = ResolveExistingDirectoryHint(initial);
    return eui::platform::chooseFile(opts);
}

inline bool ChooseNewLinkPath(std::string& outPath, bool preferFolderStyle = false) {
    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninit = SUCCEEDED(coHr);

    IFileSaveDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        if (shouldUninit) CoUninitialize();
        return false;
    }

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts &= ~FOS_OVERWRITEPROMPT;
    opts &= ~FOS_FILEMUSTEXIST;
    opts |= FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT;
    dlg->SetOptions(opts);
    dlg->SetTitle(preferFolderStyle ? L"指定新链接文件夹路径（填写名称即可，不会实际创建文件夹）"
                                    : L"指定新链接文件路径");

    bool ok = false;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR pszFilePath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath) {
                outPath = WideToUtf8(pszFilePath);
                CoTaskMemFree(pszFilePath);
                ok = true;
            }
            item->Release();
        }
    }

    dlg->Release();
    if (shouldUninit) CoUninitialize();
    return ok;
}

inline bool RevealInExplorer(const std::string& rawPath) {
    const std::string clean = CleanPathInput(rawPath);
    if (clean.empty()) {
        return false;
    }
    const std::wstring wpath = Utf8ToWide(clean);
    if (PathExistsNoFollow(wpath)) {
        const std::wstring args = L"/select,\"" + wpath + L"\"";
        const HINSTANCE res = ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<std::intptr_t>(res) > 32;
    }

    const fs::path parent = fs::path(wpath).parent_path();
    if (!parent.empty() && PathExistsNoFollow(parent.wstring())) {
        const HINSTANCE res = ShellExecuteW(nullptr, L"open", parent.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<std::intptr_t>(res) > 32;
    }
    return false;
}

inline bool OpenFolderInExplorer(const fs::path& dir) {
    if (dir.empty() || !PathExistsNoFollow(dir.wstring())) {
        return false;
    }
    const HINSTANCE res = ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(res) > 32;
}

inline bool CopyTextToClipboard(const std::string& utf8) {
    const std::wstring wide = Utf8ToWide(utf8);
    if (!OpenClipboard(nullptr)) {
        return false;
    }
    EmptyClipboard();
    const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    void* ptr = GlobalLock(hMem);
    if (ptr) {
        memcpy(ptr, wide.c_str(), bytes);
        GlobalUnlock(hMem);
    }
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
}

inline std::string ReadTextFromClipboard() {
    if (!OpenClipboard(nullptr)) {
        return {};
    }
    std::string result;
    if (HANDLE hData = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* wptr = static_cast<const wchar_t*>(GlobalLock(hData))) {
            result = WideToUtf8(wptr);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

inline fs::path GetAppDataDir() {
    PWSTR localAppData = nullptr;
    fs::path base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) && localAppData) {
        base = fs::path(localAppData) / L"FluentLinker";
        CoTaskMemFree(localAppData);
    } else {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        base = fs::path(exePath).parent_path();
    }
    std::error_code ec;
    fs::create_directories(base, ec);
    return base;
}

inline fs::path GetHistoryFilePath() {
    return GetAppDataDir() / L"mklink_history.tsv";
}

inline fs::path GetSettingsFilePath() {
    return GetAppDataDir() / L"settings.ini";
}

inline void ParseHistoryFile(const fs::path& filePath, std::vector<HistoryItem>& items) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> fields;
        std::size_t start = 0;
        while (true) {
            const std::size_t tab = line.find('\t', start);
            if (tab == std::string::npos) {
                fields.push_back(line.substr(start));
                break;
            }
            fields.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        if (fields.size() >= 5) {
            HistoryItem item;
            item.timestamp = fields[0];
            item.type = fields[1];
            item.linkPath = fields[2];
            item.targetPath = fields[3];
            item.result = fields[4];
            item.success = ContainsCaseInsensitive(item.result, "成功") ||
                           ContainsCaseInsensitive(item.result, "完成") ||
                           ContainsCaseInsensitive(item.result, "ok") ||
                           ContainsCaseInsensitive(item.result, "success");
            items.push_back(std::move(item));
        }
    }
}

inline void WriteAllHistory(const fs::path& filePath, const std::vector<HistoryItem>& items) {
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    for (const auto& item : items) {
        out << item.timestamp << '\t'
            << item.type << '\t'
            << item.linkPath << '\t'
            << item.targetPath << '\t'
            << item.result << '\n';
    }
}

inline std::vector<HistoryItem> LoadHistoryWithMigration() {
    const fs::path canonical = GetHistoryFilePath();
    std::vector<HistoryItem> items;
    std::error_code ec;

    if (!fs::exists(canonical, ec)) {
        // Migrate from legacy locations if present
        std::vector<fs::path> legacyPaths;
        PWSTR localAppData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) && localAppData) {
            legacyPaths.push_back(fs::path(localAppData) / L"MyModernUI_mklink_history.tsv");
            CoTaskMemFree(localAppData);
        }
        wchar_t exePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
            legacyPaths.push_back(fs::path(exePath).parent_path() / L"mklink_history.tsv");
        }
        for (const auto& legacy : legacyPaths) {
            if (fs::exists(legacy, ec)) {
                ParseHistoryFile(legacy, items);
                break;
            }
        }
        if (!items.empty()) {
            WriteAllHistory(canonical, items);
        }
        return items;
    }

    ParseHistoryFile(canonical, items);
    return items;
}

inline void AppendHistoryEntry(const HistoryItem& item) {
    std::ofstream out(GetHistoryFilePath(), std::ios::binary | std::ios::app);
    if (!out.is_open()) {
        return;
    }
    auto sanitize = [](std::string s) {
        std::replace(s.begin(), s.end(), '\t', ' ');
        std::replace(s.begin(), s.end(), '\r', ' ');
        std::replace(s.begin(), s.end(), '\n', ' ');
        return s;
    };
    out << sanitize(item.timestamp) << '\t'
        << sanitize(item.type) << '\t'
        << sanitize(item.linkPath) << '\t'
        << sanitize(item.targetPath) << '\t'
        << sanitize(item.result) << '\n';
}

inline AppSettings LoadSettings() {
    AppSettings settings;
    std::ifstream in(GetSettingsFilePath(), std::ios::binary);
    if (!in.is_open()) {
        return settings;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        if (key == "darkTheme") {
            settings.darkTheme = (val != "0" && val != "false");
        } else if (key == "defaultLinkType") {
            try {
                const int parsed = std::stoi(val);
                if (parsed >= 0 && parsed <= 3) {
                    settings.defaultLinkType = parsed;
                }
            } catch (...) {}
        } else if (key == "autoCreateParent") {
            settings.autoCreateParent = (val != "0" && val != "false");
        }
    }
    return settings;
}

inline void SaveSettings(const AppSettings& settings) {
    std::ofstream out(GetSettingsFilePath(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << "darkTheme=" << (settings.darkTheme ? "1" : "0") << '\n';
    out << "defaultLinkType=" << settings.defaultLinkType << '\n';
    out << "autoCreateParent=" << (settings.autoCreateParent ? "1" : "0") << '\n';
}

} // namespace fl::win
