#include "app/dsl_app.h"
#include "components/components.h"

#define NOMINMAX
#include <windows.h>
#include <shobjidl.h> // For native Windows IFileOpenDialog
#include <shlobj.h>   // For SHGetKnownFolderPath
#include <Knownfolders.h> // For FOLDERID_LocalAppData
#include <objbase.h>  // For COM CoInitializeEx
#include <shellapi.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

struct ComInit {
    ComInit() { ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); }
    ~ComInit() { ::CoUninitialize(); }
} g_comInit;

namespace app {

enum class LinkType { Directory, HardLink, Junction };

struct HistoryItem {
    std::string timestamp;
    std::string type;
    std::string linkPath;
    std::string targetPath;
    std::string result;
};

struct ValidationResult {
    bool isValid = true;
    std::string message = "等待输入...";
    core::Color color = {0.55f, 0.55f, 0.55f, 1.0f};
};

static std::wstring Utf8ToWide(const char* text) {
    if (text == nullptr || text[0] == '\0') return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring wide(needed - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), needed);
    return wide;
}

static std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return "";
    int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return "";
    std::string utf8(needed - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

static std::wstring GetHistoryPath() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    return (exePath.parent_path() / L"mklink_history.tsv").wstring();
}

static std::string CurrentTimestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt = system_clock::to_time_t(now);
    tm localTime{};
    localtime_s(&localTime, &tt);
    char buffer[64] = {};
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

static std::wstring QuoteArg(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back(L'"');
    for (wchar_t ch : value) {
        if (ch == L'"') escaped.push_back(L'\\');
        escaped.push_back(ch);
    }
    escaped.push_back(L'"');
    return escaped;
}

static const char* LinkTypeName(LinkType type) {
    switch (type) {
    case LinkType::Directory: return "目录符号链接 (/D)";
    case LinkType::HardLink: return "文件硬链接 (/H)";
    case LinkType::Junction: return "目录联接 (/J)";
    }
    return "目录符号链接 (/D)";
}

static std::wstring BuildMklinkCommand(LinkType type, const std::string& linkPath, const std::string& targetPath) {
    std::wstring command = L"mklink ";
    if (type == LinkType::Directory) command += L"/D ";
    else if (type == LinkType::HardLink) command += L"/H ";
    else command += L"/J ";
    command += QuoteArg(Utf8ToWide(linkPath.c_str()));
    command += L" ";
    command += QuoteArg(Utf8ToWide(targetPath.c_str()));
    return command;
}

static bool RunCommand(const std::wstring& command, std::string& output, DWORD& exitCode) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return false;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;

    PROCESS_INFORMATION pi{};
    std::wstring fullCommand = L"cmd.exe /c " + command;
    std::vector<wchar_t> mutableCommand(fullCommand.begin(), fullCommand.end());
    mutableCommand.push_back(L'\0');

    BOOL created = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);

    if (!created) {
        CloseHandle(readPipe);
        return false;
    }

    std::wstring wideOutput;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        int needed = MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, nullptr, 0);
        if (needed > 0) {
            std::wstring chunk(needed - 1, L'\0');
            MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, chunk.data(), needed);
            wideOutput += chunk;
        }
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);

    output = WideToUtf8(wideOutput);
    return true;
}

static std::string SanitizeField(std::string value) {
    for (char& ch : value) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    return value;
}

static void LoadHistory(std::vector<HistoryItem>& history) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, GetHistoryPath().c_str(), L"rb") != 0 || file == nullptr) return;

    char line[8192];
    while (fgets(line, sizeof(line), file)) {
        std::string text = line;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();

        std::vector<std::string> fields;
        size_t start = 0;
        while (true) {
            size_t pos = text.find('\t', start);
            fields.push_back(text.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
            if (pos == std::string::npos) break;
            start = pos + 1;
        }
        if (fields.size() >= 5) history.push_back({ fields[0], fields[1], fields[2], fields[3], fields[4] });
    }
    fclose(file);
}

