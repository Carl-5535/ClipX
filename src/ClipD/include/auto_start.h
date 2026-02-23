#pragma once

#include <string>

namespace clipx {

class AutoStart {
public:
    static bool IsEnabled();
    static bool Enable(bool enable);
    static bool IsRegistryKeyExists();

private:
    static constexpr const char* REGISTRY_KEY = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const char* APP_NAME = "ClipX";
};

} // namespace clipx
