#include "common/windows.h"
#include "common/utils.h"
#include "common/logger.h"
#include <shlobj.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <ctime>

namespace clipx {
namespace utils {

int64_t GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string GetExecutableDir() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    return exePath.parent_path().string();
}

std::string GetAppDataDir() {
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        std::wstring wpath(path);
        CoTaskMemFree(path);
        std::string result = WideToUtf8(wpath) + "\\ClipX";
        EnsureDirectory(result);
        return result;
    }
    return GetExecutableDir();
}

bool EnsureDirectory(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return std::filesystem::create_directories(path);
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

std::vector<uint8_t> ComputeHash(const std::vector<uint8_t>& data) {
    // Simple hash implementation (djb2 algorithm for now)
    // For production, consider using a proper SHA-256 library
    std::vector<uint8_t> hash(32, 0);

    uint64_t h1 = 5381;
    uint64_t h2 = 261;

    for (uint8_t byte : data) {
        h1 = ((h1 << 5) + h1) ^ byte;
        h2 = ((h2 << 6) + h2) ^ byte;
    }

    // Fill hash with computed values
    for (int i = 0; i < 8; i++) {
        hash[i] = static_cast<uint8_t>((h1 >> (i * 8)) & 0xFF);
        hash[i + 8] = static_cast<uint8_t>((h2 >> (i * 8)) & 0xFF);
        hash[i + 16] = static_cast<uint8_t>((h1 ^ h2) >> (i * 8)) & 0xFF;
        hash[i + 24] = static_cast<uint8_t>((h1 + h2) >> (i * 8)) & 0xFF;
    }

    return hash;
}

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()),
                                    nullptr, 0, nullptr, nullptr);
    if (size == 0) return "";

    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()),
                        &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";

    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()),
                                    nullptr, 0);
    if (size == 0) return L"";

    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()),
                        &result[0], size);
    return result;
}

std::string FormatTimestamp(int64_t timestamp) {
    auto time = std::chrono::system_clock::from_time_t(timestamp / 1000);
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - time);

    std::tm tm;
    time_t timeT = std::chrono::system_clock::to_time_t(time);
#ifdef _WIN32
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif

    if (diff.count() < 60) {
        return "Just now";
    } else if (diff.count() < 3600) {
        int mins = static_cast<int>(diff.count() / 60);
        return std::to_string(mins) + " minute" + (mins > 1 ? "s" : "") + " ago";
    } else if (diff.count() < 86400) {
        int hours = static_cast<int>(diff.count() / 3600);
        return std::to_string(hours) + " hour" + (hours > 1 ? "s" : "") + " ago";
    } else if (diff.count() < 604800) {
        int days = static_cast<int>(diff.count() / 86400);
        return std::to_string(days) + " day" + (days > 1 ? "s" : "") + " ago";
    } else {
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d");
        return oss.str();
    }
}

std::string TruncateString(const std::string& str, size_t maxLength, const std::string& ellipsis) {
    if (str.length() <= maxLength) {
        return str;
    }
    if (maxLength <= ellipsis.length()) {
        return ellipsis.substr(0, maxLength);
    }
    return str.substr(0, maxLength - ellipsis.length()) + ellipsis;
}

bool IsWhitespace(const std::string& str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
        return std::isspace(static_cast<unsigned char>(c));
    });
}

std::string Trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](char c) {
        return std::isspace(static_cast<unsigned char>(c));
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](char c) {
        return std::isspace(static_cast<unsigned char>(c));
    }).base();

    if (start >= end) return "";
    return std::string(start, end);
}

std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return result;
}

bool FileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

int64_t GetFileSize(const std::string& path) {
    try {
        return std::filesystem::file_size(path);
    } catch (...) {
        return -1;
    }
}

std::string FormatSize(int64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
    return oss.str();
}

