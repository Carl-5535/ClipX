#pragma once

#include <string>
#include <functional>
#include <windows.h>
#include <shellapi.h>

namespace clipx {

class TrayIcon {
public:
    using MenuCallback = std::function<void(int menuItemId)>;

    TrayIcon();
    ~TrayIcon();

    bool Initialize(HWND hwnd, UINT callbackMessage);
    void Shutdown();

    void SetTooltip(const std::string& tooltip);
    void SetIcon(HICON icon);
    void SetIconFromResource(int resourceId);

    // Show balloon notification
    void ShowBalloon(const std::string& title, const std::string& message, DWORD flags = NIIF_INFO);

    // Show context menu
    void ShowContextMenu(HWND hwnd, int x, int y);

    // Menu item management
    void AddMenuItem(int id, const std::string& text, bool enabled = true);
    void AddMenuSeparator();
    void SetMenuItemChecked(int id, bool checked);
    void SetMenuCallback(MenuCallback callback);

private:
    HWND m_hwnd = nullptr;
    UINT m_callbackMessage = 0;
    bool m_initialized = false;
    HMENU m_menu = nullptr;
    MenuCallback m_menuCallback;
    NOTIFYICONDATAW m_nid = {};
};

} // namespace clipx
