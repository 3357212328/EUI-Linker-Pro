#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <dxgi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h> // For native Windows IFileOpenDialog
#include <objbase.h>  // For COM CoInitializeEx

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem> // C++20 Filesystem for path validation
#include "resource.h" // Icon resource ID

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0;
static UINT g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool g_LightTheme = false;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

enum class LinkType
{
    Directory,
    HardLink,
    Junction
};

struct HistoryItem
{
    std::string timestamp;
    std::string type;
    std::string linkPath;
    std::string targetPath;
    std::string result;
};

// Fluent UI Path Validation Result
struct ValidationResult
{
    bool isValid = true;
    std::string message = "等待输入路径...";
    ImVec4 color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
};

static std::wstring Utf8ToWide(const char* text)
{
    if (text == nullptr || text[0] == '\0')
        return L"";

    int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (needed <= 0)
        return L"";

    std::wstring wide(needed - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), needed);
    return wide;
}

static std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
        return "";

    int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
        return "";

    std::string utf8(needed - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

static std::wstring GetHistoryPath()
{
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
        return L"mklink_history.tsv";

    std::wstring path = localAppData;
    CoTaskMemFree(localAppData);
    path += L"\\MyModernUI_mklink_history.tsv";
    return path;
}

static std::string CurrentTimestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt = system_clock::to_time_t(now);
    tm localTime{};
    localtime_s(&localTime, &tt);

    char buffer[64] = {};
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

static std::wstring QuoteArg(const std::wstring& value)
{
    std::wstring escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back(L'"');
    for (wchar_t ch : value)
    {
        if (ch == L'"')
            escaped.push_back(L'\\');
        escaped.push_back(ch);
    }
    escaped.push_back(L'"');
    return escaped;
}

static const char* LinkTypeName(LinkType type)
{
    switch (type)
    {
    case LinkType::Directory: return "Directory symlink";
    case LinkType::HardLink: return "File hard link";
    case LinkType::Junction: return "Directory junction";
    }
    return "Directory symlink";
}

static std::wstring BuildMklinkCommand(LinkType type, const char* linkPath, const char* targetPath)
{
    std::wstring command = L"mklink ";
    if (type == LinkType::Directory)
        command += L"/D ";
    else if (type == LinkType::HardLink)
        command += L"/H ";
    else
        command += L"/J ";

    command += QuoteArg(Utf8ToWide(linkPath));
    command += L" ";
    command += QuoteArg(Utf8ToWide(targetPath));
    return command;
}

static bool RunCommand(const std::wstring& command, std::string& output, DWORD& exitCode)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
        return false;
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

    if (!created)
    {
        CloseHandle(readPipe);
        return false;
    }

    std::wstring wideOutput;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        int needed = MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, nullptr, 0);
        if (needed > 0)
        {
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

static std::string SanitizeField(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }
    return value;
}

static void LoadHistory(std::vector<HistoryItem>& history)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, GetHistoryPath().c_str(), L"rb") != 0 || file == nullptr)
        return;

    char line[8192];
    while (fgets(line, sizeof(line), file))
    {
        std::string text = line;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.pop_back();

        std::vector<std::string> fields;
        size_t start = 0;
        while (true)
        {
            size_t pos = text.find('\t', start);
            fields.push_back(text.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
            if (pos == std::string::npos)
                break;
            start = pos + 1;
        }

        if (fields.size() >= 5)
            history.push_back({ fields[0], fields[1], fields[2], fields[3], fields[4] });
    }

    fclose(file);
}

static void AppendHistory(const HistoryItem& item)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, GetHistoryPath().c_str(), L"ab") != 0 || file == nullptr)
        return;

    std::string line = SanitizeField(item.timestamp) + "\t"
        + SanitizeField(item.type) + "\t"
        + SanitizeField(item.linkPath) + "\t"
        + SanitizeField(item.targetPath) + "\t"
        + SanitizeField(item.result) + "\n";
    fwrite(line.data(), 1, line.size(), file);
    fclose(file);
}