static void AppendHistory(const HistoryItem& item) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, GetHistoryPath().c_str(), L"ab") != 0 || file == nullptr) return;

    std::string line = SanitizeField(item.timestamp) + "\t"
        + SanitizeField(item.type) + "\t"
        + SanitizeField(item.linkPath) + "\t"
        + SanitizeField(item.targetPath) + "\t"
        + SanitizeField(item.result) + "\n";
    fwrite(line.data(), 1, line.size(), file);
    fclose(file);
}

static std::wstring OpenFolderDialog() {
    std::wstring path;
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        if (SUCCEEDED(pFileOpen->Show(nullptr))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                    path = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    return path;
}

static std::wstring OpenFileDialog() {
    std::wstring path;
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
            pFileOpen->SetOptions(dwOptions | FOS_FORCEFILESYSTEM);
        }
        if (SUCCEEDED(pFileOpen->Show(nullptr))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                    path = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    return path;
}

static ValidationResult ValidateInputs(LinkType type, const std::string& linkPath, const std::string& targetPath) {
    ValidationResult res;
    if (linkPath.empty() && targetPath.empty()) {
        res.isValid = false;
        res.message = "等待输入...";
        res.color = {0.55f, 0.55f, 0.55f, 1.00f};
        return res;
    }
    if (linkPath.empty()) {
        res.isValid = false;
        res.message = "请输入链接存放路径";
        res.color = {0.95f, 0.65f, 0.20f, 1.00f};
        return res;
    }
    if (targetPath.empty()) {
        res.isValid = false;
        res.message = "请输入真实目标路径";
        res.color = {0.95f, 0.65f, 0.20f, 1.00f};
        return res;
    }

    try {
        std::wstring wLink = Utf8ToWide(linkPath.c_str());
        std::wstring wTarget = Utf8ToWide(targetPath.c_str());
        std::filesystem::path pLink(wLink);
        std::filesystem::path pTarget(wTarget);

        if (!std::filesystem::exists(pTarget)) {
            res.isValid = false;
            res.message = "目标路径不存在，执行 mklink 可能会失败。";
            res.color = {0.95f, 0.40f, 0.40f, 1.00f};
            return res;
        }

        bool targetIsDir = std::filesystem::is_directory(pTarget);
        if (type == LinkType::HardLink && targetIsDir) {
            res.isValid = false;
            res.message = "文件硬链接的目标必须是文件，不能是目录！";
            res.color = {0.95f, 0.40f, 0.40f, 1.00f};
            return res;
        }
        if ((type == LinkType::Directory || type == LinkType::Junction) && !targetIsDir) {
            res.isValid = false;
            res.message = "目录软链接或联接的目标必须是目录，不能是文件！";
            res.color = {0.95f, 0.40f, 0.40f, 1.00f};
            return res;
        }
        if (std::filesystem::exists(pLink)) {
            res.isValid = false;
            res.message = "链接路径已存在！Windows 不允许在已有位置创建链接。";
            res.color = {0.95f, 0.40f, 0.40f, 1.00f};
            return res;
        }
        if (pLink.has_parent_path() && !pLink.parent_path().empty() && !std::filesystem::exists(pLink.parent_path())) {
            res.isValid = false;
            res.message = "链接所在的父文件夹不存在！请先创建该层目录。";
            res.color = {0.95f, 0.40f, 0.40f, 1.00f};
            return res;
        }

        res.isValid = true;
        res.message = "路径配置完全有效，准备就绪。";
        res.color = {0.38f, 0.85f, 0.38f, 1.00f};
    } catch (const std::exception& e) {
        res.isValid = false;
        res.message = std::string("路径有效性解析出错: ") + e.what();
        res.color = {0.95f, 0.40f, 0.40f, 1.00f};
    }
    return res;
}

struct AppState {
    int currentTab = 0;
    int linkTypeIdx = 0; // 0: Dir, 1: Hard, 2: Junction
    std::string linkPath;
    std::string targetPath;
    std::vector<HistoryItem> history;
    bool historyLoaded = false;
    std::string searchFilter;
    float historyScroll = 0.0f;
    
    // Toast state
    bool showToast = false;
    std::string toastTitle;
    std::string toastMsg;
    bool toastSuccess = false;
};

AppState g_state;

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Fluent Linker Pro - EUI Edition")
        .pageId("fluent_linker")
        .clearColor({0.10f, 0.12f, 0.14f, 1.0f})
        .textFont("C:\\Windows\\Fonts\\msyh.ttc")
        .windowSize(1000, 700);
    return config;
}

