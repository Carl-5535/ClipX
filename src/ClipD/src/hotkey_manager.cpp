#include "hotkey_manager.h"
#include "common/logger.h"

namespace clipx {

HotkeyManager::HotkeyManager() = default;

HotkeyManager::~HotkeyManager() {
    for (const auto& pair : m_callbacks) {
        UnregisterHotKey(m_hwnd, pair.first);
    }
}

bool HotkeyManager::Initialize(HWND hwnd) {
    if (m_initialized) {
        return true;
    }

    m_hwnd = hwnd;
    m_initialized = true;
    LOG_INFO("HotkeyManager initialized");
    return true;
}

bool HotkeyManager::RegisterHotkey(int id, UINT modifiers, UINT vk, HotkeyCallback callback) {
    if (!m_initialized) {
        LOG_ERROR("HotkeyManager not initialized");
        return false;
    }

    // Unregister first if already registered
    if (m_callbacks.find(id) != m_callbacks.end()) {
        UnregisterHotKey(m_hwnd, id);
    }

    if (!RegisterHotKey(m_hwnd, id, modifiers, vk)) {
        DWORD error = GetLastError();
        if (error == ERROR_HOTKEY_ALREADY_REGISTERED) {
            LOG_ERROR("Hotkey already registered by another application");
        } else {
            LOG_ERROR("Failed to register hotkey: " + std::to_string(error));
        }
        return false;
    }

    m_callbacks[id] = std::move(callback);
    LOG_INFO("Hotkey registered: id=" + std::to_string(id));
    return true;
}

bool HotkeyManager::UnregisterHotkey(int id) {
    if (!m_initialized) {
        return false;
    }

    auto it = m_callbacks.find(id);
    if (it == m_callbacks.end()) {
        return false;
    }

    if (!UnregisterHotKey(m_hwnd, id)) {
        LOG_ERROR("Failed to unregister hotkey: " + std::to_string(GetLastError()));
        return false;
    }

    m_callbacks.erase(it);
    LOG_INFO("Hotkey unregistered: id=" + std::to_string(id));
    return true;
}

void HotkeyManager::HandleHotkey(WPARAM wParam) {
    int id = static_cast<int>(wParam);

    auto it = m_callbacks.find(id);
    if (it != m_callbacks.end()) {
        LOG_DEBUG("Hotkey triggered: id=" + std::to_string(id));
        it->second();
    }
}

bool HotkeyManager::IsRegistered(int id) const {
    return m_callbacks.find(id) != m_callbacks.end();
}

} // namespace clipx
