#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <windows.h>

namespace clipx {

class HotkeyManager {
public:
    using HotkeyCallback = std::function<void()>;

    HotkeyManager();
    ~HotkeyManager();

    bool Initialize(HWND hwnd);

    // Register a hotkey
    // modifiers: MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN, MOD_NOREPEAT
    // vk: virtual key code
    bool RegisterHotkey(int id, UINT modifiers, UINT vk, HotkeyCallback callback);

    // Unregister a hotkey
    bool UnregisterHotkey(int id);

    // Handle WM_HOTKEY message
    void HandleHotkey(WPARAM wParam);

    // Check if hotkey is registered
    bool IsRegistered(int id) const;

private:
    HWND m_hwnd = nullptr;
    bool m_initialized = false;
    std::unordered_map<int, HotkeyCallback> m_callbacks;
};

} // namespace clipx
