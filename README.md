# Fluent Linker Pro

基于 **EUI-NEO (`v0.6.3`)** 与现代 C++17 构建的 Windows NTFS 符号链接、目录联接与目录搬家桌面工具（`mklink` 图形化工作台）。

## 核心功能

- **创建与目录搬家工作台**
  - 完整支持 4 种 Windows 链接机制：**目录联接 (`/J`)**、**目录符号链接 (`/D`)**、**文件硬链接 (`/H`)**、**文件符号链接**。
  - **目录搬家并建链 (`Move & Link`)**：先通过 Windows `IFileOperation` 将大体积文件夹安全迁移到目标分区，再在原路径无缝建立重定向链接，解决 C 盘空间紧张问题。
  - **智能预检与一键修复**：实时检查目标是否存在、文件/目录类型匹配、同分区限制 (`/H`)、NTFS 支持与管理员/开发者模式权限；粘贴路径自动剥离两端双引号；一键追加目录名、路径互换、自动创建缺失父级目录。
  - **异步命令执行**：后台进程执行 `mklink` 并实时回显原始输出，不阻塞界面渲染。
- **链接体检与安全解除工具**
  - 底层读取 Windows 重解析点 (`FSCTL_GET_REPARSE_POINT`) 与硬链接兄弟入口 (`FindFirstFileNameW`)。
  - 支持单路径精准剖析及**多层级异步目录扫描**，快速识别悬空/失效链接。
  - **安全解除链接 (`Safe Unlink`)**：基于 `RemoveDirectoryW` / `DeleteFileW` 仅移除链接入口本身，绝不删除指向目录的真实文件。
- **执行历史与详情中心**
  - 自动持久化至 `%LOCALAPPDATA%\FluentLinker\mklink_history.tsv`（首次运行自动迁移旧版本历史记录）。
  - 支持分类筛选、大小写不敏感关键词搜索、点击查看详情、快速定位链接或目标所在资源管理器位置，以及一键回填参数重用。
- **原理指南与环境权限助手**
  - 实时检测管理员身份与 Windows 开发者模式（Developer Mode），支持一键跳转系统开发者设置或 UAC 提权重启。

## 目录结构

```text
EUI-Linker-Pro/
├── 3rd/EUI-NEO/           # EUI-NEO v0.6.3 渲染与组件框架
├── assets/                # 应用图标 (icon.ico / icon.png)
├── src/
│   ├── main.cpp           # 应用入口、顶栏导航、全局弹窗与主题系统
│   ├── app_actions.h      # 异步任务调度与状态控制
│   ├── linker_types.h     # 核心数据结构定义
│   ├── ui_theme.h         # Fluent 深色/浅色主题与可复用组件
│   ├── win_linker.h       # Win32 mklink / 重解析点 / IFileOperation / 历史持久化
│   └── pages/             # 创建与搬家 / 链接体检 / 执行历史 / 原理指南 页面
└── CMakeLists.txt
```

## 编译与运行（Visual Studio / CMake）

1. 确保已安装 **Visual Studio**（包含「使用 C++ 的桌面开发」与 Windows SDK）。
2. 在项目根目录执行配置与 Release 编译：

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
```

3. 运行生成的程序：

```powershell
.\build\Release\fluent_linker.exe
```

## 开源许可

本项目遵循仓库根目录 [LICENSE](LICENSE) 中的许可协议。
