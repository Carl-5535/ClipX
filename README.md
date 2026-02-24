# ClipX

> 超轻量级、本地持久化、无条数限制的 Windows 剪贴板增强工具

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

- **剪贴板历史管理** - 自动记录所有复制内容
- **全文搜索** - 支持中英文搜索，实时过滤
- **标签分类** - 为剪贴板条目添加标签，永久保存
- **收藏功能** - 标记常用内容，快速访问
- **现代UI** - 半透明磨砂玻璃质感，深色主题
- **轻量级** - 内存占用 < 10MB，冷启动 < 500ms
- **本地存储** - 数据完全本地化，无网络依赖

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

执行后会自动生成 `ClipX-v1.0.0-Win64.zip` 发布包。

## 构建发布包

### 快速打包

```powershell
.\build_and_zip.ps1
```

这会生成 `ClipX-v1.0.0-Win64.zip`，包含：
- `ClipX.exe`
- `Overlay.exe`
- `README.md` (用户文档)

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
```

**问题：资源文件编译失败**
```
解决方案：确保安装了 Windows SDK
```

### 运行时错误

**问题：Overlay 启动失败**
```
检查：Overlay.exe 是否与 ClipX.exe 在同一目录
```

**问题：热键不生效**
```
检查：F9 是否被其他程序占用
```

## 版本发布

### 发布流程

1. 更新版本号（CMakeLists.txt）
2. 更新 CHANGELOG.md
3. 运行 `build_and_zip.ps1`
4. 测试生成的 ZIP
5. 创建 Git Tag
6. 上传到 Release

### 版本命名

```
v主版本.次版本.修订号
例如：v1.0.0
```

## 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 许可证

MIT License

## 联系方式

- Issues: [GitHub Issues](https://github.com/your-repo/ClipX/issues)
- Discussions: [GitHub Discussions](https://github.com/your-repo/ClipX/discussions)
