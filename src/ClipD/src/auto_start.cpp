#include "auto_start.h"
#include "common/logger.h"
#include "common/utils.h"
#include <windows.h>

namespace clipx {

bool AutoStart::IsEnabled() {
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        REGISTRY_KEY,
        0,
        KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        return false;
    }

    result = RegQueryValueExA(hKey, APP_NAME, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
}

bool AutoStart::Enable(bool enable) {
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        REGISTRY_KEY,
        0,
        KEY_WRITE,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        LOG_ERROR("Failed to open registry key: " + std::to_string(result));
        return false;
    }

    if (enable) {
        // Get current executable path
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::string path = utils::WideToUtf8(exePath);

        // Add quotes around the path
        std::string quotedPath = "\"" + path + "\"";

        result = RegSetValueExA(
            hKey,
            APP_NAME,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(quotedPath.c_str()),
            static_cast<DWORD>(quotedPath.length() + 1)
        );

        if (result != ERROR_SUCCESS) {
            LOG_ERROR("Failed to set registry value: " + std::to_string(result));
            RegCloseKey(hKey);
            return false;
        }

        LOG_INFO("Auto-start enabled");
    } else {
        result = RegDeleteValueA(hKey, APP_NAME);

        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
            LOG_ERROR("Failed to delete registry value: " + std::to_string(result));
            RegCloseKey(hKey);
            return false;
        }

        LOG_INFO("Auto-start disabled");
    }

    RegCloseKey(hKey);
    return true;
}

bool AutoStart::IsRegistryKeyExists() {
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        REGISTRY_KEY,
        0,
        KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        return false;
    }

    RegCloseKey(hKey);
    return true;
}

} // namespace clipx
