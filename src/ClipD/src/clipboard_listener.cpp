#include "common/windows.h"
#include "clipboard_listener.h"
#include "common/logger.h"
#include "common/utils.h"
#include <vector>
#include <algorithm>

// Define DROPFILES locally if not available
#ifndef DROPFILES
typedef struct _DROPFILES {
    DWORD pFiles;
    POINT pt;
    BOOL fNC;
    BOOL fWide;
} DROPFILES, *LPDROPFILES;
#endif

namespace clipx {

ClipboardListener::ClipboardListener() = default;

ClipboardListener::~ClipboardListener() {
    if (m_initialized && m_hwnd) {
        RemoveClipboardFormatListener(m_hwnd);
    }
}

bool ClipboardListener::Initialize(HWND hwnd) {
    if (m_initialized) {
        return true;
    }

    m_hwnd = hwnd;
    if (!AddClipboardFormatListener(hwnd)) {
        LOG_ERROR("Failed to add clipboard format listener");
        return false;
    }

    m_initialized = true;
    m_lastClipboardSequence = GetClipboardSequenceNumber();
    LOG_INFO("ClipboardListener initialized");
    return true;
}

void ClipboardListener::SetOnClipboardChangeCallback(OnClipboardChangeCallback callback) {
    m_onClipboardChange = std::move(callback);
}

void ClipboardListener::IgnoreNextChange() {
    m_ignoreNextChange = true;
    LOG_DEBUG("Ignoring next clipboard change");
}

void ClipboardListener::ProcessClipboardChange() {
    if (!m_initialized) return;

    // Check if clipboard sequence has changed
    int64_t currentSequence = GetClipboardSequenceNumber();
    if (currentSequence == m_lastClipboardSequence) {
        return;
    }
    m_lastClipboardSequence = currentSequence;

    // Check if we should ignore this change (e.g., when we set clipboard ourselves)
    if (m_ignoreNextChange) {
        m_ignoreNextChange = false;
        LOG_DEBUG("Ignored clipboard change as requested");
        return;
    }

    LOG_DEBUG("Clipboard changed, sequence: " + std::to_string(currentSequence));

    ClipboardEntry entry = ReadClipboard();
    if (!entry.data.empty() || !entry.preview.empty()) {
        if (m_onClipboardChange) {
            m_onClipboardChange(entry);
        }
    }
}

std::string ClipboardListener::GetSourceApplication() {
    HWND hwndOwner = GetClipboardOwner();
    if (!hwndOwner) {
        return "";
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwndOwner, &processId);
    if (!processId) {
        return "";
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) {
        return "";
    }

    wchar_t processName[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
        CloseHandle(hProcess);
        std::wstring wname(processName);
        size_t pos = wname.find_last_of(L"\\");
        if (pos != std::wstring::npos) {
            return utils::WideToUtf8(wname.substr(pos + 1));
        }
        return utils::WideToUtf8(wname);
    }

    CloseHandle(hProcess);
    return "";
}

ClipboardEntry ClipboardListener::ReadClipboard() {
    ClipboardEntry entry;
    entry.timestamp = utils::GetCurrentTimestamp();
    entry.sourceApp = GetSourceApplication();

    if (!OpenClipboard(nullptr)) {
        LOG_ERROR("Failed to open clipboard");
        return entry;
    }

    // Priority order: Text, HTML, Image, Files
    UINT format = 0;
    UINT priorityFormats[] = {
        CF_UNICODETEXT,
        RegisterClipboardFormatW(L"HTML Format"),
        CF_DIB,
        CF_HDROP
    };

    // Find the best available format
    for (UINT pf : priorityFormats) {
        if (IsClipboardFormatAvailable(pf)) {
            format = pf;
            break;
        }
    }

    if (format == 0) {
        // Try to find any available format
        format = EnumClipboardFormats(0);
    }

    if (format == 0) {
        CloseClipboard();
        LOG_DEBUG("No supported clipboard format available");
        return entry;
    }

    // Read based on format
    if (format == CF_UNICODETEXT) {
        entry.type = ClipboardDataType::Text;
        entry.data = ReadTextFromClipboard();
        entry.preview = utils::GeneratePreview(entry.data);
    } else if (format == RegisterClipboardFormatW(L"HTML Format")) {
        entry.type = ClipboardDataType::Html;
        entry.data = ReadHtmlFromClipboard();
        // Extract text from HTML for preview
        std::string html(entry.data.begin(), entry.data.end());
        size_t start = html.find("<body");
        if (start != std::string::npos) {
            start = html.find('>', start);
            size_t end = html.find("</body>", start);
            if (start != std::string::npos && end != std::string::npos) {
                entry.preview = html.substr(start + 1, end - start - 1);
                // Remove HTML tags
                std::string cleaned;
                bool inTag = false;
                for (char c : entry.preview) {
                    if (c == '<') inTag = true;
                    else if (c == '>') inTag = false;
                    else if (!inTag) cleaned += c;
                }
                entry.preview = utils::TruncateString(utils::Trim(cleaned), 100);
            }
        }
        if (entry.preview.empty()) {
            entry.preview = "[HTML content]";
        }
    } else if (format == CF_DIB) {
        entry.type = ClipboardDataType::Image;
        entry.data = ReadImageFromClipboard();
        entry.preview = "[Image: " + utils::FormatSize(entry.data.size()) + "]";
    } else if (format == CF_HDROP) {
        entry.type = ClipboardDataType::Files;
        entry.data = ReadFilesFromClipboard();
        entry.preview = utils::GeneratePreview(entry.data);
    } else {
        // Generic format - try to read as text
        HANDLE hData = GetClipboardData(format);
        if (hData) {
            void* ptr = GlobalLock(hData);
            SIZE_T size = GlobalSize(hData);
            if (ptr && size > 0) {
                entry.type = ClipboardDataType::Custom;
                entry.data.assign(static_cast<uint8_t*>(ptr),
                                  static_cast<uint8_t*>(ptr) + size);
                entry.preview = "[Custom format: " + std::to_string(format) + "]";
            }
            GlobalUnlock(hData);
        }
    }

    CloseClipboard();
    return entry;
}

std::vector<uint8_t> ClipboardListener::ReadTextFromClipboard() {
    std::vector<uint8_t> result;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        return result;
    }

