# ClipX

> 超轻量级、本地持久化、无条数限制的 Windows 剪贴板增强工具

## 项目介绍

ClipX 是一个 Windows 剪贴板管理工具，旨在解决 Windows 原生剪贴板（Win+V）的以下问题：

- **历史条数限制** - 无条数限制，支持存储海量剪贴板历史
- **重启后历史丢失** - 所有数据持久化到本地 SQLite 数据库
- **不支持搜索** - 支持全文搜索和标签搜索
- **不支持扩展** - 支持标签分类功能
- **无法深度定制** - 可配置热键、存储策略等

### 功能特性

- **剪贴板历史管理** - 自动记录所有复制内容
- **全文搜索** - 支持中英文搜索，支持标签搜索
- **标签分类** - 为剪贴板条目添加标签，快速分类筛选
- **收藏功能** - 标记常用内容，快速访问
- **现代UI** - 半透明磨砂玻璃质感，深色主题
- **轻量级** - 内存占用 < 10MB，冷启动 < 500ms
- **本地存储** - 数据完全本地化，无需网络

### 系统架构

```
ClipX/
├── ClipD.exe       # 后台守护进程（常驻）
├── Overlay.exe     # UI 浮层程序（按需启动）
├── clipx.db        # SQLite 数据库
├── config.json     # 配置文件
└── logs/           # 日志目录
```

### 技术栈

- **语言**: C++17
- **UI 框架**: Win32 API + GDI
- **数据库**: SQLite 3
- **IPC**: Windows Named Pipe
- **构建**: CMake

## 编译方法

### 环境要求

- Windows 10/11
- Visual Studio 2019 或更高版本（支持 C++17）
- CMake 3.15+

### 编译步骤

```bash
# 1. 克隆项目
git clone https://github.com/your-repo/ClipX.git
cd ClipX

# 2. 创建构建目录
mkdir build && cd build

# 3. 生成项目文件
cmake .. -A x64

# 4. 编译
cmake --build . --config Release

# 5. 输出文件位于 build/bin/Release/ 目录
```

### 依赖项

项目使用以下第三方库（已包含在 `third_party/` 目录）：

- **SQLite 3** - 数据库引擎
- **nlohmann/json** - JSON 解析库

## 使用方法

### 启动程序

1. 运行 `ClipD.exe` 启动后台守护进程
2. 程序会自动最小化到系统托盘
3. 按 `F9` 键呼出剪贴板历史浮层

### 基本操作

| 快捷键 | 功能 |
|--------|------|
| `F9` | 显示/隐藏剪贴板历史浮层 |
| `↑` / `↓` | 上下导航 |
| `Enter` | 选中并粘贴到剪贴板 |
| `Escape` | 关闭浮层 |
| `Delete` | 删除选中条目 |
| `右键` | 显示上下文菜单（添加标签、删除等） |

### 搜索功能

- 在搜索框中输入关键词进行全文搜索
- 支持中英文搜索
- 支持通过标签名称搜索

### 标签功能

- 右键点击条目 → "Add Tag" 添加标签
- 标签面板显示所有标签及其使用次数
- 点击标签可快速筛选相关条目

### 系统托盘

- **双击托盘图标**: 显示剪贴板历史
- **右键托盘图标**: 显示菜单
  - Show: 显示浮层
  - Auto Start: 开机自启动
  - About: 关于信息
  - Exit: 退出程序

### 配置文件

配置文件位于 `%APPDATA%\ClipX\config.json`，可配置：

- 热键设置
- 存储策略（自动清理天数等）
- UI 设置（窗口大小、透明度等）
- 行为设置（去重、粘贴后自动关闭等）

## 许可证

本项目采用 MIT 许可证开源。

```
MIT License

Copyright (c) 2024 ClipX

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
