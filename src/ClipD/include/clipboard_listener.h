#pragma once

#include <string>
#include <functional>
#include "common/windows.h"
#include "common/types.h"

namespace clipx {

class ClipboardListener {
public:
    using OnClipboardChangeCallback = std::function<void(const ClipboardEntry&)>;

    ClipboardListener();
    ~ClipboardListener();

    bool Initialize(HWND hwnd);
    void SetOnClipboardChangeCallback(OnClipboardChangeCallback callback);
    void ProcessClipboardChange();

    // Get source application name
    static std::string GetSourceApplication();

private:
    ClipboardEntry ReadClipboard();
    std::vector<uint8_t> ReadTextFromClipboard();
    std::vector<uint8_t> ReadHtmlFromClipboard();
    std::vector<uint8_t> ReadImageFromClipboard();
    std::vector<uint8_t> ReadFilesFromClipboard();

    HWND m_hwnd = nullptr;
    bool m_initialized = false;
    OnClipboardChangeCallback m_onClipboardChange;
    int64_t m_lastClipboardSequence = 0;
};

} // namespace clipx