// Fluent UI Style Application
// Fluent UI Size Parameters
static void ApplyFluentSizes(float scale)
{
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Roundings (Fluent UI standard)
    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    
    // Border Sizes
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    
    // Paddings & Spacings
    style.WindowPadding = ImVec2(20.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.ItemSpacing = ImVec2(12.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;
    
    style.ScaleAllSizes(scale);
}

// Fluent UI Colors - Dark Theme
static void ApplyFluentDarkColors()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Backgrounds
    colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.10f, 1.00f); // #1a1a1a - Fluent Mica dark background
    colors[ImGuiCol_ChildBg]              = ImVec4(0.13f, 0.13f, 0.13f, 1.00f); // #202020 - Cards / sidebar background
    colors[ImGuiCol_PopupBg]              = ImVec4(0.15f, 0.15f, 0.15f, 0.98f); // #262626 - Dropdowns / popups
    
    // Borders
    colors[ImGuiCol_Border]               = ImVec4(0.24f, 0.24f, 0.24f, 1.00f); // #3d3d3d - Subtle separator borders
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // Text
    colors[ImGuiCol_Text]                 = ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // Off-white crisp text
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.62f, 0.62f, 0.62f, 1.00f); // Brighter gray secondary text
    
    // Headers (Selection & Navigation)
    colors[ImGuiCol_Header]               = ImVec4(0.00f, 0.47f, 0.83f, 0.35f); // Soft blue selection
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.00f, 0.47f, 0.83f, 0.55f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.00f, 0.47f, 0.83f, 0.75f);
    
    // Buttons (Fluent standard button rest/hover/active)
    colors[ImGuiCol_Button]               = ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // #2e2e2e
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.24f, 0.24f, 0.24f, 1.00f); // #3d3d3d
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.15f, 0.15f, 0.15f, 1.00f); // #262626
    
    // Input fields and combo boxes
    colors[ImGuiCol_FrameBg]              = ImVec4(0.15f, 0.15f, 0.15f, 1.00f); // #262626
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.20f, 0.20f, 0.20f, 1.00f); // #333333
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // #1f1f1f
    
    // Navigation Tabs
    colors[ImGuiCol_Tab]                  = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    
    // Accents (Checkmarks, Sliders)
    colors[ImGuiCol_CheckMark]            = ImVec4(0.38f, 0.80f, 1.00f, 1.00f); // Windows 11 bright accent blue (#60cdff)
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.38f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.35f, 0.35f, 0.35f, 0.45f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.45f, 0.55f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.55f, 0.55f, 0.55f, 0.75f);
    
    // Tables
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    
    // Separation lines
    colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.38f, 0.80f, 1.00f, 1.00f);
    
    colors[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
}

// Fluent UI Colors - Light Theme
static void ApplyFluentLightColors()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Backgrounds
    colors[ImGuiCol_WindowBg]             = ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // #f3f3f3 - Fluent Light Window Bg
    colors[ImGuiCol_ChildBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // #ffffff - White cards background
    colors[ImGuiCol_PopupBg]              = ImVec4(1.00f, 1.00f, 1.00f, 0.98f); // #ffffff
    
    // Borders
    colors[ImGuiCol_Border]               = ImVec4(0.82f, 0.82f, 0.82f, 1.00f); // #d2d2d2 - Soft gray borders
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // Text
    colors[ImGuiCol_Text]                 = ImVec4(0.13f, 0.13f, 0.13f, 1.00f); // #202020 - Dark charcoal text
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.41f, 0.41f, 0.41f, 1.00f); // #686868 - Secondary disabled text
    
    // Headers (Selection & Navigation)
    colors[ImGuiCol_Header]               = ImVec4(0.00f, 0.47f, 0.83f, 0.15f); // Soft standard blue selection
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.00f, 0.47f, 0.83f, 0.25f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.00f, 0.47f, 0.83f, 0.35f);
    
    // Buttons (Fluent standard button rest/hover/active)
    colors[ImGuiCol_Button]               = ImVec4(0.92f, 0.92f, 0.92f, 1.00f); // #eaeaea
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.84f, 0.84f, 0.84f, 1.00f); // #d5d5d5
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.80f, 0.80f, 0.80f, 1.00f); // #cccccc
    
    // Input fields and combo boxes
    colors[ImGuiCol_FrameBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // #ffffff
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.96f, 0.96f, 0.96f, 1.00f); // #f5f5f5
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.90f, 0.90f, 0.90f, 1.00f); // #e5e5e5
    
    // Navigation Tabs
    colors[ImGuiCol_Tab]                  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    
    // Accents (Checkmarks, Sliders)
    colors[ImGuiCol_CheckMark]            = ImVec4(0.00f, 0.47f, 0.83f, 1.00f); // Windows 11 Standard Accent Blue (#0078d4)
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.06f, 0.43f, 0.75f, 1.00f); // #106ebe
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.80f, 0.80f, 0.80f, 0.45f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.69f, 0.69f, 0.69f, 0.55f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.56f, 0.56f, 0.56f, 0.75f);
    
    // Tables
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(0.00f, 0.00f, 0.00f, 0.02f);
    
    // Separation lines
    colors[ImGuiCol_Separator]            = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.77f, 0.77f, 0.77f, 1.00f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
    
    colors[ImGuiCol_TitleBg]              = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
}

// Apply primary accent coloring to a button
static void PushAccentButtonStyle()
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.47f, 0.83f, 1.00f));       // Accent Blue rest
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.53f, 0.90f, 1.00f)); // Lighter Blue hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.40f, 0.70f, 1.00f));  // Darker Blue active
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));          // Crisp white text
}

