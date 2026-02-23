#include <windows.h>
#include <string>
#include <vector>
#include <memory>

#include "common/types.h"
#include "common/logger.h"
#include "common/config.h"
#include "common/ipc_protocol.h"
#include "common/utils.h"
#include "ipc_client.h"
#include "overlay_window.h"

namespace clipx {

class OverlayApp {
public:
    bool Initialize(HINSTANCE hInstance) {
        m_hInstance = hInstance;

        // Initialize logger (minimal logging for overlay)
        Logger::Instance().Init(utils::GetAppDataDir() + "\\logs\\overlay.log", LogLevel::Debug);

        // Connect to ClipD
        if (!m_ipcClient.Connect(IPC_PIPE_NAME, 2000)) {
            LOG_ERROR("Failed to connect to ClipD");
            return false;
        }

        // Create overlay window
        if (!m_overlayWindow.Initialize(m_hInstance)) {
            LOG_ERROR("Failed to initialize overlay window");
            return false;
        }

        m_overlayWindow.SetOnEntrySelected([this](int64_t id) {
            OnEntrySelected(id);
        });

        m_overlayWindow.SetOnClose([this]() {
            PostQuitMessage(0);
        });

        m_overlayWindow.SetOnSearch([this](const std::string& keyword) {
            SearchHistory(keyword);
        });

        m_overlayWindow.SetOnAddTag([this](int64_t entryId, const std::string& tag) {
            AddTagToEntry(entryId, tag);
        });

        m_overlayWindow.SetOnDelete([this](int64_t entryId) {
            DeleteEntry(entryId);
        });

        m_overlayWindow.SetOnGetTags([this](int64_t entryId) -> std::vector<std::string> {
            return GetTagsForEntry(entryId);
        });

        m_overlayWindow.SetOnGetAllTags([this]() -> std::vector<std::pair<std::string, int>> {
            return GetAllTags();
        });

        // Load initial data
        LoadHistory();

        LOG_INFO("Overlay initialized");
        return true;
    }

    void Run() {
        m_overlayWindow.Show();

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Shutdown();
    }

private:
    void LoadHistory() {
        IPCRequest request;
        request.action = IPCAction::GET_HISTORY;
        request.requestId = 1;
        request.params = {
            {"limit", 100},
            {"offset", 0}
        };

        IPCResponse response = m_ipcClient.SendRequest(request);

        if (!response.success) {
            LOG_ERROR("Failed to load history: " + response.error);
            return;
        }

        std::vector<UIEntry> entries;

        if (response.data.contains("entries") && response.data["entries"].is_array()) {
            for (const auto& item : response.data["entries"]) {
                UIEntry entry;
                entry.id = item.value("id", static_cast<int64_t>(0));
                entry.preview = item.value("preview", "");
                entry.sourceApp = item.value("source_app", "");
                entry.type = static_cast<ClipboardDataType>(item.value("type", 1));
                entry.isFavorited = item.value("is_favorited", false);
                entry.copyCount = item.value("copy_count", 1);

                // Parse tags
                if (item.contains("tags") && item["tags"].is_array()) {
                    for (const auto& tag : item["tags"]) {
                        if (tag.is_string()) {
                            entry.tags.push_back(tag.get<std::string>());
                        }
                    }
                }

                // Format timestamp
                int64_t timestamp = item.value("timestamp", static_cast<int64_t>(0));
                entry.timestampStr = utils::FormatTimestamp(timestamp);

                entries.push_back(entry);
            }
        }

        m_overlayWindow.SetEntries(entries);
        LOG_DEBUG("Loaded " + std::to_string(entries.size()) + " entries");
    }

    void OnEntrySelected(int64_t id) {
        IPCRequest request;
        request.action = IPCAction::SET_CLIPBOARD;
        request.requestId = 2;
        request.params = {{"id", id}};

        IPCResponse response = m_ipcClient.SendRequest(request);

        if (!response.success) {
            LOG_ERROR("Failed to set clipboard: " + response.error);
        } else {
            LOG_INFO("Set clipboard from entry: " + std::to_string(id));

            // Optionally paste after setting clipboard
            if (Config::Instance().GetNested<bool>("behavior.paste_after_select", false)) {
                // Simulate Ctrl+V
                SimulatePaste();
            }
        }
    }

    void SimulatePaste() {
        // Give time for clipboard to be set
        Sleep(50);

        // Simulate Ctrl+V keypress
        INPUT inputs[4] = {};

        // Key down: Ctrl
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;

        // Key down: V
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 'V';

        // Key up: V
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'V';
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

        // Key up: Ctrl
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_CONTROL;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(4, inputs, sizeof(INPUT));
    }