    wchar_t* ptr = static_cast<wchar_t*>(GlobalLock(hData));
    if (!ptr) {
        return result;
    }

    std::wstring wtext(ptr);
    GlobalUnlock(hData);

    std::string utf8 = utils::WideToUtf8(wtext);
    result.assign(utf8.begin(), utf8.end());

    return result;
}

std::vector<uint8_t> ClipboardListener::ReadHtmlFromClipboard() {
    std::vector<uint8_t> result;

    UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    HANDLE hData = GetClipboardData(htmlFormat);
    if (!hData) {
        return result;
    }

    char* ptr = static_cast<char*>(GlobalLock(hData));
    if (!ptr) {
        return result;
    }

    SIZE_T size = GlobalSize(hData);
    result.assign(ptr, ptr + size);

    GlobalUnlock(hData);
    return result;
}

std::vector<uint8_t> ClipboardListener::ReadImageFromClipboard() {
    std::vector<uint8_t> result;

    HANDLE hData = GetClipboardData(CF_DIB);
    if (!hData) {
        return result;
    }

    void* ptr = GlobalLock(hData);
    if (!ptr) {
        return result;
    }

    SIZE_T size = GlobalSize(hData);

    // Convert DIB to a simple format (just store the raw DIB for now)
    // In production, you'd want to convert to PNG for better compression
    result.assign(static_cast<uint8_t*>(ptr),
                  static_cast<uint8_t*>(ptr) + size);

    GlobalUnlock(hData);
    return result;
}

std::vector<uint8_t> ClipboardListener::ReadFilesFromClipboard() {
    std::vector<uint8_t> result;

    HANDLE hData = GetClipboardData(CF_HDROP);
    if (!hData) {
        return result;
    }

    // Get file count
    UINT fileCount = DragQueryFileW(static_cast<HDROP>(hData), 0xFFFFFFFF, nullptr, 0);

    std::string fileList;
    for (UINT i = 0; i < fileCount; i++) {
        wchar_t buffer[MAX_PATH];
        DragQueryFileW(static_cast<HDROP>(hData), i, buffer, MAX_PATH);
        std::string path = utils::WideToUtf8(buffer);
        fileList += path + '\0';
    }

    result.assign(fileList.begin(), fileList.end());
    return result;
}

} // namespace clipx