static void PopAccentButtonStyle()
{
    ImGui::PopStyleColor(4);
}

// Native Windows File/Folder browse dialogs using IFileOpenDialog
static std::wstring OpenFolderDialog(HWND hwndOwner)
{
    std::wstring path;
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr))
    {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
        {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        if (SUCCEEDED(pFileOpen->Show(hwndOwner)))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                {
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

static std::wstring OpenFileDialog(HWND hwndOwner)
{
    std::wstring path;
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr))
    {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
        {
            pFileOpen->SetOptions(dwOptions | FOS_FORCEFILESYSTEM);
        }
        if (SUCCEEDED(pFileOpen->Show(hwndOwner)))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                {
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

// Live path validation using standard C++20 filesystem
static ValidationResult ValidateInputs(LinkType type, const char* linkPath, const char* targetPath)
{
    ValidationResult res;
    if (linkPath[0] == '\0' && targetPath[0] == '\0')
    {
        res.isValid = false;
        res.message = "等待输入路径...";
        res.color = ImVec4(0.55f, 0.55f, 0.55f, 1.00f); // Slate gray
        return res;
    }

    if (linkPath[0] == '\0')
    {
        res.isValid = false;
        res.message = "请输入链接存放路径 (Link Path)";
        res.color = ImVec4(0.95f, 0.65f, 0.20f, 1.00f); // Warning orange
        return res;
    }

    if (targetPath[0] == '\0')
    {
        res.isValid = false;
        res.message = "请输入真实目标路径 (Target Path)";
        res.color = ImVec4(0.95f, 0.65f, 0.20f, 1.00f);
        return res;
    }

    try
    {
        std::wstring wLink = Utf8ToWide(linkPath);
        std::wstring wTarget = Utf8ToWide(targetPath);
        
        std::filesystem::path pLink(wLink);
        std::filesystem::path pTarget(wTarget);

        // 1. Verify Target
        if (!std::filesystem::exists(pTarget))
        {
            res.isValid = false;
            res.message = "[!] 目标路径不存在! 执行 mklink 可能会失败。";
            res.color = ImVec4(0.95f, 0.25f, 0.25f, 1.00f); // Red
            return res;
        }

        bool targetIsDir = std::filesystem::is_directory(pTarget);
        if (type == LinkType::HardLink && targetIsDir)
        {
            res.isValid = false;
            res.message = "[X] 文件硬链接 (/H) 的目标必须是文件，不能是目录文件夹!";
            res.color = ImVec4(0.95f, 0.25f, 0.25f, 1.00f);
            return res;
        }
        if ((type == LinkType::Directory || type == LinkType::Junction) && !targetIsDir)
        {
            res.isValid = false;
            res.message = "[X] 目录软链接 (/D) 或联接 (/J) 的目标必须是目录，不能是文件!";
            res.color = ImVec4(0.95f, 0.25f, 0.25f, 1.00f);
            return res;
        }

        // 2. Verify Link path itself
        if (std::filesystem::exists(pLink))
        {
            res.isValid = false;
            res.message = "[X] 链接路径的文件或文件夹已存在! Windows 不允许在已有位置重复创建链接。";
            res.color = ImVec4(0.95f, 0.25f, 0.25f, 1.00f);
            return res;
        }

        // 3. Verify parent directory exists
        if (pLink.has_parent_path() && !pLink.parent_path().empty() && !std::filesystem::exists(pLink.parent_path()))
        {
            res.isValid = false;
            res.message = "[X] 链接文件所在的父级文件夹不存在! 请先创建该层目录。";
            res.color = ImVec4(0.95f, 0.25f, 0.25f, 1.00f);
            return res;
        }

        // Configuration checks out perfectly
        res.isValid = true;
        res.message = "[OK] 路径配置完全有效! 已满足 mklink 创建参数标准，准备就绪。";
        res.color = ImVec4(0.38f, 0.85f, 0.38f, 1.00f); // Bright green
    }
    catch (const std::exception& e)
    {
        res.isValid = false;
        res.message = std::string("路径有效性解析出错: ") + e.what();
        res.color = ImVec4(0.95f, 0.25f, 0.25f, 1.00f);
    }
    return res;
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    // Initialize Windows COM library for Native Dialog Shell calls
    HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    ImGui_ImplWin32_EnableDpiAwareness();
    float mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    HINSTANCE hInst = GetModuleHandle(nullptr);
    HICON hAppIcon   = ::LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
    HICON hAppIconSm = ::LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInst, hAppIcon, nullptr, nullptr, nullptr, L"MyModernUI", hAppIconSm };
    ::RegisterClassExW(&wc);
    // Beautiful Fluent GUI scale window
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Fluent Linker - Windows 链接重定向大师 (mklink UI)", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * mainScale), (int)(860 * mainScale), nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        if (SUCCEEDED(hrCo))
            ::CoUninitialize();
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ApplyFluentSizes(mainScale);
    if (g_LightTheme)
        ApplyFluentLightColors();
    else
        ApplyFluentDarkColors();

    // Dynamic Font Loading - Supports Segoe UI / Microsoft YaHei high-DPI scaling
    float fontSize = 18.0f * mainScale;
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;
    ImFont* customFont = nullptr;
    
    // Attempt Microsoft YaHei from system fonts first (for perfect Chinese rendering)
    customFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", fontSize, &fontConfig, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    if (!customFont)
    {
        customFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttf", fontSize, &fontConfig, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    }
    if (!customFont)
    {
        customFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", fontSize, &fontConfig, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    }
    
    if (customFont)
    {
        style.FontScaleDpi = 1.0f; // Crisp scaled fonts
    }
    else
    {
        io.Fonts->AddFontDefault();
        style.FontScaleDpi = mainScale;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    std::vector<HistoryItem> history;
    LoadHistory(history);

    LinkType linkType = LinkType::Directory;
    char linkPath[512] = "";
    char targetPath[512] = "";
    char commandPreview[1200] = "";
    char statusText[512] = " Ready (就绪)";
    char outputText[4096] = "";

    auto refreshCommandPreview = [&]()
    {
        std::string preview = WideToUtf8(BuildMklinkCommand(linkType, linkPath, targetPath));
        strncpy_s(commandPreview, preview.c_str(), _TRUNCATE);
    };
    refreshCommandPreview();

    // Native navigation active state managed automatically by ImGui TabBar

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_SwapChainOccluded && g_pSwapChain && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ----------------- MAIN NATIVE IMGUI WINDOW (FITS VIEWPORT WITH TITLE BAR & MENU BAR) -----------------
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
                                       
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        
        ImGui::Begin("Fluent Linker - Windows 链接重定向大师 (mklink UI)", nullptr, windowFlags);
        ImGui::PopStyleVar(2);

        // Native MenuBar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("文件 (File)"))
            {
                if (ImGui::MenuItem("退出程序 (Exit)", "Alt+F4"))
                {
                    ::PostQuitMessage(0);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("界面主题 (Theme)"))
            {
                if (ImGui::MenuItem("浅色模式 (Light)", nullptr, g_LightTheme))
                {
                    g_LightTheme = true;
                    ApplyFluentLightColors();
                }
                if (ImGui::MenuItem("深色模式 (Dark)", nullptr, !g_LightTheme))
                {
                    g_LightTheme = false;
                    ApplyFluentDarkColors();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Native TabBar
        if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("创建链接 (Create Link)"))
            {
                ImGui::Spacing();
                ImGui::TextUnformatted("创建软链接 / 硬链接 / 目录联接");
                ImGui::TextDisabled("通过直观美观的界面一键创建快捷指向，省去痛苦的手动记忆命令行参数。");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();

                float totalWidth = ImGui::GetContentRegionAvail().x;
                float leftColumnWidth = totalWidth * 0.55f;
                float rightColumnWidth = totalWidth - leftColumnWidth - 12.0f * mainScale;

                // --- LEFT COLUMN (55% width) ---
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * mainScale, 14.0f * mainScale));
                ImGui::BeginChild("LeftParamsPanel", ImVec2(leftColumnWidth, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
                ImGui::PopStyleVar();
                
                // Choice cards
                ImGui::TextColored(g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f), "第一步：选择快捷链接类型");
                ImGui::Spacing();
                
                // Choice cards rendering
                float cardWidth = (ImGui::GetContentRegionAvail().x - 24.0f * mainScale) / 3.0f;
                float cardHeight = 110.0f * mainScale;
                for (int i = 0; i < 3; i++)
                {
                    bool isSelected = (static_cast<int>(linkType) == i);
                    
                    ImVec4 cardBg;
                    ImVec4 cardBorder;
                    if (isSelected)
                    {
                        cardBg = g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 0.08f) : ImVec4(0.00f, 0.47f, 0.83f, 0.20f);
                        cardBorder = g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f);
                    }
                    else
                    {
                        cardBg = g_LightTheme ? ImVec4(1.00f, 1.00f, 1.00f, 1.00f) : ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
                        cardBorder = g_LightTheme ? ImVec4(0.85f, 0.85f, 0.85f, 1.00f) : ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBg);
                    ImGui::PushStyleColor(ImGuiCol_Border, cardBorder);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, isSelected ? 2.0f : 1.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * mainScale, 10.0f * mainScale));
                    
                    char childName[64];
                    sprintf_s(childName, "ChoiceCard_%d", i);
                    
                    ImGui::BeginChild(childName, ImVec2(cardWidth, cardHeight), true, ImGuiWindowFlags_NoScrollbar);
                    
                    if (i == 0)
                    {
                        ImGui::Text("目录符号链接");
                        ImGui::SameLine(cardWidth - 40.0f * mainScale);
                        ImGui::TextColored(isSelected ? (g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f)) : ImVec4(0.55f, 0.55f, 0.55f, 1.00f), "/D");
                        ImGui::Spacing();
                        ImGui::TextDisabled("用于文件夹，兼容性好\n跨硬盘，最推荐选用");
                    }
                    else if (i == 1)
                    {
                        ImGui::Text("文件硬链接");
                        ImGui::SameLine(cardWidth - 40.0f * mainScale);
                        ImGui::TextColored(isSelected ? (g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f)) : ImVec4(0.55f, 0.55f, 0.55f, 1.00f), "/H");
                        ImGui::Spacing();
                        ImGui::TextDisabled("物理单文件共享，无占用\n严禁跨硬盘分区");
                    }
                    else if (i == 2)
                    {
                        ImGui::Text("目录联接");
                        ImGui::SameLine(cardWidth - 40.0f * mainScale);
                        ImGui::TextColored(isSelected ? (g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f)) : ImVec4(0.55f, 0.55f, 0.55f, 1.00f), "/J");
                        ImGui::Spacing();
                        ImGui::TextDisabled("本地文件夹重定向\n经典低级路径管道");
                    }
                    
                    ImGui::EndChild();
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        if (ImGui::IsItemClicked())
                        {
                            linkType = static_cast<LinkType>(i);
                            refreshCommandPreview();
                        }
                    }
                    
                    ImGui::PopStyleVar(3);
                    ImGui::PopStyleColor(2);
                    
                    if (i < 2)
                        ImGui::SameLine(0.0f, 12.0f * mainScale);
                }

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::TextColored(g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f), "第二步：配置路径参数");
                ImGui::Spacing();

                // Link path configuration
                ImGui::Text("1. 链接路径 (Link Path) - 快捷文件或文件夹要在哪创建：");
                ImGui::Spacing();
                
                float rm2 = 10.0f * mainScale; // right-side inset for left panel rows
                float buttonAreaWidth = 215.0f * mainScale;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonAreaWidth - 8.0f * mainScale - rm2);
                if (ImGui::InputText("##LinkPath", linkPath, IM_ARRAYSIZE(linkPath)))
                    refreshCommandPreview();
                
                ImGui::SameLine();
                if (ImGui::Button("浏览文件##LinkFile", ImVec2(100.0f * mainScale, 0.0f)))
                {
                    std::wstring path = OpenFileDialog(hwnd);
                    if (!path.empty())
                    {
                        std::string u8 = WideToUtf8(path);
                        strncpy_s(linkPath, u8.c_str(), _TRUNCATE);
                        refreshCommandPreview();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("浏览目录##LinkDir", ImVec2(100.0f * mainScale, 0.0f)))
                {
                    std::wstring path = OpenFolderDialog(hwnd);
                    if (!path.empty())
                    {
                        std::string u8 = WideToUtf8(path);
                        strncpy_s(linkPath, u8.c_str(), _TRUNCATE);
                        refreshCommandPreview();
                    }
                }

                ImGui::Spacing();
                ImGui::Spacing();

                // Target path configuration
                ImGui::Text("2. 目标路径 (Target Path) - 该快捷方式实际指向的真实目录或文件：");
                ImGui::Spacing();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonAreaWidth - 8.0f * mainScale - rm2);
                if (ImGui::InputText("##TargetPath", targetPath, IM_ARRAYSIZE(targetPath)))
                    refreshCommandPreview();
                
                ImGui::SameLine();
                if (ImGui::Button("浏览文件##TargetFile", ImVec2(100.0f * mainScale, 0.0f)))
                {
                    std::wstring path = OpenFileDialog(hwnd);
                    if (!path.empty())
                    {
                        std::string u8 = WideToUtf8(path);
                        strncpy_s(targetPath, u8.c_str(), _TRUNCATE);
                        refreshCommandPreview();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("浏览目录##TargetDir", ImVec2(100.0f * mainScale, 0.0f)))
                {
                    std::wstring path = OpenFolderDialog(hwnd);
                    if (!path.empty())
                    {
                        std::string u8 = WideToUtf8(path);
                        strncpy_s(targetPath, u8.c_str(), _TRUNCATE);
                        refreshCommandPreview();
                    }
                }

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Live Path Validations
                ValidationResult val = ValidateInputs(linkType, linkPath, targetPath);
                ImGui::Text("路径校验状态：");
                ImGui::SameLine();
                ImGui::TextColored(val.color, "%s", val.message.c_str());

                ImGui::EndChild(); // End LeftParamsPanel

                ImGui::SameLine(0.0f, 16.0f * mainScale);

                // --- RIGHT COLUMN (45% width) ---
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * mainScale, 14.0f * mainScale));
                ImGui::BeginChild("RightConsolePanel", ImVec2(rightColumnWidth, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
                ImGui::PopStyleVar();

                ImGui::TextColored(g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f), "第三步：控制中心");
                ImGui::Spacing();

                float rm = 10.0f * mainScale; // right-side inset margin for full-width items

                ImGui::Text("底层命令行预览 (Command Preview)：");
                ImGui::Spacing();
                // Force dark terminal styling for code preview regardless of theme
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.05f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.90f, 0.30f, 1.00f)); // Matrix green text
                ImGui::InputTextMultiline("##CommandPreview", commandPreview, IM_ARRAYSIZE(commandPreview),
                    ImVec2(ImGui::GetContentRegionAvail().x - rm, 60.0f * mainScale), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor(2);

                ImGui::Spacing();
                ImGui::Spacing();

                // Actions panel: Big buttons
                bool canRun = val.isValid;
                if (!canRun)
                    ImGui::BeginDisabled();

                PushAccentButtonStyle();
                if (ImGui::Button("一键创建快捷链接 (Run)",
                    ImVec2(ImGui::GetContentRegionAvail().x - rm, 45.0f * mainScale)))
                {
                    refreshCommandPreview();
                    std::string output;
                    DWORD exitCode = 0;
                    bool started = RunCommand(BuildMklinkCommand(linkType, linkPath, targetPath), output, exitCode);

                    if (started)
                    {
                        strncpy_s(outputText, output.c_str(), _TRUNCATE);
                        if (exitCode == 0)
                        {
                            strncpy_s(statusText, "创建成功! Windows 目录/文件硬核重定向已完成。", _TRUNCATE);
                        }
                        else
                        {
                            strncpy_s(statusText, "创建失败! 底层指令执行错误，请对照下方系统信息分析排查。", _TRUNCATE);
                        }

                        HistoryItem item;
                        item.timestamp = CurrentTimestamp();
                        item.type = LinkTypeName(linkType);
                        item.linkPath = linkPath;
                        item.targetPath = targetPath;
                        item.result = output.empty() ? statusText : output;
                        AppendHistory(item);
                        history.insert(history.begin(), item);
                    }
                    else
                    {
                        strncpy_s(statusText, "无法呼唤 cmd.exe，可能当前安全防护限制了高级特权进程启动。", _TRUNCATE);
                    }
                }
                PopAccentButtonStyle();

                if (!canRun)
                    ImGui::EndDisabled();

                ImGui::Spacing();

                if (ImGui::Button("重置参数 (Clear)",
                    ImVec2(ImGui::GetContentRegionAvail().x - rm, 36.0f * mainScale)))
                {
                    linkPath[0] = '\0';
                    targetPath[0] = '\0';
                    outputText[0] = '\0';
                    strncpy_s(statusText, " Ready (就绪)", _TRUNCATE);
                    refreshCommandPreview();
                }

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Execution feedbacks
                ImGui::Text("系统执行反馈 (Console Outputs)：");
                ImGui::Spacing();
                
                // Capsule status badge
                ImVec4 badgeBg;
                ImVec4 badgeText;
                if (strstr(statusText, "成功") != nullptr)
                {
                    badgeBg = ImVec4(0.10f, 0.40f, 0.15f, 0.20f);
                    badgeText = ImVec4(0.38f, 0.85f, 0.38f, 1.00f);
                }
                else if (strstr(statusText, "失败") != nullptr || strstr(statusText, "无法") != nullptr)
                {
                    badgeBg = ImVec4(0.50f, 0.10f, 0.10f, 0.20f);
                    badgeText = ImVec4(0.95f, 0.35f, 0.35f, 1.00f);
                }
                else
                {
                    badgeBg = g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 0.08f) : ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
                    badgeText = g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
                }

                ImGui::PushStyleColor(ImGuiCol_ChildBg, badgeBg);
                ImGui::PushStyleColor(ImGuiCol_Border, badgeText);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * mainScale, 6.0f * mainScale));
                ImGui::BeginChild("StatusBadge", ImVec2(ImGui::GetContentRegionAvail().x - rm, 32.0f * mainScale), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(badgeText, "%s", statusText);
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);

                ImGui::Spacing();
                
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.05f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.00f));
                ImGui::InputTextMultiline("##OutputText", outputText, IM_ARRAYSIZE(outputText),
                    ImVec2(ImGui::GetContentRegionAvail().x - rm, 100.0f * mainScale), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor(2);

                ImGui::EndChild(); // End RightConsolePanel
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("历史记录 (History Logs)"))
            {
                ImGui::Spacing();
                ImGui::TextUnformatted("历史记录中心 (History logs)");
                ImGui::TextDisabled("追溯并全局过滤该计算机上通过本软件成功或尝试创建的所有快捷链接项。");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();

                // Filter search controls
                static char searchFilter[256] = "";
                ImGui::Text("模糊关键字检索：");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(320.0f * mainScale);
                ImGui::InputText("##SearchFilter", searchFilter, IM_ARRAYSIZE(searchFilter));

                ImGui::SameLine();
                if (ImGui::Button("打开原始历史日志文件"))
                {
                    ::ShellExecuteW(nullptr, L"open", GetHistoryPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }

                ImGui::Spacing();
                ImGui::Spacing();

                static int selectedHistoryIdx = -1;

                // Main logs table using modern Fluent borders
                if (ImGui::BeginTable("HistoryTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 320.0f * mainScale)))
                {
                    ImGui::TableSetupColumn("创建时刻 (Timestamp)", ImGuiTableColumnFlags_WidthFixed, 180.0f * mainScale);
                    ImGui::TableSetupColumn("类型 (Type)", ImGuiTableColumnFlags_WidthFixed, 140.0f * mainScale);
                    ImGui::TableSetupColumn("链接存放路径 (Link Location)", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("最终物理指向 (Actual Target)", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("命令状态/结果", ImGuiTableColumnFlags_WidthFixed, 180.0f * mainScale);
                    ImGui::TableHeadersRow();

                    int displayedRow = 0;
                    for (size_t i = 0; i < history.size(); ++i)
                    {
                        const auto& item = history[i];

                        // Filtering logic
                        if (searchFilter[0] != '\0')
                        {
                            std::string needle = searchFilter;
                            auto contains = [](const std::string& haystack, const std::string& needle) {
                                return haystack.find(needle) != std::string::npos;
                            };
                            if (!contains(item.timestamp, needle) &&
                                !contains(item.type, needle) &&
                                !contains(item.linkPath, needle) &&
                                !contains(item.targetPath, needle) &&
                                !contains(item.result, needle))
                            {
                                continue;
                            }
                        }

                        ImGui::TableNextRow();

                        // Create selectable cell to span whole row
                        ImGui::TableSetColumnIndex(0);
                        char itemLabel[256];
                        sprintf_s(itemLabel, "%s##history_row_%zu", item.timestamp.c_str(), i);

                        bool isSelected = (selectedHistoryIdx == static_cast<int>(i));
                        if (ImGui::Selectable(itemLabel, isSelected, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            selectedHistoryIdx = static_cast<int>(i);
                        }

                        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(item.type.c_str());
                        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(item.linkPath.c_str());
                        ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(item.targetPath.c_str());
                        ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(item.result.c_str());

                        displayedRow++;
                    }
                    ImGui::EndTable();
                }

                // Record Details Inspector Panel
                if (selectedHistoryIdx >= 0 && selectedHistoryIdx < static_cast<int>(history.size()))
                {
                    const auto& item = history[selectedHistoryIdx];

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::TextColored(g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f), "选中记录详情卡片 (Selected Inspector Drawer):");
                    ImGui::Spacing();

                    ImGui::BeginChild("LogInspectorCard", ImVec2(-1, 0.0f), true, 0);
                    
                    ImGui::Text("发生时间:  %s", item.timestamp.c_str());
                    ImGui::Text("链接类型:  %s", item.type.c_str());
                    
                    ImGui::Spacing();
                    
                    ImGui::Text("链接地址:  %s", item.linkPath.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("在资源管理器中打开父级"))
                    {
                        std::wstring wPath = Utf8ToWide(item.linkPath.c_str());
                        std::filesystem::path p(wPath);
                        std::wstring parent = p.has_parent_path() ? p.parent_path().wstring() : wPath;
                        ::ShellExecuteW(nullptr, L"open", parent.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    }

                    ImGui::Text("真实目标:  %s", item.targetPath.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("在资源管理器中打开目标"))
                    {
                        std::wstring wPath = Utf8ToWide(item.targetPath.c_str());
                        std::filesystem::path p(wPath);
                        std::wstring parent = p.has_parent_path() ? p.parent_path().wstring() : wPath;
                        ::ShellExecuteW(nullptr, L"open", parent.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    }

                    ImGui::Spacing();
                    ImGui::Text("控制台生成输出日志:");
                    ImGui::Spacing();
                    
                    static char drawerLog[4096];
                    strncpy_s(drawerLog, item.result.c_str(), _TRUNCATE);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.08f, 1.00f));
                    ImGui::InputTextMultiline("##DrawerLog", drawerLog, IM_ARRAYSIZE(drawerLog), ImVec2(-1, 80.0f * mainScale), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopStyleColor();

                    ImGui::Spacing();
                    if (ImGui::Button("拷贝该条目参数到系统剪切板"))
                    {
                        std::string copyText = "Link type: " + item.type + "\nLink Path: " + item.linkPath + "\nTarget Path: " + item.targetPath;
                        ImGui::SetClipboardText(copyText.c_str());
                    }

                    ImGui::EndChild();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("关于与帮助 (About / Help)"))
            {
                ImGui::Spacing();
                ImGui::TextUnformatted("系统帮助与原理科普");
                ImGui::TextDisabled("了解 Windows 重定向背后的深层文件系统特性，科学选择硬链接、符号链接和目录联接。");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();

                auto drawFAQCard = [&](const char* title, const char* label, const char* syntax, const char* detail, const char* warning) {
                    ImGui::BeginChild(title, ImVec2(-1, 190.0f * mainScale), true, 0);
                    ImGui::TextColored(g_LightTheme ? ImVec4(0.00f, 0.47f, 0.83f, 1.00f) : ImVec4(0.38f, 0.80f, 1.00f, 1.00f), "%s", title);
                    ImGui::TextDisabled("%s", label);
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::Text("经典语法: %s", syntax);
                    ImGui::Spacing();
                    ImGui::TextWrapped("功能阐述: %s", detail);
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.00f), "风险注意: %s", warning);
                    ImGui::EndChild();
                    ImGui::Spacing();
                    ImGui::Spacing();
                };

                drawFAQCard(
                    "目录符号链接 (Directory Symbolic Link) [/D]",
                    "【推荐日常场景选用】适用于各种文件夹路径重定向、跨硬盘软件移动迁移等情况",
                    "mklink /D  <链接名称>  <实际目标真实文件夹>",
                    "生成一个轻量级的绝对/相对指针，在系统底层它是一个真正的符号对象。Windows 绝大多数软件读取它时会完美透明将其当成普通物理文件夹。在 C++ 或各种游戏运行时兼容性最好。",
                    "移除该快捷链接**绝不会**对目标文件夹内的数据造成任何损害。然而，如果在链接文件夹内部直接进行文件编辑修改/删除等具体文件级交互，它将会真实修改或删除物理源文件夹内所对应的原文件！"
                );

                drawFAQCard(
                    "文件硬链接 (File Hard Link) [/H]",
                    "【仅适用于独立物理文件】只能用于单文件共享，绝不能用于整个文件夹",
                    "mklink /H  <链接文件名字>  <实际目标源文件路径>",
                    "在当前 NT 文件系统的 MFT 中为指定磁盘扇区上的同一个物理数据登记一个新的名字。换言之，该文件的两个不同名字都直接指代同一份真实的存储扇区。物理上只占用一份空间，修改其中一方实时双向反馈。",
                    "硬链接**严禁跨硬盘分区**创建。此外，删除硬链接本身非常安全，不会导致数据丢弃；只有当一个文件的全部硬链接引用计数（包括最原始的名字）被全部彻底卸载后，它的物理空间才会被磁盘宣告释放。"
                );

                drawFAQCard(
                    "目录联接 (Directory Junction) [/J]",
                    "【特定分区及老旧软件场景】主要用于本地不同物理磁盘驱动器之间文件夹的快速硬重定向",
                    "mklink /J  <链接联接名字>  <实际目标物理文件夹>",
                    "这是一种 Windows 2000 时代便出现的极低级别路径桥接手段，只能用于本地文件夹路径。在现代 Windows 平台上基本上由目录符号链接 `/D` 所取代，但作为不需要管理员权限就可以建立的本地跨区管道，特定场合依然有用。",
                    "目录联接**不支持任何网络共享或映射网络驱动器重定向**。另外在极低级文件清理工具操作该联接本身时，需提防由于低级系统库的漏洞导致把真实目标里的内容也清空的情况。"
                );
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End(); // End Main Window

        ImGui::Render();
        float clearColor[4];
        if (g_LightTheme)
        {
            clearColor[0] = 0.95f; clearColor[1] = 0.95f; clearColor[2] = 0.95f; clearColor[3] = 1.0f;
        }
        else
        {
            clearColor[0] = 0.10f; clearColor[1] = 0.10f; clearColor[2] = 0.10f; clearColor[3] = 1.0f;
        }
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    // Cleanly uninitialize COM library
    if (SUCCEEDED(hrCo))
        ::CoUninitialize();

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (!g_pSwapChain || !g_pd3dDevice)
        return;

    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr) || pBackBuffer == nullptr)
        return;

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        g_mainRenderTargetView = nullptr;
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
