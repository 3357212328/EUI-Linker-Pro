# Fluent Linker Pro (Qt 6 Edition)

Windows NTFS 符号链接、目录联接与迁移工作台（C++20 原生引擎 + Qt 6.8 Quick / QML 现代流畅界面）。

> 说明：当前 `qt` 分支（默认主分支）采用 **Qt 6.8.3 Quick (QML + `QtQuick.Effects` + `QtQuick.Shapes`)** 重构桌面前端；`slint` 分支保留了 Slint v1.18.1 版本。

## 核心功能

- **创建链接 (`/D` / `/J` / `/H` / `FILE`)**：提供“仅创建链接”与“搬家并链接（先迁移后建立透明链接，支持失败自动回滚）”双工作流，实时生成 `mklink` 预览命令并附带磁盘/权限实时诊断与一键修复。
- **链接检测与安全拆解**：精准识别符号链接（含相对路径解析与断链标红）、目录联接（Junction）、同卷多硬链接（列出所有姐妹路径），支持可调深度遍历扫描与仅移除重解析点/硬链接而不伤及真实数据的“安全拆解”。
- **操作历史与一键回填**：自动记录本地操作历史，支持类型筛选、关键词过滤与一键回填至新建表单。
- **权限中心与深浅色主题**：即时检测管理员令牌与 Windows 开发者模式，支持一键请求提权或跳转系统开发者设置，内置顺滑渐变深/浅双主题与 Win32 原生标题栏联动。

## 动画与视觉设计

- 侧边栏 `SpringAnimation` 弹性高亮滑块 + 平滑淡入位移动画切页
- 分段模式条滑动胶囊与卡片悬停弹性放大 / GPU `MultiEffect` 阴影提升
- 一键互换路径 180° 旋转图标、异步执行环形进度加载器、弹性确认弹窗与带倒计时进度条的浮动通知条

## 本地构建 (Windows + MSVC + Qt 6.8.3)

```powershell
# 确保已安装 Visual Studio 2022/2026 C++ 桌面工具链 与 Qt 6.8.3 (win64_msvc2022_64)
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="D:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release --parallel
```

编译完成后，`CMakeLists.txt` 会自动调用 `windeployqt` 将运行所需 Qt DLL 与 QML 模块部署到 `build/Release/` 目录，可直接运行 `build\Release\fluent_linker.exe`。
