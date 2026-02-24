#include "common/windows.h"
#include <string>
#include <filesystem>

#include "common/types.h"
#include "common/logger.h"
#include "common/config.h"
#include "common/ipc_protocol.h"
#include "common/utils.h"
#include "clipboard_listener.h"
#include "data_manager.h"
#include "ipc_server.h"
#include "hotkey_manager.h"
#include "tray_icon.h"
#include "auto_start.h"

namespace clipx {

// Window message for tray icon
constexpr UINT WM_TRAYICON = WM_USER + 1;

// Hotkey IDs
constexpr int HOTKEY_SHOW_OVERLAY = 1;

// Menu item IDs
constexpr int ID_TRAY_SHOW = 1001;
constexpr int ID_TRAY_AUTOSTART = 1002;
constexpr int ID_TRAY_SETTINGS = 1003;
constexpr int ID_TRAY_ABOUT = 1004;
constexpr int ID_TRAY_EXIT = 1005;

class ClipDApp {
public:
    bool Initialize(HINSTANCE hInstance) {
        m_hInstance = hInstance;

        // Get application directory
        m_appDir = utils::GetAppDataDir();
        utils::EnsureDirectory(m_appDir);

        // Initialize logger
        std::string logLevel = Config::Instance().GetNested<std::string>("advanced.log_level", "info");
        LogLevel level = LogLevel::Info;
        if (logLevel == "debug") level = LogLevel::Debug;
        else if (logLevel == "warning") level = LogLevel::Warning;
        else if (logLevel == "error") level = LogLevel::Error;

        Logger::Instance().Init(m_appDir + "\\logs\\clipx.log", level);

        // Load config
        Config::Instance().Load(m_appDir + "\\config.json");

        // Create hidden window
        if (!CreateHiddenWindow()) {
            LOG_ERROR("Failed to create hidden window");
            return false;
        }

        // Initialize database
        std::string dbPath = m_appDir + "\\" + Config::Instance().GetNested<std::string>("advanced.db_file", "clipx.db");
        if (!DataManager::Instance().Initialize(dbPath)) {
            LOG_ERROR("Failed to initialize database");
            return false;
        }

        // Clean up orphaned tag records (tags pointing to deleted entries)
        int cleanedTags = DataManager::Instance().CleanupOrphanedTags();
        if (cleanedTags > 0) {
            LOG_INFO("Cleaned up " + std::to_string(cleanedTags) + " orphaned tag records on startup");
        }

        // Clear memory entries on startup (only tagged entries persist)
        DataManager::Instance().ClearMemoryEntries();

        // Initialize clipboard listener
        if (!m_clipboardListener.Initialize(m_hwnd)) {
            LOG_ERROR("Failed to initialize clipboard listener");
            return false;
        }

        m_clipboardListener.SetOnClipboardChangeCallback([this](const ClipboardEntry& entry) {
            OnClipboardChange(entry);
        });

        // Initialize IPC server
        if (!m_ipcServer.Start(IPC_PIPE_NAME)) {
            LOG_ERROR("Failed to start IPC server");
            return false;
        }

        m_ipcServer.SetRequestHandler([this](const IPCRequest& request) {
            return HandleIPCRequest(request);
        });

        // Initialize hotkey manager
        if (!m_hotkeyManager.Initialize(m_hwnd)) {
            LOG_ERROR("Failed to initialize hotkey manager");
            return false;
        }

        // Register hotkey (F9)
        UINT modifiers = 0;
        UINT vk = VK_F9;
        if (!m_hotkeyManager.RegisterHotkey(HOTKEY_SHOW_OVERLAY, modifiers, vk, [this]() {
            ShowOverlay();
        })) {
            LOG_WARN("Hotkey already registered by another application");
        }

        // Initialize tray icon
        if (!m_trayIcon.Initialize(m_hwnd, WM_TRAYICON)) {
            LOG_ERROR("Failed to initialize tray icon");
            return false;
        }

        m_trayIcon.SetTooltip("ClipX");
        m_trayIcon.SetIconFromResource(1);  // Load icon from resources (ID = 1)
        m_trayIcon.SetMenuCallback([this](int id) {
            OnTrayMenuCommand(id);
        });

        // Update auto-start menu item
        m_trayIcon.SetMenuItemChecked(ID_TRAY_AUTOSTART, AutoStart::IsEnabled());

        LOG_INFO("ClipD initialized successfully");
        return true;
    }

