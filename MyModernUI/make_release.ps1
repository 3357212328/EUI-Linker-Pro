$dist = 'C:\Users\33572\Desktop\FluentLinker_v1.0_Release'
New-Item -ItemType Directory -Path $dist -Force | Out-Null

Copy-Item 'C:\Users\33572\Desktop\MyModernUI\x64\Release\MyModernUI.exe' (Join-Path $dist 'FluentLinker.exe')

$readme = @"
# Fluent Linker v1.0
Windows 链接管理器 - mklink 图形化界面工具

## 功能
- 创建 Windows 软链接 (Symbolic Link /D)
- 创建文件硬链接 (Hard Link /H)
- 创建目录联接 (Junction /J)
- 历史记录管理
- 浅色 / 深色主题切换

## 系统要求
- Windows 10 / 11 (x64)
- 部分链接类型需要管理员权限

## 使用方法
直接运行 FluentLinker.exe 即可。
如需创建系统级链接，请右键 -> 以管理员身份运行。

## 技术栈
- Dear ImGui + DirectX 11
- C++20 / MSVC v145
"@
Set-Content (Join-Path $dist 'README.txt') $readme -Encoding UTF8

Write-Host "Release package created at: $dist"
Get-ChildItem $dist | Format-Table Name, Length -AutoSize
