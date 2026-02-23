#pragma once

#include <string>
#include <mutex>
#include <functional>
#include "json/json.hpp"

namespace clipx {

class Config {
public:
    static Config& Instance();

    bool Load(const std::string& path);
    bool Save(const std::string& path);
    bool Save();

    template<typename T>
    T Get(const std::string& key, const T& defaultValue) const;

    template<typename T>
    void Set(const std::string& key, const T& value);

    bool Has(const std::string& key) const;

    // Get nested values using dot notation (e.g., "ui.width")
    template<typename T>
    T GetNested(const std::string& key, const T& defaultValue) const;

    // Set nested values using dot notation (e.g., "ui.width")
    template<typename T>
    void SetNested(const std::string& key, const T& value);

    const nlohmann::json& GetRaw() const { return m_config; }
    nlohmann::json& GetRaw() { return m_config; }

    // Get config file path
    const std::string& GetPath() const { return m_path; }

private:
    Config() = default;
    ~Config() = default;

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    nlohmann::json m_config;
    std::string m_path;
    mutable std::mutex m_mutex;

    void SetDefaults();
};

// Template implementations
template<typename T>
T Config::Get(const std::string& key, const T& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_config.contains(key)) {
        try {
            return m_config[key].get<T>();
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

template<typename T>
void Config::Set(const std::string& key, const T& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = value;
}

template<typename T>
T Config::GetNested(const std::string& key, const T& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Split key by '.'
    size_t start = 0;
    size_t end = key.find('.');
    nlohmann::json current = m_config;

    while (end != std::string::npos) {
        std::string part = key.substr(start, end - start);
        if (!current.contains(part)) {
            return defaultValue;
        }
        current = current[part];
        start = end + 1;
        end = key.find('.', start);
    }

    // Get the final part
    std::string finalPart = key.substr(start);
    if (!current.contains(finalPart)) {
        return defaultValue;
    }

    try {
        return current[finalPart].get<T>();
    } catch (...) {
        return defaultValue;
    }
}

template<typename T>
void Config::SetNested(const std::string& key, const T& value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Split key by '.'
    size_t start = 0;
    size_t end = key.find('.');
    nlohmann::json* current = &m_config;

    while (end != std::string::npos) {
        std::string part = key.substr(start, end - start);
        if (!current->contains(part)) {
            (*current)[part] = nlohmann::json::object();
        }
        current = &(*current)[part];
        start = end + 1;
        end = key.find('.', start);
    }

    // Set the final part
    std::string finalPart = key.substr(start);
    (*current)[finalPart] = value;
}

// Specializations
template<>
std::string Config::GetNested<std::string>(const std::string& key, const std::string& defaultValue) const;

} // namespace clipx