    void Run() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void Shutdown() {
        m_ipcServer.Stop();
        DataManager::Instance().Shutdown();
        m_trayIcon.Shutdown();
        Logger::Instance().Shutdown();
    }

private:
    bool CreateHiddenWindow() {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.lpfnWndProc = WndProcStatic;
        wcex.hInstance = m_hInstance;
        wcex.lpszClassName = L"ClipD_HiddenWindow";

        if (!RegisterClassExW(&wcex)) {
            return false;
        }

        m_hwnd = CreateWindowExW(
            0,
            L"ClipD_HiddenWindow",
            L"ClipD",
            0,
            0, 0, 0, 0,
            HWND_MESSAGE,  // Message-only window
            nullptr,
            m_hInstance,
            this
        );

        return m_hwnd != nullptr;
    }

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ClipDApp* app = nullptr;

        if (msg == WM_NCCREATE) {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = reinterpret_cast<ClipDApp*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<ClipDApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (app) {
            return app->WndProc(hwnd, msg, wParam, lParam);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_CLIPBOARDUPDATE:
                m_clipboardListener.ProcessClipboardChange();
                return 0;

            case WM_HOTKEY:
                m_hotkeyManager.HandleHotkey(wParam);
                return 0;

            case WM_TRAYICON:
                if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
                    POINT pt;
                    GetCursorPos(&pt);
                    m_trayIcon.ShowContextMenu(hwnd, pt.x, pt.y);
                } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                    ShowOverlay();
                }
                return 0;

            case WM_COMMAND:
                OnTrayMenuCommand(LOWORD(wParam));
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }

    void OnClipboardChange(const ClipboardEntry& entry) {
        // Check for deduplication against database
        if (Config::Instance().GetNested<bool>("behavior.deduplicate", true)) {
            auto hash = utils::ComputeHash(entry.data);
            auto existingId = DataManager::Instance().FindByHash(hash);

            if (existingId.has_value()) {
                // Update existing entry in database
                DataManager::Instance().UpdateCopyCount(*existingId, entry.timestamp);
                LOG_DEBUG("Updated existing entry: " + std::to_string(*existingId));
                return;
            }
        }

        // Insert new entry to memory only (not persisted until tagged)
        int64_t id = DataManager::Instance().InsertMemoryOnly(entry);
        if (id < 0) {
            LOG_INFO("Inserted new clipboard entry to memory: " + std::to_string(id));

            // Check for auto-cleanup (database only)
            int cleanupDays = Config::Instance().GetNested<int>("storage.auto_cleanup_days", 30);
            if (cleanupDays > 0) {
                int64_t cutoffTime = utils::GetCurrentTimestamp() - (cleanupDays * 24 * 60 * 60 * 1000LL);
                DataManager::Instance().DeleteOlderThan(cutoffTime);
            }
        }
    }