    void SearchHistory(const std::string& keyword) {
        std::vector<UIEntry> entries;

        if (keyword.empty()) {
            // If keyword is empty, load all history
            LoadHistory();
            return;
        }

        // Search via IPC
        IPCRequest request;
        request.action = IPCAction::SEARCH;
        request.requestId = 10;
        request.params = {
            {"keyword", keyword},
            {"limit", 100}
        };

        IPCResponse response = m_ipcClient.SendRequest(request);

        if (!response.success) {
            LOG_ERROR("Failed to search: " + response.error);
            m_overlayWindow.SetEntries(entries);
            return;
        }

        if (response.data.contains("entries") && response.data["entries"].is_array()) {
            for (const auto& item : response.data["entries"]) {
                UIEntry entry;
                entry.id = item.value("id", static_cast<int64_t>(0));
                entry.preview = item.value("preview", "");
                entry.sourceApp = item.value("source_app", "");
                entry.type = static_cast<ClipboardDataType>(item.value("type", 1));
                entry.isFavorited = item.value("is_favorited", false);
                entry.copyCount = item.value("copy_count", 1);

                // Parse tags
                if (item.contains("tags") && item["tags"].is_array()) {
                    for (const auto& tag : item["tags"]) {
                        if (tag.is_string()) {
                            entry.tags.push_back(tag.get<std::string>());
                        }
                    }
                }

                int64_t timestamp = item.value("timestamp", static_cast<int64_t>(0));
                entry.timestampStr = utils::FormatTimestamp(timestamp);

                entries.push_back(entry);
            }
        }

        m_overlayWindow.SetEntries(entries);
        LOG_DEBUG("Search found " + std::to_string(entries.size()) + " entries for: " + keyword);
    }

    void AddTagToEntry(int64_t entryId, const std::string& tag) {
        IPCRequest request;
        request.action = IPCAction::ADD_TAG;
        request.requestId = 11;
        request.params = {
            {"id", entryId},
            {"tag", tag}
        };

        IPCResponse response = m_ipcClient.SendRequest(request);

        if (!response.success) {
            LOG_ERROR("Failed to add tag: " + response.error);
        } else {
            LOG_INFO("Added tag '" + tag + "' to entry: " + std::to_string(entryId));
        }
    }

    void DeleteEntry(int64_t entryId) {
        IPCRequest request;
        request.action = IPCAction::DELETE_ENTRY;
        request.requestId = 12;
        request.params = {{"id", entryId}};

        IPCResponse response = m_ipcClient.SendRequest(request);

        if (!response.success) {
            LOG_ERROR("Failed to delete entry: " + response.error);
        } else {
            LOG_INFO("Deleted entry: " + std::to_string(entryId));
            // Reload history
            LoadHistory();
        }
    }

    std::vector<std::string> GetTagsForEntry(int64_t entryId) {
        IPCRequest request;
        request.action = IPCAction::GET_TAGS;
        request.requestId = 13;
        request.params = {{"id", entryId}};

        IPCResponse response = m_ipcClient.SendRequest(request);

        std::vector<std::string> tags;

        if (!response.success) {
            LOG_ERROR("Failed to get tags: " + response.error);
            return tags;
        }

        if (response.data.contains("tags") && response.data["tags"].is_array()) {
            for (const auto& tag : response.data["tags"]) {
                if (tag.is_string()) {
                    tags.push_back(tag.get<std::string>());
                }
            }
        }

        LOG_DEBUG("Got " + std::to_string(tags.size()) + " tags for entry: " + std::to_string(entryId));
        return tags;
    }

    std::vector<std::pair<std::string, int>> GetAllTags() {
        IPCRequest request;
        request.action = IPCAction::GET_ALL_TAGS;
        request.requestId = 14;

        IPCResponse response = m_ipcClient.SendRequest(request);

        std::vector<std::pair<std::string, int>> tags;

        if (!response.success) {
            LOG_ERROR("Failed to get all tags: " + response.error);
            return tags;
        }

        if (response.data.contains("tags") && response.data["tags"].is_array()) {
            for (const auto& tag : response.data["tags"]) {
                if (tag.is_object() && tag.contains("name") && tag.contains("count")) {
                    std::string name = tag["name"].get<std::string>();
                    int count = tag["count"].get<int>();
                    tags.push_back({name, count});
                }
            }
        }

        LOG_DEBUG("Got " + std::to_string(tags.size()) + " tags");
        return tags;
    }

    void Shutdown() {
        m_ipcClient.Disconnect();
        Logger::Instance().Shutdown();
    }

    HINSTANCE m_hInstance = nullptr;
    IPCClient m_ipcClient;
    OverlayWindow m_overlayWindow;
};

} // namespace clipx

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    clipx::OverlayApp app;

    if (!app.Initialize(hInstance)) {
        return 1;
    }

    app.Run();
    return 0;
}
