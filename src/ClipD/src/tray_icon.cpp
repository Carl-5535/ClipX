#include "tray_icon.h"
#include "common/logger.h"
#include "common/utils.h"
#include <windows.h>
#include <commctrl.h>

namespace clipx {

// Menu item IDs
constexpr int ID_TRAY_SHOW = 1001;
constexpr int ID_TRAY_AUTOSTART = 1002;
constexpr int ID_TRAY_SETTINGS = 1003;
constexpr int ID_TRAY_ABOUT = 1004;
constexpr int ID_TRAY_EXIT = 1005;

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    Shutdown();
}

bool TrayIcon::Initialize(HWND hwnd, UINT callbackMessage) {
    if (m_initialized) {
        return true;
    }

    m_hwnd = hwnd;
    m_callbackMessage = callbackMessage;

    // Create context menu
    m_menu = CreatePopupMenu();
    AddMenuItem(ID_TRAY_SHOW, "Show History");
    AddMenuSeparator();
    AddMenuItem(ID_TRAY_AUTOSTART, "Auto Start");
    AddMenuSeparator();
    AddMenuItem(ID_TRAY_SETTINGS, "Settings...");
    AddMenuItem(ID_TRAY_ABOUT, "About");
    AddMenuSeparator();
    AddMenuItem(ID_TRAY_EXIT, "Exit");

    // Initialize NOTIFYICONDATA
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = callbackMessage;
    m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(m_nid.szTip, L"ClipX");

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        LOG_ERROR("Failed to add tray icon");
        return false;
    }

    m_initialized = true;
    LOG_INFO("TrayIcon initialized");
    return true;
}

void TrayIcon::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_menu) {
        DestroyMenu(m_menu);
        m_menu = nullptr;
    }

    Shell_NotifyIconW(NIM_DELETE, &m_nid);

    if (m_nid.hIcon) {
        DestroyIcon(m_nid.hIcon);
        m_nid.hIcon = nullptr;
    }

    m_initialized = false;
    LOG_INFO("TrayIcon shutdown");
}

void TrayIcon::SetTooltip(const std::string& tooltip) {
    std::wstring wtooltip = utils::Utf8ToWide(tooltip);
    wcscpy_s(m_nid.szTip, wtooltip.c_str());
    m_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::SetIcon(HICON icon) {
    if (m_nid.hIcon) {
        DestroyIcon(m_nid.hIcon);
    }
    m_nid.hIcon = icon;
    m_nid.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::SetIconFromResource(int resourceId) {
    HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(resourceId));
    if (icon) {
        SetIcon(icon);
    }
}

void TrayIcon::ShowBalloon(const std::string& title, const std::string& message, DWORD flags) {
    std::wstring wtitle = utils::Utf8ToWide(title);
    std::wstring wmessage = utils::Utf8ToWide(message);

    m_nid.uFlags = NIF_INFO;
    m_nid.dwInfoFlags = flags;
    wcscpy_s(m_nid.szInfoTitle, wtitle.c_str());
    wcscpy_s(m_nid.szInfo, wmessage.c_str());

    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::ShowContextMenu(HWND hwnd, int x, int y) {
    if (!m_menu) {
        return;
    }

    // Required for the menu to close when clicking outside
    SetForegroundWindow(hwnd);

    TrackPopupMenu(m_menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, x, y, 0, hwnd, nullptr);
}

void TrayIcon::AddMenuItem(int id, const std::string& text, bool enabled) {
    if (!m_menu) {
        return;
    }

    std::wstring wtext = utils::Utf8ToWide(text);
    AppendMenuW(m_menu, MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), id, wtext.c_str());
}

void TrayIcon::AddMenuSeparator() {
    if (!m_menu) {
        return;
    }
    AppendMenuW(m_menu, MF_SEPARATOR, 0, nullptr);
}

void TrayIcon::SetMenuItemChecked(int id, bool checked) {
    if (!m_menu) {
        return;
    }
    CheckMenuItem(m_menu, id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
}

void TrayIcon::SetMenuCallback(MenuCallback callback) {
    m_menuCallback = std::move(callback);
}

} // namespace clipx