std::string GeneratePreview(const std::vector<uint8_t>& data, size_t maxLength) {
    if (data.empty()) {
        return "[Empty]";
    }

    // Try to interpret as UTF-8 text
    std::string text(reinterpret_cast<const char*>(data.data()),
                     std::min(data.size(), maxLength * 2));

    // Check if it's valid UTF-8 text (not binary)
    bool isText = true;
    size_t checkBytes = std::min(text.size(), static_cast<size_t>(200));
    for (size_t i = 0; i < checkBytes; ) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        // ASCII control characters (except common whitespace)
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
            isText = false;
            break;
        }

        // Check UTF-8 multibyte sequences
        if (c >= 0x80) {
            // Multibyte UTF-8 character
            int expectedBytes;
            if ((c & 0xE0) == 0xC0) {
                expectedBytes = 2;  // 110xxxxx
            } else if ((c & 0xF0) == 0xE0) {
                expectedBytes = 3;  // 1110xxxx
            } else if ((c & 0xF8) == 0xF0) {
                expectedBytes = 4;  // 11110xxx
            } else {
                isText = false;  // Invalid UTF-8 start byte
                break;
            }

            // Check continuation bytes
            if (i + expectedBytes > text.size()) {
                isText = false;
                break;
            }
            for (int j = 1; j < expectedBytes; j++) {
                unsigned char next = static_cast<unsigned char>(text[i + j]);
                if ((next & 0xC0) != 0x80) {  // Not 10xxxxxx
                    isText = false;
                    break;
                }
            }
            if (!isText) break;
            i += expectedBytes;
        } else {
            i++;
        }
    }

    if (isText) {
        // Replace newlines/tabs with spaces, keep UTF-8 characters intact
        std::string preview;
        for (size_t i = 0; i < text.size(); ) {
            unsigned char c = static_cast<unsigned char>(text[i]);

            if (c == '\n' || c == '\r') {
                if (!preview.empty() && preview.back() != ' ') {
                    preview += ' ';
                }
                i++;
            } else if (c == '\t') {
                preview += ' ';
                i++;
            } else if (c < 0x80) {
                // ASCII character
                if (c >= 32 || c == ' ') {
                    preview += c;
                }
                i++;
            } else {
                // UTF-8 multibyte character - copy all bytes
                int charBytes = 1;
                if ((c & 0xE0) == 0xC0) charBytes = 2;
                else if ((c & 0xF0) == 0xE0) charBytes = 3;
                else if ((c & 0xF8) == 0xF0) charBytes = 4;

                for (int j = 0; j < charBytes && i + j < text.size(); j++) {
                    preview += text[i + j];
                }
                i += charBytes;
            }
        }
        return TruncateString(Trim(preview), maxLength);
    }

    // Binary data - show hex preview
    std::ostringstream oss;
    oss << "[Binary " << data.size() << " bytes]";
    return oss.str();
}

static const char* base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        result += base64_chars[(triple >> 18) & 0x3F];
        result += base64_chars[(triple >> 12) & 0x3F];
        result += (i - 2 <= data.size()) ? base64_chars[(triple >> 6) & 0x3F] : '=';
        result += (i - 1 <= data.size()) ? base64_chars[triple & 0x3F] : '=';
    }

    return result;
}

std::vector<uint8_t> Base64Decode(const std::string& encoded) {
    static int decodeTable[256];
    static bool initialized = false;

    if (!initialized) {
        std::fill(decodeTable, decodeTable + 256, -1);
        for (int i = 0; i < 64; i++) {
            decodeTable[static_cast<unsigned char>(base64_chars[i])] = i;
        }
        initialized = true;
    }

    std::vector<uint8_t> result;
    result.reserve((encoded.size() / 4) * 3);

    size_t i = 0;
    while (i < encoded.size()) {
        int sextet_a = (i < encoded.size() && encoded[i] != '=') ?
                       decodeTable[static_cast<unsigned char>(encoded[i++])] : 0;
        int sextet_b = (i < encoded.size() && encoded[i] != '=') ?
                       decodeTable[static_cast<unsigned char>(encoded[i++])] : 0;
        int sextet_c = (i < encoded.size() && encoded[i] != '=') ?
                       decodeTable[static_cast<unsigned char>(encoded[i++])] : 0;
        int sextet_d = (i < encoded.size() && encoded[i] != '=') ?
                       decodeTable[static_cast<unsigned char>(encoded[i++])] : 0;

        uint32_t triple = (sextet_a << 18) | (sextet_b << 12) | (sextet_c << 6) | sextet_d;

        if (sextet_a != -1 && sextet_b != -1) {
            result.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
            if (encoded[i - 2] != '=') {
                result.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
            }
            if (encoded[i - 1] != '=') {
                result.push_back(static_cast<uint8_t>(triple & 0xFF));
            }
        }
    }

    return result;
}

} // namespace utils
} // namespace clipx
