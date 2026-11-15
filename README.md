# Fluent Linker Pro

基于 **Slint (`v1.18.1`, Fluent + Skia 渲染后端)** 与现代 **C++20** 构建的 Windows NTFS 符号链接、目录联接与目录搬家桌面工具（`mklink` 图形化工作台）。

## 核心功能

- **新建与目录搬家工作台 (`Create & Move`)**
  - 完整支持 4 种 Windows 链接机制：**目录联接 (`/J`)**、**目录符号链接 (`/D`)**、**文件硬链接 (`/H`)**、**文件符号链接**。
  - **安全搬家并建链 (`Move & Link`)**：先通过 Windows `robocopy` / `IFileOperation` 将大体积文件夹安全迁移到目标分区，再在原路径无缝建立重定向链接，解决 C 盘空间紧张问题。
  - **实时合规预检与一键修复**：自动检测目标存在性、类型匹配、同分区限制 (`/H`)、NTFS 支持与管理员/开发者模式权限；自动剥离两端引号；支持路径互换、一键补全同名后缀与自动创建缺失父级目录。
  - **异步执行不卡顿**：后台线程执行搬家与建链，保留完整命令行输出，完成后支持一键在资源管理器中定位或转入体检页。
- **体检与扫描工具 (`Inspect & Unlink`)**
  - 底层解析 Windows 重解析点 (`FSCTL_GET_REPARSE_POINT`) 与硬链接兄弟入口 (`FindFirstFileNameW`)。
  - 支持单路径深度剖析及可取消的多层级异步目录扫描，一眼识别悬空/失效链接。
  - **安全解除链接 (`Safe Unlink`)**：通过 `RemoveDirectoryW` / `DeleteFileW` 仅移除链接入口本身，绝不删除指向目标的真实数据。
- **执行记录中心 (`History`)**
  - 自动持久化至 `%LOCALAPPDATA%\FluentLinker\mklink_history.tsv`（自动兼容迁移旧版记录）。
  - 支持分类筛选、关键词过滤、一键定位、跳转体检及回填参数重用。
- **环境与指南助手 (`Privileges & Guide`)**
  - 实时检测 UAC 管理员令牌与 Windows 开发者模式（Developer Mode），支持一键跳转系统设置或提权重启，附带 4 种链接方式对比矩阵。

## 目录结构

```text
EUI-Linker-Pro/
├── .github/workflows/     # Windows x64 Release CI 工作流
├── assets/                # 应用图标 (icon.ico / icon.png)
├── src/
│   ├── main.cpp           # 程序入口与 DPI 初始化
│   ├── app_controller.h   # Slint 事件绑定、异步调度与主题联动
│   ├── linker_types.h     # 核心领域数据结构定义
│   ├── win_linker.h       # Win32 NTFS / mklink / 重解析点 / 历史与设置持久化
│   └── ui/                # Slint (.slint) Fluent UI 声明式界面
│       ├── app.slint
│       ├── theme.slint
│       ├── create_page.slint
│       ├── inspect_page.slint
│       ├── history_page.slint
│       └── guide_page.slint
└── CMakeLists.txt
```

## 编译与运行（Visual Studio + Rust + CMake）

1. 环境准备：
   - **Visual Studio 2022 / 2026**（含「使用 C++ 的桌面开发」与 Windows SDK）
   - **Rust 工具链**（`rustc >= 1.92` + `cargo`，用于在 CMake 构建阶段编译 Slint 运行时与编译器）
2. 在项目根目录打开 VS 开发者命令行并执行配置与 Release 编译：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

3. 运行生成的程序（确保 `fluent_linker.exe` 与同目录的 `slint_cpp.dll` 在一起）：

```powershell
.\build\Release\fluent_linker.exe
```

## 开源许可

本项目遵循仓库根目录 [LICENSE](LICENSE) 中的许可协议。
