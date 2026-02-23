#pragma once

#include <string>
#include <cstdint>
#include "json/json.hpp"
#include "common/types.h"

namespace clipx {

// IPC message header (binary, sent before JSON payload)
struct IPCHeader {
    uint32_t magic;         // Magic number: 0x434C4958 ("CLIX")
    uint32_t payloadSize;   // Size of JSON payload in bytes
};

constexpr uint32_t IPC_MAGIC = 0x434C4958;
constexpr const char* IPC_PIPE_NAME = "\\\\.\\pipe\\ClipX_IPC";
constexpr int IPC_DEFAULT_TIMEOUT_MS = 5000;
constexpr int IPC_BUFFER_SIZE = 65536;

// IPC Request
struct IPCRequest {
    std::string action;
    int32_t requestId = 0;
    nlohmann::json params;

    nlohmann::json ToJson() const {
        return {
            {"action", action},
            {"request_id", requestId},
            {"params", params}
        };
    }

    static IPCRequest FromJson(const nlohmann::json& json) {
        IPCRequest req;
        req.action = json.value("action", "");
        req.requestId = json.value("request_id", 0);
        req.params = json.value("params", nlohmann::json::object());
        return req;
    }
};

// IPC Response
struct IPCResponse {
    int32_t requestId = 0;
    bool success = false;
    nlohmann::json data;
    std::string error;
    int32_t errorCode = 0;

    nlohmann::json ToJson() const {
        nlohmann::json json = {
            {"request_id", requestId},
            {"success", success},
            {"data", data}
        };
        if (!error.empty()) {
            json["error"] = error;
            json["error_code"] = errorCode;
        }
        return json;
    }

    static IPCResponse FromJson(const nlohmann::json& json) {
        IPCResponse resp;
        resp.requestId = json.value("request_id", 0);
        resp.success = json.value("success", false);
        resp.data = json.value("data", nlohmann::json::object());
        resp.error = json.value("error", "");
        resp.errorCode = json.value("error_code", 0);
        return resp;
    }

    static IPCResponse Success(int32_t requestId, const nlohmann::json& data = {}) {
        IPCResponse resp;
        resp.requestId = requestId;
        resp.success = true;
        resp.data = data;
        return resp;
    }

    static IPCResponse Error(int32_t requestId, const std::string& error, int32_t errorCode = 9999) {
        IPCResponse resp;
        resp.requestId = requestId;
        resp.success = false;
        resp.error = error;
        resp.errorCode = errorCode;
        return resp;
    }
};

// IPC Notification (async events from server to client)
struct IPCNotification {
    std::string event;
    nlohmann::json data;

    nlohmann::json ToJson() const {
        return {
            {"event", event},
            {"data", data}
        };
    }

    static IPCNotification FromJson(const nlohmann::json& json) {
        IPCNotification notif;
        notif.event = json.value("event", "");
        notif.data = json.value("data", nlohmann::json::object());
        return notif;
    }
};

// Action types
namespace IPCAction {
    constexpr const char* PING = "ping";
    constexpr const char* GET_HISTORY = "get_history";
    constexpr const char* SEARCH = "search";
    constexpr const char* GET_ENTRY = "get_entry";
    constexpr const char* SET_CLIPBOARD = "set_clipboard";
    constexpr const char* DELETE_ENTRY = "delete_entry";
    constexpr const char* TOGGLE_FAVORITE = "toggle_favorite";
    constexpr const char* GET_STATS = "get_stats";
    constexpr const char* CLEAR_ALL = "clear_all";
    constexpr const char* GET_CONFIG = "get_config";
    constexpr const char* SET_CONFIG = "set_config";
    constexpr const char* SHUTDOWN = "shutdown";
    constexpr const char* ADD_TAG = "add_tag";
    constexpr const char* REMOVE_TAG = "remove_tag";
    constexpr const char* GET_TAGS = "get_tags";
    constexpr const char* GET_ALL_TAGS = "get_all_tags";
}

// Event types
namespace IPCEvent {
    constexpr const char* CLIPBOARD_CHANGED = "clipboard_changed";
    constexpr const char* ENTRY_DELETED = "entry_deleted";
    constexpr const char* CONFIG_CHANGED = "config_changed";
}

// Error codes
namespace IPCError {
    constexpr int32_t SUCCESS = 0;
    constexpr int32_t IPC_CONNECTION_FAILED = 1001;
    constexpr int32_t IPC_TIMEOUT = 1002;
    constexpr int32_t IPC_INVALID_REQUEST = 1003;
    constexpr int32_t DB_OPEN_FAILED = 2001;
    constexpr int32_t DB_WRITE_FAILED = 2002;
    constexpr int32_t DB_QUERY_FAILED = 2003;
    constexpr int32_t DB_NOT_FOUND = 2004;
    constexpr int32_t CLIPBOARD_READ_FAILED = 3001;
    constexpr int32_t CLIPBOARD_WRITE_FAILED = 3002;
    constexpr int32_t CONFIG_PARSE_FAILED = 4001;
    constexpr int32_t UNKNOWN_ERROR = 9999;
}

// Helper to convert ClipboardEntry to JSON
inline nlohmann::json ClipboardEntryToJson(const ClipboardEntry& entry) {
    nlohmann::json json = {
        {"id", entry.id},
        {"timestamp", entry.timestamp},
        {"type", static_cast<int32_t>(entry.type)},
        {"preview", entry.preview},
        {"source_app", entry.sourceApp},
        {"copy_count", entry.copyCount},
        {"is_favorited", entry.isFavorited},
        {"is_tagged", entry.isTagged}
    };
    if (!entry.tags.empty()) {
        json["tags"] = entry.tags;
    }
    return json;
}

// Helper to convert JSON to ClipboardEntry
inline ClipboardEntry JsonToClipboardEntry(const nlohmann::json& json) {
    ClipboardEntry entry;
    entry.id = json.value("id", static_cast<int64_t>(0));
    entry.timestamp = json.value("timestamp", static_cast<int64_t>(0));
    entry.type = static_cast<ClipboardDataType>(json.value("type", 1));
    entry.preview = json.value("preview", "");
    entry.sourceApp = json.value("source_app", "");
    entry.copyCount = json.value("copy_count", 1);
    entry.isFavorited = json.value("is_favorited", false);
    entry.isTagged = json.value("is_tagged", false);
    if (json.contains("tags") && json["tags"].is_array()) {
        for (const auto& tag : json["tags"]) {
            if (tag.is_string()) {
                entry.tags.push_back(tag.get<std::string>());
            }
        }
    }
    return entry;
}

} // namespace clipx
