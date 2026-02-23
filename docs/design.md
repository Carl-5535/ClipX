# ClipX 设计文档

版本: 1.0
更新日期: 2026-02-22

---

## 目录

1. [概述](#1-概述)
2. [设计原则](#2-设计原则)
3. [系统架构](#3-系统架构)
4. [进程模型](#4-进程模型)
5. [核心模块设计](#5-核心模块设计)
6. [数据库设计](#6-数据库设计)
7. [IPC 通信协议](#7-ipc-通信协议)
8. [配置系统](#8-配置系统)
9. [UI/UX 设计](#9-uiux-设计)
10. [热键系统](#10-热键系统)
11. [支持的剪贴板格式](#11-支持的剪贴板格式)
12. [搜索功能](#12-搜索功能)
13. [错误处理](#13-错误处理)
14. [性能优化](#14-性能优化)
15. [扩展机制](#15-扩展机制)

---

## 1. 概述

### 1.1 背景

Windows 原生剪贴板历史记录功能（Win+V）存在以下局限：

| 问题 | 影响 |
|------|------|
| 历史条数限制（默认25条） | 无法保留长期工作记录 |
| 重启后历史丢失 | 重要内容需要手动备份 |
| 无搜索功能 | 查找历史内容效率低 |
| 无扩展能力 | 无法自定义功能 |
| 有限的数据类型 | 仅支持文本、HTML、图片 |

### 1.2 目标

开发一个**超轻量级、本地持久化、无条数限制、风格接近 Win+V** 的剪贴板增强工具。

### 1.3 非目标

- 不做云同步
- 不做加密存储
- 不做跨平台
- 不做复杂的权限管理

---

## 2. 设计原则

| 原则 | 说明 |
|------|------|
| 极简架构 | 双进程模型，最小依赖 |
| 低内存占用 | 目标 < 10MB 常驻内存 |
| 快速启动 | 冷启动 < 500ms |
| 本地优先 | 无网络依赖 |
| 可扩展 | 预留插件接口 |

---

## 3. 系统架构

### 3.1 组件概览

```
┌─────────────────────────────────────────────────────────┐
│                      ClipX 系统                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │              ClipD.exe (后台守护进程)             │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │   │
│  │  │ Clipboard   │  │ DataManager │  │   IPC    │ │   │
│  │  │ Listener    │──▶│ (SQLite)    │──▶│  Server  │ │   │
│  │  └─────────────┘  └─────────────┘  └──────────┘ │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          │ Named Pipe                   │
│                          ▼                              │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Overlay.exe (UI 浮层程序)            │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │   │
│  │  │    IPC      │  │  ViewModel  │  │    UI    │ │   │
│  │  │   Client    │──▶│   Manager   │──▶│ Renderer │ │   │
│  │  └─────────────┘  └─────────────┘  └──────────┘ │   │
│  └─────────────────────────────────────────────────┘   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 3.2 文件结构

```
ClipX/
├── ClipD.exe                 # 后台守护进程
├── Overlay.exe               # UI 浮层程序
├── clipx.db                  # SQLite 数据库
├── config.json               # 配置文件
└── logs/
    └── clipx.log             # 日志文件
```

---

## 4. 进程模型

### 4.1 双进程架构

| 进程 | 职责 | 生命周期 |
|------|------|----------|
| ClipD.exe | 监听剪贴板、数据存储、IPC 服务 | 用户登录后常驻运行 |
| Overlay.exe | UI 展示、用户交互 | 按需启动，使用完毕后退出 |

### 4.2 进程通信

- **通信方式**: Windows Named Pipe
- **管道名称**: `\\.\pipe\ClipX_IPC`
- **通信模式**: 请求-响应（同步）+ 事件通知（异步）

### 4.3 进程生命周期

```
┌─────────────────────────────────────────────────────────┐
│                     ClipD.exe                           │
├─────────────────────────────────────────────────────────┤
│  启动 ──▶ 初始化 ──▶ 监听剪贴板 ──▶ 等待 IPC 请求        │
│                          │                              │
│                          ▼                              │
│                    [收到退出信号] ──▶ 清理 ──▶ 退出      │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                    Overlay.exe                          │
├─────────────────────────────────────────────────────────┤
│  热键触发 ──▶ 启动 ──▶ 连接 IPC ──▶ 获取数据 ──▶ 渲染 UI │
│                                               │         │
│                                               ▼         │
│               [用户选择/取消] ──▶ 写入剪贴板 ──▶ 退出    │
└─────────────────────────────────────────────────────────┘
```

---

## 5. 核心模块设计

### 5.1 ClipboardListener（剪贴板监听器）

**职责**: 监听系统剪贴板变化，读取内容并发送给 DataManager 存储。

**技术实现**:

```cpp
// 使用 Win32 API 监听剪贴板
class ClipboardListener {
public:
    void Start();
    void Stop();

private:
    HWND m_hwnd;
    std::function<void(ClipboardData)> m_onClipboardChange;

    // Win32 消息处理
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// 注册监听
AddClipboardFormatListener(hwnd);

// 消息处理
case WM_CLIPBOARDUPDATE:
    // 读取剪贴板内容
    // 调用 DataManager 存储
    break;
```

**数据流**:

```
Windows Clipboard
       │
       │ WM_CLIPBOARDUPDATE
       ▼
ClipboardListener
       │
       │ ClipboardData
       ▼
  DataManager
       │
       │ SQL INSERT
       ▼
   SQLite DB
```

### 5.2 DataManager（数据管理器）

**职责**: 管理剪贴板历史的存储、查询、删除。

**核心接口**:

```cpp
class DataManager {
public:
    // 初始化数据库连接
    bool Initialize(const std::string& dbPath);

    // 存储剪贴板数据
    int64_t Insert(const ClipboardData& data);

    // 查询历史记录
    std::vector<ClipboardEntry> Query(const QueryOptions& options);

    // 搜索
    std::vector<ClipboardEntry> Search(const std::string& keyword);

    // 删除
    bool Delete(int64_t id);
    bool DeleteOlderThan(int64_t timestamp);
    bool DeleteAll();

    // 获取统计信息
    size_t GetTotalCount();
    size_t GetDatabaseSize();

private:
    sqlite3* m_db;
    std::mutex m_mutex;
};

struct ClipboardEntry {
    int64_t id;
    int64_t timestamp;
    ClipboardDataType type;
    std::vector<uint8_t> data;
    std::string preview;      // 用于显示的预览文本
    std::string sourceApp;    // 来源应用（可选）
    int32_t copyCount;        // 复制次数（用于智能排序）
};

struct QueryOptions {
    int limit = 100;
    int offset = 0;
    ClipboardDataType filterType = ClipboardDataType::All;
    SortOrder sortOrder = SortOrder::LatestFirst;
};
```

### 5.3 IPC Server（ClipD 端）

**职责**: 接收 Overlay 的请求，返回数据或执行操作。

**实现**:

```cpp
class IPCServer {
public:
    bool Start(const std::string& pipeName);
    void Stop();
    void SetHandler(std::function<IPCResponse(IPCRequest)> handler);

private:
    HANDLE m_pipe;
    std::thread m_listenerThread;
    bool m_running;

    void ListenLoop();
    IPCResponse ProcessRequest(const IPCRequest& request);
};
```

### 5.4 IPC Client（Overlay 端）

**职责**: 向 ClipD 发送请求并接收响应。

**实现**:

```cpp
class IPCClient {
public:
    bool Connect(const std::string& pipeName, int timeoutMs = 5000);
    void Disconnect();
    IPCResponse SendRequest(const IPCRequest& request);

    // 异步通知订阅
    void Subscribe(const std::string& eventType,
                   std::function<void(IPCNotification)> callback);

private:
    HANDLE m_pipe;
    bool m_connected;
};
```

### 5.5 HotkeyManager（热键管理器）

**职责**: 注册和管理全局热键。

**实现**:

```cpp
class HotkeyManager {
public:
    bool RegisterHotkey(int id, UINT modifiers, UINT vk);
    bool UnregisterHotkey(int id);
    void SetHandler(int id, std::function<void()> handler);

private:
    HWND m_hwnd;
    std::unordered_map<int, std::function<void()>> m_handlers;
};

// 热键消息处理
case WM_HOTKEY:
    if (auto it = m_handlers.find(wParam); it != m_handlers.end()) {
        it->second();
    }
    break;
```

---

## 6. 数据库设计

### 6.1 表结构

```sql
-- 剪贴板历史主表
CREATE TABLE clipboard_entries (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp       INTEGER NOT NULL,           -- Unix 时间戳（毫秒）
    type            INTEGER NOT NULL,           -- 数据类型枚举
    data            BLOB NOT NULL,              -- 原始数据
    preview         TEXT,                       -- 预览文本（用于显示和搜索）
    source_app      TEXT,                       -- 来源应用名称
    hash            BLOB NOT NULL,              -- 数据哈希（用于去重）
    copy_count      INTEGER DEFAULT 1,          -- 复制次数
    is_favorited    INTEGER DEFAULT 0,          -- 是否收藏
    created_at      INTEGER NOT NULL,
    updated_at      INTEGER NOT NULL
);

-- 索引
CREATE INDEX idx_timestamp ON clipboard_entries(timestamp DESC);
CREATE INDEX idx_type ON clipboard_entries(type);
CREATE INDEX idx_hash ON clipboard_entries(hash);
CREATE INDEX idx_favorited ON clipboard_entries(is_favorited);

-- FTS5 全文搜索虚拟表（用于文本内容搜索）
CREATE VIRTUAL TABLE clipboard_search USING fts5(
    preview,
    content='clipboard_entries',
    content_rowid='id'
);

-- 触发器：保持 FTS 索引同步
CREATE TRIGGER clipboard_ai AFTER INSERT ON clipboard_entries BEGIN
    INSERT INTO clipboard_search(rowid, preview)
    VALUES (new.id, new.preview);
END;

CREATE TRIGGER clipboard_ad AFTER DELETE ON clipboard_entries BEGIN
    INSERT INTO clipboard_search(clipboard_search, rowid, preview)
    VALUES ('delete', old.id, old.preview);
END;

CREATE TRIGGER clipboard_au AFTER UPDATE ON clipboard_entries BEGIN
    INSERT INTO clipboard_search(clipboard_search, rowid, preview)
    VALUES ('delete', old.id, old.preview);
    INSERT INTO clipboard_search(rowid, preview)
    VALUES (new.id, new.preview);
END;
```

### 6.2 数据类型枚举

```cpp
enum class ClipboardDataType : int32_t {
    Text        = 1,    // 纯文本
    Html        = 2,    // HTML 格式
    Rtf         = 3,    // RTF 格式
    Image       = 4,    // 位图图像
    Files       = 5,    // 文件列表
    Custom      = 99    // 自定义格式
};
```

### 6.3 数据去重策略

使用 SHA-256 哈希进行内容去重：

```cpp
std::vector<uint8_t> ComputeHash(const std::vector<uint8_t>& data);

// 插入时检查
if (DataManager::ExistsByHash(hash)) {
    // 更新 copy_count 和 timestamp
    DataManager::UpdateExisting(hash, newTimestamp);
} else {
    // 插入新记录
    DataManager::Insert(data);
}
```

---

## 7. IPC 通信协议

### 7.1 消息格式

所有 IPC 消息使用 JSON 格式：

```cpp
// 请求
struct IPCRequest {
    std::string action;         // 操作类型
    int32_t request_id;         // 请求 ID（用于匹配响应）
    nlohmann::json params;      // 参数
};

// 响应
struct IPCResponse {
    int32_t request_id;         // 对应的请求 ID
    bool success;               // 是否成功
    nlohmann::json data;        // 返回数据
    std::string error;          // 错误信息（失败时）
};

// 异步通知
struct IPCNotification {
    std::string event;          // 事件类型
    nlohmann::json data;        // 事件数据
};
```

### 7.2 支持的操作

| Action | 描述 | 参数 | 返回 |
|--------|------|------|------|
| `ping` | 心跳检测 | - | `{ "pong": true }` |
| `get_history` | 获取历史列表 | `limit`, `offset`, `type` | `ClipboardEntry[]` |
| `search` | 搜索历史 | `keyword`, `limit` | `ClipboardEntry[]` |
| `get_entry` | 获取单条详情 | `id` | `ClipboardEntry` |
| `set_clipboard` | 写入剪贴板 | `id` | `{ "success": true }` |
| `delete_entry` | 删除条目 | `id` | `{ "success": true }` |
| `toggle_favorite` | 切换收藏 | `id` | `{ "success": true }` |
| `get_stats` | 获取统计信息 | - | `{ "count": N, "size": N }` |
| `clear_all` | 清空历史 | - | `{ "success": true }` |

### 7.3 异步通知事件

| Event | 描述 | 数据 |
|-------|------|------|
| `clipboard_changed` | 剪贴板内容变化 | `ClipboardEntry` |
| `entry_deleted` | 条目被删除 | `{ "id": N }` |
| `config_changed` | 配置变更 | `config.json` 内容 |

### 7.4 示例

**请求历史列表**:

```json
{
    "action": "get_history",
    "request_id": 1,
    "params": {
        "limit": 50,
        "offset": 0,
        "type": 0
    }
}
```

**响应**:

```json
{
    "request_id": 1,
    "success": true,
    "data": {
        "entries": [
            {
                "id": 1,
                "timestamp": 1708600000000,
                "type": 1,
                "preview": "Hello World",
                "source_app": "notepad.exe",
                "copy_count": 3,
                "is_favorited": false
            }
        ],
        "total": 100
    }
}
```

---

## 8. 配置系统

### 8.1 配置文件格式 (config.json)

```json
{
    "version": "1.0",
    "hotkey": {
        "show_overlay": {
            "modifiers": ["win", "ctrl"],
            "key": "v"
        }
    },
    "ui": {
        "width": 400,
        "max_height": 600,
        "font_size": 14,
        "theme": "system",
        "opacity": 0.95,
        "show_source_app": true,
        "show_timestamp": true,
        "preview_length": 100
    },
    "storage": {
        "max_entries": 10000,
        "max_data_size_mb": 100,
        "auto_cleanup_days": 30,
        "exclude_types": []
    },
    "behavior": {
        "auto_start": true,
        "close_on_select": true,
        "paste_after_select": true,
        "smart_sort": true,
        "deduplicate": true
    },
    "advanced": {
        "log_level": "info",
        "log_file": "logs/clipx.log",
        "db_file": "clipx.db"
    }
}
```

### 8.2 配置管理

```cpp
class ConfigManager {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path);

    template<typename T>
    T Get(const std::string& key, const T& defaultValue);

    template<typename T>
    void Set(const std::string& key, const T& value);

    void WatchForChanges(std::function<void()> callback);

private:
    nlohmann::json m_config;
    std::string m_path;
    std::mutex m_mutex;
};
```

---

## 9. UI/UX 设计

### 9.1 整体布局

```
┌─────────────────────────────────────────────┐
│  🔍 搜索...                              ✕  │  <- 标题栏（搜索框）
├─────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────┐ │
│ │ ⭐ Hello World                          │ │
│ │    notepad.exe · 2分钟前                │ │  <- 历史条目（可选中）
│ ├─────────────────────────────────────────┤ │
│ │ 📝 This is a longer text that will...   │ │
│ │    chrome.exe · 10分钟前                │ │
│ ├─────────────────────────────────────────┤ │
│ │ 🖼️ [图片预览]                           │ │
│ │    screenshot.exe · 1小时前              │ │
│ ├─────────────────────────────────────────┤ │
│ │ 📁 C:\Users\...\Documents               │ │
│ │    explorer.exe · 昨天                  │ │
│ └─────────────────────────────────────────┘ │
├─────────────────────────────────────────────┤
│  ⚙️ 设置    🗑️ 清空    📊 统计            │  <- 底部工具栏
└─────────────────────────────────────────────┘
```

### 9.2 交互设计

| 操作 | 结果 |
|------|------|
| `Enter` / `双击` | 选中内容写入剪贴板，关闭浮层 |
| `Ctrl+Enter` | 选中内容写入剪贴板，保持浮层 |
| `Escape` | 关闭浮层 |
| `↑` / `↓` | 上下导航 |
| `Ctrl+F` | 聚焦搜索框 |
| `Delete` | 删除选中条目 |
| `Ctrl+D` | 切换收藏状态 |
| `右键` | 显示上下文菜单 |

### 9.3 视觉规范

```cpp
struct UITheme {
    // 颜色
    uint32_t background_color = 0xF3F3F3;
    uint32_t item_color = 0xFFFFFF;
    uint32_t item_hover_color = 0xE5F3FF;
    uint32_t item_selected_color = 0xCCE8FF;
    uint32_t text_color = 0x1A1A1A;
    uint32_t text_secondary_color = 0x666666;
    uint32_t border_color = 0xE0E0E0;
    uint32_t accent_color = 0x0078D4;

    // 尺寸
    int corner_radius = 8;
    int item_height = 60;
    int padding = 12;
    int spacing = 4;
};
```

### 9.4 动画

- **淡入**: 浮层出现时 150ms 淡入
- **滑动**: 列表滚动时平滑滚动
- **选中**: 选中项高亮过渡 100ms

---

## 10. 热键系统

### 10.1 默认热键

| 热键 | 功能 |
|------|------|
| `Win+Ctrl+V` | 显示/隐藏历史浮层 |
| `Win+Ctrl+Shift+V` | 显示收藏浮层 |

### 10.2 热键注册

```cpp
// 在 ClipD 中注册全局热键
bool RegisterGlobalHotkey() {
    // MOD_WIN = 0x0008, MOD_CONTROL = 0x0002
    return RegisterHotKey(
        hwnd,           // 接收 WM_HOTKEY 的窗口
        HOTKEY_SHOW_ID, // 热键 ID
        MOD_WIN | MOD_CONTROL,  // 修饰键
        'V'             // 虚拟键码
    );
}

// 热键触发时启动 Overlay
case WM_HOTKEY:
    if (wParam == HOTKEY_SHOW_ID) {
        ShellExecute(NULL, "open", "Overlay.exe", NULL, NULL, SW_SHOWNORMAL);
    }
    break;
```

---

## 11. 支持的剪贴板格式

### 11.1 格式处理策略

| 格式 | Windows 格式名 | 存储方式 | 预览生成 |
|------|---------------|----------|----------|
| 纯文本 | CF_UNICODETEXT | UTF-8 文本 | 截取前100字符 |
| HTML | CF_HTML | 原始 HTML | 提取文本内容 |
| RTF | CF_RTF | 原始 RTF | 提取文本内容 |
| 位图 | CF_DIB | PNG 压缩 | 生成缩略图 |
| 文件列表 | CF_HDROP | 路径列表 | 显示文件名 |
| 自定义 | CF_PRIVATEFIRST+ | 原始二进制 | 显示格式名 |

### 11.2 格式转换

```cpp
class ClipboardFormatConverter {
public:
    // 读取剪贴板所有可用格式
    std::vector<ClipboardFormat> GetAvailableFormats();

    // 选择最佳格式存储
    ClipboardData ExtractBestFormat();

    // 生成预览文本
    std::string GeneratePreview(const ClipboardData& data);

    // 将数据写回剪贴板
    bool WriteToClipboard(const ClipboardData& data);
};
```

---

## 12. 搜索功能

### 12.1 搜索策略

1. **前缀匹配**: 输入即时触发搜索
2. **FTS5 全文搜索**: 使用 SQLite FTS5 进行高效搜索
3. **模糊搜索**: 支持拼音搜索（可选）
4. **高亮显示**: 搜索结果高亮匹配关键词

### 12.2 搜索实现

```sql
-- FTS5 搜索查询
SELECT e.*, snippet(clipboard_search, '[', ']', '...', 1, 32) as highlight
FROM clipboard_entries e
JOIN clipboard_search s ON e.id = s.rowid
WHERE clipboard_search MATCH ?
ORDER BY e.timestamp DESC
LIMIT 50;
```

### 12.3 搜索优化

- 输入防抖（300ms）
- 结果缓存
- 搜索结果按相关度 + 时间排序

---

## 13. 错误处理

### 13.1 错误码

| 错误码 | 描述 |
|--------|------|
| 0 | 成功 |
| 1001 | IPC 连接失败 |
| 1002 | IPC 超时 |
| 2001 | 数据库打开失败 |
| 2002 | 数据库写入失败 |
| 2003 | 数据库查询失败 |
| 3001 | 剪贴板读取失败 |
| 3002 | 剪贴板写入失败 |
| 4001 | 配置文件解析失败 |
| 9999 | 未知错误 |

### 13.2 日志系统

```cpp
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static void Init(const std::string& path, LogLevel level);
    static void Log(LogLevel level, const std::string& message);
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
};
```

---

## 14. 性能优化

### 14.1 内存优化

- Overlay 进程按需启动，使用完毕退出
- 图片数据压缩存储（PNG）
- 大文本截断存储（可配置最大长度）
- LRU 缓存预览数据

### 14.2 启动优化

- 延迟初始化非关键模块
- 数据库连接池
- 预编译 SQL 语句

### 14.3 查询优化

- 合理使用索引
- 分页查询
- 预览数据单独缓存

### 14.4 性能指标

| 指标 | 目标值 |
|------|--------|
| ClipD 内存占用 | < 10MB |
| Overlay 启动时间 | < 200ms |
| 1000条历史查询 | < 50ms |
| 搜索响应时间 | < 100ms |

---

## 15. 扩展机制

### 15.1 插件系统（未来）

预留插件接口，支持：

- 自定义数据格式处理器
- 自定义搜索过滤器
- 自定义 UI 主题

### 15.2 Webhook（未来）

支持配置 Webhook，在剪贴板变化时通知外部服务。

### 15.3 API 接口（未来）

提供本地 HTTP API，供其他程序调用。

---

## 附录

### A. 技术选型理由

| 技术 | 选型理由 |
|------|----------|
| C++ | 性能、内存控制、原生 Win32 API 访问 |
| SQLite | 轻量级、单文件、成熟稳定 |
| Named Pipe | 高效的本地 IPC 机制 |
| Direct2D | 现代化的 2D 渲染，硬件加速 |
| JSON | 人类可读，易于调试 |

### B. 参考资料

- [Windows Clipboard API](https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard)
- [SQLite FTS5 Extension](https://www.sqlite.org/fts5.html)
- [Direct2D Documentation](https://docs.microsoft.com/en-us/windows/win32/direct2d/direct2d-portal)

### C. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-02-22 | 初始设计文档 |