    IPCResponse HandleIPCRequest(const IPCRequest& request) {
        LOG_DEBUG("Handling IPC request: " + request.action);

        if (request.action == IPCAction::PING) {
            return IPCResponse::Success(request.requestId, {{"pong", true}});
        }

        if (request.action == IPCAction::GET_HISTORY) {
            QueryOptions options;
            options.limit = request.params.value("limit", 100);
            options.offset = request.params.value("offset", 0);
            options.favoritesOnly = request.params.value("favorites_only", false);

            int typeFilter = request.params.value("type", 0);
            if (typeFilter > 0) {
                options.filterType = static_cast<ClipboardDataType>(typeFilter);
            }

            auto entries = DataManager::Instance().Query(options);

            nlohmann::json entriesJson = nlohmann::json::array();
            for (const auto& entry : entries) {
                entriesJson.push_back(ClipboardEntryToJson(entry));
            }

            return IPCResponse::Success(request.requestId, {
                {"entries", entriesJson},
                {"total", entries.size()}
            });
        }

        if (request.action == IPCAction::SEARCH) {
            std::string keyword = request.params.value("keyword", "");
            int limit = request.params.value("limit", 50);

            if (keyword.empty()) {
                return IPCResponse::Error(request.requestId, "Missing keyword", IPCError::IPC_INVALID_REQUEST);
            }

            auto entries = DataManager::Instance().Search(keyword, limit);

            nlohmann::json entriesJson = nlohmann::json::array();
            for (const auto& entry : entries) {
                entriesJson.push_back(ClipboardEntryToJson(entry));
            }

            return IPCResponse::Success(request.requestId, {{"entries", entriesJson}});
        }

        if (request.action == IPCAction::GET_ENTRY) {
            int64_t id = request.params.value("id", static_cast<int64_t>(0));
            auto entry = DataManager::Instance().GetEntry(id);

            if (!entry.has_value()) {
                return IPCResponse::Error(request.requestId, "Entry not found", IPCError::DB_NOT_FOUND);
            }

            return IPCResponse::Success(request.requestId, ClipboardEntryToJson(*entry));
        }

        if (request.action == IPCAction::SET_CLIPBOARD) {
            int64_t id = request.params.value("id", static_cast<int64_t>(0));

            // Tell the listener to ignore the next clipboard change
            // (since we're about to set the clipboard ourselves)
            m_clipboardListener.IgnoreNextChange();

            if (!DataManager::Instance().SetClipboard(id)) {
                return IPCResponse::Error(request.requestId, "Failed to set clipboard", IPCError::CLIPBOARD_WRITE_FAILED);
            }

            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::DELETE_ENTRY) {
            int64_t id = request.params.value("id", static_cast<int64_t>(0));

            if (!DataManager::Instance().Delete(id)) {
                return IPCResponse::Error(request.requestId, "Failed to delete entry", IPCError::DB_WRITE_FAILED);
            }

            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::TOGGLE_FAVORITE) {
            int64_t id = request.params.value("id", static_cast<int64_t>(0));

            if (!DataManager::Instance().ToggleFavorite(id)) {
                return IPCResponse::Error(request.requestId, "Failed to toggle favorite", IPCError::DB_WRITE_FAILED);
            }

            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::GET_STATS) {
            auto stats = DataManager::Instance().GetStats();
            return IPCResponse::Success(request.requestId, {
                {"count", stats.totalCount},
                {"text_size", stats.textSize},
                {"image_size", stats.imageSize},
                {"total_size", stats.totalSize}
            });
        }

        if (request.action == IPCAction::CLEAR_ALL) {
            if (!DataManager::Instance().DeleteAll()) {
                return IPCResponse::Error(request.requestId, "Failed to clear all", IPCError::DB_WRITE_FAILED);
            }
            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::GET_CONFIG) {
            return IPCResponse::Success(request.requestId, Config::Instance().GetRaw());
        }

        if (request.action == IPCAction::SET_CONFIG) {
            // Merge new config
            // TODO: Implement config update
            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::SHUTDOWN) {
            PostMessage(m_hwnd, WM_CLOSE, 0, 0);
            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::ADD_TAG) {
            int64_t id = request.params.value("id", static_cast<int64_t>(0));
            std::string tag = request.params.value("tag", "");

            if (tag.empty()) {
                return IPCResponse::Error(request.requestId, "Missing tag name", IPCError::IPC_INVALID_REQUEST);
            }

            if (!DataManager::Instance().AddTag(id, tag)) {
                return IPCResponse::Error(request.requestId, "Failed to add tag", IPCError::DB_WRITE_FAILED);
            }

            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::REMOVE_TAG) {
            int64_t id = request.params.value("id", static_cast<int64_t>(0));
            std::string tag = request.params.value("tag", "");

            if (tag.empty()) {
                return IPCResponse::Error(request.requestId, "Missing tag name", IPCError::IPC_INVALID_REQUEST);
            }

            if (!DataManager::Instance().RemoveTag(id, tag)) {
                return IPCResponse::Error(request.requestId, "Failed to remove tag", IPCError::DB_WRITE_FAILED);
            }

            return IPCResponse::Success(request.requestId, {{"success", true}});
        }

        if (request.action == IPCAction::GET_TAGS) {
            int64_t id = request.params.value("id", static_cast<int64_t>(0));

            auto tags = DataManager::Instance().GetTags(id);

            nlohmann::json tagsJson = nlohmann::json::array();
            for (const auto& tag : tags) {
                tagsJson.push_back(tag);
            }

            return IPCResponse::Success(request.requestId, {{"tags", tagsJson}});
        }

        if (request.action == IPCAction::GET_ALL_TAGS) {
            auto tags = DataManager::Instance().GetAllTags();

            nlohmann::json tagsJson = nlohmann::json::array();
            for (const auto& [tagName, count] : tags) {
                tagsJson.push_back({
                    {"name", tagName},
                    {"count", count}
                });
            }

            return IPCResponse::Success(request.requestId, {{"tags", tagsJson}});
        }

        return IPCResponse::Error(request.requestId, "Unknown action: " + request.action, IPCError::IPC_INVALID_REQUEST);
    }

