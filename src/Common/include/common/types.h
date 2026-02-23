#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace clipx {

// Clipboard data types
enum class ClipboardDataType : int32_t {
    Text = 1,
    Html = 2,
    Rtf = 3,
    Image = 4,
    Files = 5,
    Custom = 99
};

// Clipboard entry structure
struct ClipboardEntry {
    int64_t id = 0;
    int64_t timestamp = 0;
    ClipboardDataType type = ClipboardDataType::Text;
    std::vector<uint8_t> data;
    std::string preview;
    std::string sourceApp;
    int32_t copyCount = 1;
    bool isFavorited = false;
    bool isTagged = false;
    std::vector<std::string> tags;

    ClipboardEntry() = default;

    ClipboardEntry(const ClipboardEntry&) = default;
    ClipboardEntry& operator=(const ClipboardEntry&) = default;
    ClipboardEntry(ClipboardEntry&&) = default;
    ClipboardEntry& operator=(ClipboardEntry&&) = default;
};

// Query options for history retrieval
struct QueryOptions {
    int limit = 100;
    int offset = 0;
    std::optional<ClipboardDataType> filterType;
    bool favoritesOnly = false;
    enum class SortOrder {
        LatestFirst,
        OldestFirst,
        MostCopied,
        Alphabetical
    } sortOrder = SortOrder::LatestFirst;
};

// Database statistics
struct DatabaseStats {
    size_t totalCount = 0;
    size_t textSize = 0;
    size_t imageSize = 0;
    size_t totalSize = 0;
};

// Utility functions for type conversion
inline std::string ClipboardDataTypeToString(ClipboardDataType type) {
    switch (type) {
        case ClipboardDataType::Text: return "Text";
        case ClipboardDataType::Html: return "HTML";
        case ClipboardDataType::Rtf: return "RTF";
        case ClipboardDataType::Image: return "Image";
        case ClipboardDataType::Files: return "Files";
        case ClipboardDataType::Custom: return "Custom";
        default: return "Unknown";
    }
}

inline ClipboardDataType StringToClipboardDataType(const std::string& str) {
    if (str == "Text") return ClipboardDataType::Text;
    if (str == "HTML") return ClipboardDataType::Html;
    if (str == "RTF") return ClipboardDataType::Rtf;
    if (str == "Image") return ClipboardDataType::Image;
    if (str == "Files") return ClipboardDataType::Files;
    return ClipboardDataType::Custom;
}

// Get current timestamp in milliseconds
inline int64_t GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace clipx