void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen) {
    if (!g_state.historyLoaded) {
        LoadHistory(g_state.history);
        g_state.historyLoaded = true;
    }

    // Safety guards against negative sizes during init frames
    float safeWidth = std::max(600.0f, screen.width);
    float safeHeight = std::max(400.0f, screen.height);

    LinkType currentType = LinkType::Directory;
    if (g_state.linkTypeIdx == 1) currentType = LinkType::HardLink;
    if (g_state.linkTypeIdx == 2) currentType = LinkType::Junction;
    std::string cmdPreview = WideToUtf8(BuildMklinkCommand(currentType, g_state.linkPath, g_state.targetPath));

    ui.stack("root")
        .size(safeWidth, safeHeight)
        .content([&] {
            // Brighter, richer background gradient
            ui.rect("bg")
                .size(safeWidth, safeHeight)
                .gradient({0.18f, 0.22f, 0.28f, 1.0f}, {0.08f, 0.12f, 0.16f, 1.0f})
                .build();

            // Decorative blurred circles (Light bleed effects) - Increased opacity
            ui.rect("circle1")
                .position(safeWidth * 0.8f - 150.0f, safeHeight * 0.1f)
                .size(300.0f, 300.0f)
                .radius(150.0f)
                .color({0.35f, 0.2f, 0.9f, 0.65f})
                .blur(80.0f)
                .build();

            ui.rect("circle2")
                .position(safeWidth * 0.1f, safeHeight * 0.7f - 125.0f)
                .size(250.0f, 250.0f)
                .radius(125.0f)
                .color({0.0f, 0.7f, 0.8f, 0.55f})
                .blur(60.0f)
                .build();

            // Main Glass Panel
            float panelWidth = std::max(760.0f, safeWidth * 0.95f);
            float panelHeight = std::max(560.0f, safeHeight * 0.95f);
            float panelX = (safeWidth - panelWidth) * 0.5f;
            float panelY = (safeHeight - panelHeight) * 0.5f;

            components::panel(ui, "main_glass")
                .x(panelX).y(panelY)
                .size(panelWidth, panelHeight)
                .radius(20.0f)
                .color({0.20f, 0.25f, 0.32f, 0.45f})
                .border(1.0f, {1.0f, 1.0f, 1.0f, 0.25f})
                .shadow(32.0f, 0.0f, 12.0f, {0.0f, 0.0f, 0.0f, 0.5f})
                .blur(40.0f)
                .build();

            ui.column("main_content")
                .x(panelX).y(panelY)
                .size(panelWidth, panelHeight)
                .padding(24.0f)
                .content([&] {
                    float innerW = panelWidth - 48.0f;
                    ui.row("header").size(innerW, 40.0f).alignItems(core::Align::CENTER).content([&]{
                        ui.text("logo").text("Fluent Linker").fontSize(26.0f).fontWeight(700).color({0.95f, 0.95f, 1.0f, 1.0f}).size(180.0f, 30.0f).build();
                        
                        ui.stack("h_spacer").size(std::max(0.0f, innerW - 180.0f - 400.0f), 40.0f).build();

                        components::segmented(ui, "nav_segmented")
                            .size(400.0f, 38.0f)
                            .fontSize(16.0f)
                            .items({"创建快捷链接", "执行历史记录", "原理与帮助"})
                            .selected(g_state.currentTab)
                            .onChange([](int idx){ g_state.currentTab = idx; })
                            .build();
                    });

                    ui.stack("sep_stack").size(innerW, 40.0f).content([&]{
                        ui.rect("header_sep").size(innerW, 1.0f).color({1.0f, 1.0f, 1.0f, 0.15f}).position(0, 19.5f).build();
                    });

                    float contentHeight = panelHeight - 128.0f;
                    
                    if (g_state.currentTab == 0) {
                        // TAB 1: Create Link
                        float w = std::max(600.0f, std::min(1200.0f, innerW - 80.0f));
                        
                        // Use a column with alignItems(CENTER) to center all elements inside!
                        ui.column("tab1_col")
                            .size(innerW, contentHeight)
                            .alignItems(core::Align::CENTER)
                            .content([&]{
                                // Explicit sizes for EVERY element!
                                ui.stack("spacer1").size(w, 10.0f).build();

                                ui.text("t1").text("1. 选择重定向类型").fontSize(18.0f).color({0.75f, 0.80f, 0.90f, 1.0f}).size(w, 26.0f).build();
                                ui.stack("s1").size(w, 8.0f).build();
                                
                                components::segmented(ui, "type_seg")
                                    .size(w, 42.0f)
                                    .fontSize(16.0f)
                                    .items({"目录符号链接 (/D)", "文件硬链接 (/H)", "目录联接 (/J)"})
                                    .selected(g_state.linkTypeIdx)
                                    .onChange([](int idx){ g_state.linkTypeIdx = idx; })
                                    .build();
                                
                                ui.stack("s2").size(w, 24.0f).build();
                                ui.text("t2").text("2. 配置路径参数").fontSize(18.0f).color({0.75f, 0.80f, 0.90f, 1.0f}).size(w, 26.0f).build();
                                ui.stack("s3").size(w, 8.0f).build();
                                
                                auto drawInputRow = [&](const std::string& id, const std::string& label, std::string* valPtr, bool target) {
                                    ui.row(id + "_r").size(w, 40.0f).gap(8.0f).content([&, id, label, valPtr, target]{
                                        components::input(ui, id + "_in")
                                            .size(w - 180.0f, 40.0f)
                                            .fontSize(15.0f)
                                            .placeholder(label)
                                            .text(*valPtr)
                                            .onChange([valPtr](const std::string& v){ *valPtr = v; })
                                            .build();
                                        components::button(ui, id + "_bf")
                                            .size(86.0f, 40.0f)
                                            .text("选文件")
                                            .onClick([valPtr]{
                                                auto p = OpenFileDialog();
                                                if(!p.empty()) *valPtr = WideToUtf8(p);
                                            }).build();
                                        components::button(ui, id + "_bd")
                                            .size(86.0f, 40.0f)
                                            .text("选目录")
                                            .onClick([valPtr]{
                                                auto p = OpenFolderDialog();
                                                if(!p.empty()) *valPtr = WideToUtf8(p);
                                            }).build();
                                    });
                                };

                                drawInputRow("link_path", "输入要创建的快捷方式路径 (Link Path)", &g_state.linkPath, false);
                                ui.stack("gap1").size(w, 12.0f).build();
                                drawInputRow("target_path", "输入真实的目标路径 (Target Path)", &g_state.targetPath, true);

                                ui.stack("s4").size(w, 24.0f).build();
                                ui.text("t3").text("3. 执行控制").fontSize(18.0f).color({0.75f, 0.80f, 0.90f, 1.0f}).size(w, 26.0f).build();
                                ui.stack("s5").size(w, 8.0f).build();
                                
                                ui.stack("cmd_stack").size(w, 44.0f).content([&]{
                                    ui.rect("cmd_bg").size(w, 44.0f).color({0.0f, 0.0f, 0.0f, 0.35f}).radius(8.0f).border(1.0f, {1.0f, 1.0f, 1.0f, 0.1f}).build();
                                    ui.text("cmd_txt").text("> " + cmdPreview).color({0.6f, 1.0f, 0.6f, 1.0f}).fontSize(15.0f)
                                        .size(w - 20.0f, 44.0f).x(10.0f).y(0.0f).verticalAlign(core::VerticalAlign::Center).build();
                                });

                                ui.stack("gap2").size(w, 16.0f).build();

                                ValidationResult val = ValidateInputs(currentType, g_state.linkPath, g_state.targetPath);
                                ui.row("val_row").size(w, 20.0f).alignItems(core::Align::CENTER).gap(8.0f).content([&]{
                                    ui.text("val_ic").text(val.isValid ? core::dsl::utf8(0xF058) : core::dsl::utf8(0xF071)).icon(val.isValid ? 0xF058 : 0xF071).color(val.color).size(20.0f, 20.0f).build();
                                    ui.text("val_msg").text(val.message).color(val.color).fontSize(14.0f).size(w - 28.0f, 20.0f).build();
                                });

                                ui.stack("gap3").size(w, 16.0f).build();

                                ui.row("act_r").size(w, 48.0f).gap(12.0f).content([&]{
                                    components::button(ui, "run_btn")
                                        .size(w - 112.0f, 48.0f)
                                        .fontSize(16.0f)
                                        .text("创建重定向链接")
                                        .onClick([val, currentType, cmdPreview]{
                                            if(val.isValid) {
                                                std::string output;
                                                DWORD exitCode = 0;
                                                bool started = RunCommand(BuildMklinkCommand(currentType, g_state.linkPath, g_state.targetPath), output, exitCode);
                                                
                                                g_state.showToast = true;
                                                if (started && exitCode == 0) {
                                                    g_state.toastSuccess = true;
                                                    g_state.toastTitle = "创建成功";
                                                    g_state.toastMsg = "链接重定向已被安全建立。";
                                                } else {
                                                    g_state.toastSuccess = false;
                                                    g_state.toastTitle = "执行失败";
                                                    g_state.toastMsg = started ? output : "进程呼叫失败。";
                                                }
                                                if(started) {
                                                    HistoryItem item;
                                                    item.timestamp = CurrentTimestamp();
                                                    item.type = LinkTypeName(currentType);
                                                    item.linkPath = g_state.linkPath;
                                                    item.targetPath = g_state.targetPath;
                                                    item.result = output.empty() ? (exitCode == 0 ? "成功" : "失败") : output;
                                                    AppendHistory(item);
                                                    g_state.history.insert(g_state.history.begin(), item);
                                                }
                                            }
                                        }).build();
                                    
                                    components::button(ui, "clear_btn")
                                        .size(100.0f, 48.0f)
                                        .fontSize(16.0f)
                                        .text("清空")
                                        .onClick([]{
                                            g_state.linkPath = "";
                                            g_state.targetPath = "";
                                            g_state.showToast = false;
                                        }).build();
                                });
                            });
                    } else if (g_state.currentTab == 1) {
                        // TAB 2: History Logs
                        ui.column("tab2_col").size(innerW, contentHeight).content([&]{
                            ui.row("hist_search_row").size(innerW, 36.0f).alignItems(core::Align::CENTER).content([&]{
                                ui.row("hs_left").size(410.0f, 36.0f).gap(10.0f).alignItems(core::Align::CENTER).content([&]{
                                    ui.text("hist_search_lbl").text(core::dsl::utf8(0xF002) + " 记录检索").icon(0xF002).fontSize(15.0f).size(100.0f, 36.0f).verticalAlign(core::VerticalAlign::Center).build();
                                    components::input(ui, "hist_search")
                                        .size(300.0f, 36.0f)
                                        .fontSize(15.0f)
                                        .text(g_state.searchFilter)
                                        .placeholder("输入路径或时间模糊检索")
                                        .onChange([](const std::string& val){ g_state.searchFilter = val; })
                                        .build();
                                });

                                ui.stack("hs_spacer").size(std::max(0.0f, innerW - 410.0f - 140.0f), 36.0f).build();

                                components::button(ui, "open_hist_file").text("打开日志文件").fontSize(14.0f).size(140.0f, 36.0f).onClick([]{
                                    ::ShellExecuteW(nullptr, L"open", GetHistoryPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                }).build();
                            });
                            
                            ui.stack("spacer_hist").size(innerW, 16.0f).build();

                            std::vector<std::vector<std::string>> displayRows;
                            for (const auto& item : g_state.history) {
                                if (!g_state.searchFilter.empty()) {
                                    auto contains = [](const std::string& h, const std::string& n) {
                                        return h.find(n) != std::string::npos;
                                    };
                                    if (!contains(item.timestamp, g_state.searchFilter) &&
                                        !contains(item.type, g_state.searchFilter) &&
                                        !contains(item.linkPath, g_state.searchFilter) &&
                                        !contains(item.targetPath, g_state.searchFilter) &&
                                        !contains(item.result, g_state.searchFilter)) {
                                        continue;
                                    }
                                }
                                displayRows.push_back({item.timestamp, item.type, item.linkPath, item.targetPath, item.result});
                            }

                            components::scrollView(ui, "hist_scroll")
                                .size(innerW, contentHeight - 52.0f)
                                .offset(g_state.historyScroll)
                                .onChange([](float val){ g_state.historyScroll = val; })
                                .content([&](core::dsl::Ui& ui, float w, float) {
                                    float colTime = 145.0f;
                                    float colType = 135.0f;
                                    float colRes = 60.0f;
                                    float colPaths = std::max(0.0f, w - colTime - colType - colRes - 16.0f);
                                    float colLink = colPaths * 0.45f;
                                    float colTarget = colPaths * 0.55f;

                                    ui.stack("hist_head_stack").size(w, 40.0f).content([&]{
                                        ui.rect("hist_head_bg").size(w, 40.0f).color({0.15f, 0.18f, 0.22f, 1.0f}).build();
                                        ui.row("hist_head").size(w, 40.0f).alignItems(core::Align::CENTER).content([&]{
                                            ui.stack("indent").size(16.0f, 40.0f).build();
                                            ui.text("h1").text("创建时刻").fontSize(14.0f).color({0.6f, 0.7f, 0.8f, 1.0f}).size(colTime, 40.0f).verticalAlign(core::VerticalAlign::Center).build();
                                            ui.text("h2").text("类型").fontSize(14.0f).color({0.6f, 0.7f, 0.8f, 1.0f}).size(colType, 40.0f).verticalAlign(core::VerticalAlign::Center).build();
                                            ui.text("h3").text("链接存放路径").fontSize(14.0f).color({0.6f, 0.7f, 0.8f, 1.0f}).size(colLink, 40.0f).verticalAlign(core::VerticalAlign::Center).build();
                                            ui.text("h4").text("最终物理指向").fontSize(14.0f).color({0.6f, 0.7f, 0.8f, 1.0f}).size(colTarget, 40.0f).verticalAlign(core::VerticalAlign::Center).build();
                                            ui.text("h5").text("结果").fontSize(14.0f).color({0.6f, 0.7f, 0.8f, 1.0f}).size(colRes, 40.0f).verticalAlign(core::VerticalAlign::Center).build();
                                        });
                                    });

                                    for (size_t i = 0; i < displayRows.size(); i++) {
                                        auto& row = displayRows[i];
                                        ui.stack("hr_stack_" + std::to_string(i)).size(w, 64.0f).content([&]{
                                            ui.rect("hr_bg_" + std::to_string(i)).size(w, 64.0f).color(i % 2 == 0 ? core::Color{0.25f, 0.28f, 0.32f, 0.2f} : core::Color{0.0f, 0.0f, 0.0f, 0.0f}).build();
                                            ui.row("hr_" + std::to_string(i))
                                                .size(w, 64.0f)
                                                .alignItems(core::Align::CENTER)
                                                .content([&]{
                                                    ui.stack("indent" + std::to_string(i)).size(16.0f, 64.0f).build();
                                                    ui.text("r1_" + std::to_string(i)).text(row[0]).fontSize(13.0f).color({0.9f, 0.9f, 0.9f, 1.0f}).size(colTime, 64.0f).verticalAlign(core::VerticalAlign::Center).wrap(true).build();
                                                    ui.text("r2_" + std::to_string(i)).text(row[1]).fontSize(13.0f).color({0.9f, 0.9f, 0.9f, 1.0f}).size(colType, 64.0f).verticalAlign(core::VerticalAlign::Center).wrap(true).build();
                                                    ui.text("r3_" + std::to_string(i)).text(row[2]).fontSize(13.0f).color({0.8f, 0.9f, 1.0f, 1.0f}).size(colLink, 64.0f).verticalAlign(core::VerticalAlign::Center).wrap(true).build();
                                                    ui.text("r4_" + std::to_string(i)).text(row[3]).fontSize(13.0f).color({0.8f, 1.0f, 0.8f, 1.0f}).size(colTarget, 64.0f).verticalAlign(core::VerticalAlign::Center).wrap(true).build();
                                                    ui.text("r5_" + std::to_string(i)).text(row[4]).fontSize(13.0f).color(row[4] == "成功" ? core::Color{0.4f, 0.9f, 0.4f, 1.0f} : core::Color{0.9f, 0.4f, 0.4f, 1.0f}).size(colRes, 64.0f).verticalAlign(core::VerticalAlign::Center).wrap(true).build();
                                                });
                                        });
                                        ui.rect("div_" + std::to_string(i)).size(w, 1.0f).color({1.0f, 1.0f, 1.0f, 0.05f}).build();
                                    }
                                }).build();
                        });
                    } else if (g_state.currentTab == 2) {
                        // TAB 3: About
                        ui.column("tab3_col").size(innerW, contentHeight).content([&]{
                            auto drawFAQ = [&](int id, const std::string& title, const std::string& syntax, const std::string& detail) {
                                ui.stack("faq_container_" + std::to_string(id))
                                    .size(innerW, 140.0f)
                                    .content([&]{
                                        components::panel(ui, "faq_" + std::to_string(id))
                                            .size(innerW, 140.0f)
                                            .color({0.0f, 0.0f, 0.0f, 0.25f})
                                            .border(1.0f, {1.0f, 1.0f, 1.0f, 0.1f})
                                            .radius(12.0f)
                                            .build();
                                            
                                        ui.column("faq_col_" + std::to_string(id))
                                            .size(innerW, 140.0f)
                                            .padding(16.0f)
                                            .gap(6.0f)
                                            .content([&]{
                                                ui.text("faq_t_" + std::to_string(id)).text(title).color({0.5f, 0.85f, 1.0f, 1.0f}).fontSize(18.0f).fontWeight(700).size(innerW - 32.0f, 26.0f).build();
                                                ui.text("faq_s_" + std::to_string(id)).text("语法: " + syntax).color({0.7f, 1.0f, 0.7f, 1.0f}).fontSize(15.0f).size(innerW - 32.0f, 22.0f).build();
                                                ui.text("faq_d_" + std::to_string(id)).text(detail).color({0.8f, 0.8f, 0.8f, 1.0f}).fontSize(14.0f).size(innerW - 32.0f, 60.0f).wrap(true).build();
                                            });
                                    });
                                ui.stack("spacer_faq_" + std::to_string(id)).size(innerW, 16.0f).build();
                            };

                            drawFAQ(1, "目录符号链接 (Directory Symbolic Link) [/D]", "mklink /D <链接名称> <实际目标文件夹>", "生成一个轻量级的绝对/相对指针。移除该链接不会损害源文件。若在链接内修改内容，会同步影响源文件。兼容性绝佳，跨盘必备。");
                            drawFAQ(2, "文件硬链接 (File Hard Link) [/H]", "mklink /H <链接名字> <实际源文件>", "共享同一块磁盘物理扇区，完全不产生体积增长。限制是严禁跨分区。所有的文件改动双向同步，安全可靠。");
                            drawFAQ(3, "目录联接 (Directory Junction) [/J]", "mklink /J <链接名字> <实际目标文件夹>", "低级别路径桥接手段，适用于本地硬盘重定向，不支持网络映射驱动器。不需要管理员权限也能快速建立。");
                        });
                    }
                });

            // Toast overlay
            if (g_state.showToast) {
                components::toast(ui, "action_toast")
                    .screen(safeWidth, safeHeight)
                    .visible(g_state.showToast)
                    .title(g_state.toastTitle)
                    .message(g_state.toastMsg)
                    .icon(g_state.toastSuccess ? 0xF058 : 0xF071)
                    .autoDismiss(g_state.toastSuccess ? 3.0f : 0.0f)
                    .onAutoDismiss([]{ g_state.showToast = false; })
                    .onDismiss([]{ g_state.showToast = false; })
                    .build();
            }
        });
}

} // namespace app