    void ShowOverlay() {
        LOG_DEBUG("Showing overlay");

        // Get path to Overlay.exe
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::filesystem::path overlayPath = std::filesystem::path(exePath).parent_path() / "Overlay.exe";

        std::wstring cmdLine = L"\"" + overlayPath.wstring() + L"\"";

        STARTUPINFOW si = {sizeof(STARTUPINFOW)};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNORMAL;

        PROCESS_INFORMATION pi = {};

        if (CreateProcessW(
            nullptr,
            &cmdLine[0],
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi
        )) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            LOG_INFO("Overlay started");
        } else {
            LOG_ERROR("Failed to start overlay: " + std::to_string(GetLastError()));
        }
    }

    void OnTrayMenuCommand(int id) {
        switch (id) {
            case ID_TRAY_SHOW:
                ShowOverlay();
                break;

            case ID_TRAY_AUTOSTART: {
                bool isEnabled = AutoStart::IsEnabled();
                if (AutoStart::Enable(!isEnabled)) {
                    m_trayIcon.SetMenuItemChecked(ID_TRAY_AUTOSTART, !isEnabled);
                    Config::Instance().SetNested<bool>("behavior.auto_start", !isEnabled);
                    Config::Instance().Save();
                }
                break;
            }

            case ID_TRAY_SETTINGS:
                // TODO: Open settings dialog
                MessageBoxW(m_hwnd, L"Settings dialog not yet implemented", L"ClipX", MB_OK);
                break;

            case ID_TRAY_ABOUT:
                MessageBoxW(m_hwnd,
                    L"ClipX - Clipboard Manager\nVersion 1.0\n\nA lightweight clipboard history manager for Windows.",
                    L"About ClipX",
                    MB_OK | MB_ICONINFORMATION
                );
                break;

            case ID_TRAY_EXIT:
                DestroyWindow(m_hwnd);
                break;
        }
    }

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    std::string m_appDir;

    ClipboardListener m_clipboardListener;
    IPCServer m_ipcServer;
    HotkeyManager m_hotkeyManager;
    TrayIcon m_trayIcon;
};

} // namespace clipx

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Check for single instance
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"ClipX_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 1;
    }

    // Initialize COM
    CoInitialize(nullptr);

    clipx::ClipDApp app;
    if (!app.Initialize(hInstance)) {
        CoUninitialize();
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 1;
    }

    app.Run();
    app.Shutdown();

    CoUninitialize();
    ReleaseMutex(mutex);
    CloseHandle(mutex);

    return 0;
}
