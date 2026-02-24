# ClipX

> 超轻量级、本地持久化、无条数限制的 Windows 剪贴板增强工具

[![Release](https://img.shields.io/badge/release-V0.0.1-blue)](https://github.com/Carl-5535/ClipX/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

## 项目概述

ClipX 是一个 Windows 剪贴板管理工具，旨在解决 Windows 原生剪贴板（Win+V）的以下问题：

| 问题 | ClipX 解决方案 |
|------|---------------|
| 历史条数限制（默认25条） | 无条数限制，海量存储 |
| 重启后历史丢失 | 本地 SQLite 持久化存储 |
| 无搜索功能 | 全文搜索 + 标签筛选 |
| 无扩展能力 | 标签分类、收藏功能 |
| 无法深度定制 | 可配置热键、存储策略 |

### 功能特性

- **剪贴板历史管理** - 自动记录所有复制内容（文本、HTML、图像、文件）
- **全文搜索** - 支持中英文搜索，实时过滤
- **标签分类** - 为剪贴板条目添加标签，永久保存
- **收藏功能** - 标记常用内容，快速访问
- **现代UI** - 半透明磨砂玻璃质感，深色主题
- **轻量级** - 内存占用 < 10MB，冷启动 < 500ms
- **本地存储** - 数据完全本地化，无网络依赖
- **快捷键支持** - F9 唤出界面，上下键选择，Enter 确认，Del 删除

### 系统架构

```
ClipX/
├── ClipX.exe        # 主程序（后台守护进程）
├── Overlay.exe      # UI 浮层程序（按需启动）
├── clipx.db         # SQLite 数据库
├── config.json      # 配置文件
└── logs/            # 日志目录
```

### 技术栈

| 技术 | 版本 | 用途 |
|------|------|------|
| C++ | C++17 | 核心语言 |
| Win32 API | - | 系统调用 |
| GDI/GDI+ | - | 2D 渲染 |
| SQLite | 3.x | 数据存储 |
| Named Pipe | - | 进程间通信 |
| CMake | 3.16+ | 构建系统 |
| vcpkg | - | 依赖管理 |

## 项目结构

```
ClipX/
├── CMakeLists.txt           # 主 CMake 配置
├── build_and_zip.ps1        # 构建打包脚本
├── build_release.bat        # 构建脚本
├── README.md                # 项目说明（本文件）
├── README_USER.md           # 用户文档（打包时使用）
├── claude.md                # 项目指令
├── docs/
│   └── design.md            # 详细设计文档
├── cmake/
│   └── FindSQLite3.cmake    # SQLite 查找脚本
├── src/
│   ├── CMakeLists.txt
│   ├── Common/              # 共享代码库
│   │   ├── include/common/
│   │   │   ├── logger.h     # 日志系统
│   │   │   ├── config.h     # 配置管理
│   │   │   ├── utils.h      # 工具函数
│   │   │   ├── types.h      # 类型定义
│   │   │   └── ipc_protocol.h # IPC 协议
│   │   └── src/
│   ├── ClipD/               # 后台守护进程
│   │   ├── include/
│   │   │   ├── clipboard_listener.h
│   │   │   ├── data_manager.h
│   │   │   ├── ipc_server.h
│   │   │   ├── hotkey_manager.h
│   │   │   ├── tray_icon.h
│   │   │   └── auto_start.h
│   │   ├── src/
│   │   ├── resources/
│   │   │   ├── icon.ico     # 应用图标
│   │   │   └── ClipD.rc     # 资源文件
│   │   └── CMakeLists.txt
│   └── Overlay/             # UI 浮层程序
│       ├── include/
│       │   ├── ipc_client.h
│       │   ├── overlay_window.h
│       │   └── renderer.h
│       ├── src/
│       ├── resources/
│       │   ├── icon.ico
│       │   └── Overlay.rc
│       └── CMakeLists.txt
├── tests/                   # 测试代码
│   └── CMakeLists.txt
└── third_party/             # 第三方库
    └── json/                # nlohmann/json (header-only)
```

## 开发环境

### 环境要求

| 工具 | 最低版本 | 推荐版本 |
|------|----------|----------|
| OS | Windows 10 | Windows 11 |
| 编译器 | Visual Studio 2019 | Visual Studio 2022 |
| CMake | 3.16 | 3.25+ |
| vcpkg | 最新 | 最新 |
| Git | 2.0+ | 最新 |

### 安装 vcpkg

```bash
# 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# 初始化
./bootstrap-vcpkg.bat

# 集成到 Visual Studio
./vcpkg integrate install

# 安装依赖
./vcpkg install sqlite3:x64-windows
```

## 编译方法

### 方式一：Visual Studio

```bash
# 1. 生成项目文件
cmake -B build -S . -A x64 -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

# 2. 打开 build/ClipX.sln
# 3. 选择 Release 配置
# 4. 生成解决方案
```

### 方式二：命令行

```bash
# 1. 配置
cmake -B build -S . -A x64 -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

# 2. 编译
cmake --build build --config Release

# 3. 输出位置
# build/bin/Release/ClipX.exe
# build/bin/Release/Overlay.exe
```

### 方式三：使用脚本（推荐）

```powershell
.\build_and_zip.ps1
```

执行后会自动生成 `ClipX-V{version}-Win64.zip` 发布包（版本号自动从 git tag 获取）。

## 构建发布包

### 快速打包

```powershell
# 1. 创建 git tag
git tag V0.0.1

# 2. 运行构建脚本
.\build_and_zip.ps1
```

这会生成 `ClipX-V0.0.1-Win64.zip`，包含：
- `ClipX.exe` - 主程序（后台守护进程）
- `Overlay.exe` - UI 浮层程序
- `sqlite3.dll` - SQLite 数据库 DLL
- `README.md` - 用户文档

### 手动打包

```bash
# 1. 编译 Release 版本
cmake --build build --config Release

# 2. 创建打包目录
mkdir package
copy build\bin\Release\ClipX.exe package\
copy build\bin\Release\Overlay.exe package\
copy README_USER.md package\README.md

# 3. ZIP 压缩
# 使用 7-Zip 或 PowerShell Compress-Archive
```

## 依赖说明

### 第三方库

| 库 | 版本 | 用途 | 来源 |
|------|------|------|------|
| nlohmann/json | 3.11+ | JSON 解析 | header-only (已包含) |
| SQLite3 | 3.40+ | 数据库 | vcpkg |

### vcpkg 依赖

```bash
vcpkg install sqlite3:x64-windows
```

## 开发指南

### 添加新功能

1. 在对应模块的 `include/` 目录添加头文件
2. 在 `src/` 目录添加实现
3. 更新 `CMakeLists.txt` 添加新源文件
4. 更新 IPC 协议（如需要）

### 调试

```bash
# Debug 构建
cmake --build build --config Debug

# 输出位置
build/bin/Debug/ClipX.exe
build/bin/Debug/Overlay.exe

# 日志位置
%APPDATA%\ClipX\logs\clipx.log
```

### 代码风格

- 使用 C++17 标准
- 4 空格缩进
- 命名空间：`clipx`
- 类名：PascalCase
- 函数名：PascalCase
- 成员变量：m_ 前缀 + camelCase

## 测试

```bash
# 运行测试
cd build
ctest -C Release

# 或直接运行测试可执行文件
.\bin\Release\tests.exe
```

## 故障排除

### 编译错误

**问题：找不到 SQLite3**
```
解决方案：确保安装 vcpkg 并设置 CMAKE_TOOLCHAIN_FILE
vcpkg install sqlite3:x64-windows
```

**问题：资源文件编译失败**
```
解决方案：确保安装了 Windows SDK
```

### 运行时错误

**问题：程序启动失败（缺少 DLL）**
```
解决方案：确保 sqlite3.dll 与程序在同一目录
```

**问题：中文显示为空白**
```
解决方案：确保剪贴板数据编码正确（UTF-8）
```

**问题：右键删除没有反应**
```
解决方案：确保已更新到 V0.0.1 或更高版本
```

**问题：热键不生效**
```
解决方案：检查 F9 是否被其他程序占用
```

## 使用说明

### 快捷键

| 快捷键 | 功能 |
|--------|------|
| F9 | 打开剪贴板历史窗口 |
| ↑ / ↓ | 上下选择条目 |
| Enter | 粘贴选中条目 |
| Esc | 关闭窗口 |
| Del | 删除选中条目 |
| Ctrl+F | 聚焦搜索框 |

### 右键菜单

- **Add Tag** - 为条目添加标签（带标签的条目会永久保存）
- **View Tags** - 查看条目的所有标签
- **Delete** - 删除条目

### 数据存储

- **内存条目**：未添加标签的条目仅存储在内存中，重启后清空
- **持久化条目**：添加标签的条目会保存到数据库，永久保留
- **自动清理**：可配置天数自动清理旧条目（保留收藏）

### 配置文件

配置文件位于 `%APPDATA%\ClipX\config.json`：

```json
{
  "behavior": {
    "deduplicate": true,
    "paste_after_select": false,
    "auto_start": false
  },
  "storage": {
    "auto_cleanup_days": 30
  },
  "advanced": {
    "db_file": "clipx.db",
    "log_level": "info"
  }
}
```

## 版本发布

### 发布流程

1. 更新代码和测试
2. 提交所有更改 (`git commit`)
3. 创建 Git Tag (`git tag V1.0.0`)
4. 运行 `build_and_zip.ps1` 生成发布包
5. 测试生成的 ZIP
6. 推送到远程 (`git push origin master --tags`)
7. 上传 ZIP 到 GitHub Release

### 版本命名

```
V主版本.次版本.修订号
例如：V0.0.1, V1.0.0
```

### 当前版本

- **V0.0.1** - 初始版本
  - 剪贴板历史管理
  - 全文搜索（支持中文）
  - 标签分类和收藏
  - 现代化 UI 设计
  - 右键菜单和快捷键支持

## 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 许可证

MIT License

## 联系方式

- Issues: [GitHub Issues](https://github.com/Carl-5535/ClipX/issues)
- Discussions: [GitHub Discussions](https://github.com/Carl-5535/ClipX/discussions)
