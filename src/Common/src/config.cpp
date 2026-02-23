#include "common/config.h"
#include "common/logger.h"
#include <fstream>
#include <filesystem>

namespace clipx {

Config& Config::Instance() {
    static Config instance;
    return instance;
}

void Config::SetDefaults() {
    m_config = {
        {"version", "1.0"},
        {"hotkey", {
            {"show_overlay", {
                {"modifiers", {"win", "ctrl"}},
                {"key", "v"}
            }}
        }},
        {"ui", {
            {"width", 400},
            {"max_height", 600},
            {"font_size", 14},
            {"theme", "system"},
            {"opacity", 0.95},
            {"show_source_app", true},
            {"show_timestamp", true},
            {"preview_length", 100}
        }},
        {"storage", {
            {"max_entries", 10000},
            {"max_data_size_mb", 100},
            {"auto_cleanup_days", 30},
            {"exclude_types", nlohmann::json::array()}
        }},
        {"behavior", {
            {"auto_start", true},
            {"close_on_select", true},
            {"paste_after_select", true},
            {"smart_sort", true},
            {"deduplicate", true}
        }},
        {"advanced", {
            {"log_level", "info"},
            {"log_file", "logs/clipx.log"},
            {"db_file", "clipx.db"}
        }}
    };
}

bool Config::Load(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_path = path;

    // Set defaults first
    SetDefaults();

    try {
        // Check if file exists
        if (!std::filesystem::exists(path)) {
            // Config file not found, create default
            // Call Save(path) not Save() - Save() would try to acquire mutex again causing deadlock
            return Save(path);
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json loaded;
        file >> loaded;

        // Merge loaded config with defaults
        m_config.merge_patch(loaded);

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Config::Save(const std::string& path) {
    // Note: This function is called from Load() which already holds the lock
    // So we don't lock here to avoid deadlock

    try {
        // Create directory if it doesn't exist
        std::filesystem::path filePath(path);
        std::filesystem::path dir = filePath.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            // Don't use LOG_ERROR here to avoid potential issues
            return false;
        }

        file << m_config.dump(4);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Config::Save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_path.empty()) {
        return false;
    }

    try {
        // Create directory if it doesn't exist
        std::filesystem::path filePath(m_path);
        std::filesystem::path dir = filePath.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::ofstream file(m_path);
        if (!file.is_open()) {
            return false;
        }

        file << m_config.dump(4);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Config::Has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.contains(key);
}

// String specialization for nested values
template<>
std::string Config::GetNested<std::string>(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t start = 0;
    size_t end = key.find('.');
    nlohmann::json current = m_config;

    while (end != std::string::npos) {
        std::string part = key.substr(start, end - start);
        if (!current.is_object() || !current.contains(part)) {
            return defaultValue;
        }
        current = current[part];
        start = end + 1;
        end = key.find('.', start);
    }

    std::string finalPart = key.substr(start);
    if (!current.is_object() || !current.contains(finalPart)) {
        return defaultValue;
    }

    try {
        return current[finalPart].get<std::string>();
    } catch (...) {
        return defaultValue;
    }
}

} // namespace clipx
