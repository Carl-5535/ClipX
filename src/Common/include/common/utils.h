#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace clipx {
namespace utils {

// Get current timestamp in milliseconds
int64_t GetCurrentTimestamp();

// Get current executable directory
std::string GetExecutableDir();

// Get application data directory (for config, db, logs)
std::string GetAppDataDir();

// Ensure directory exists, create if necessary
bool EnsureDirectory(const std::string& path);

// Compute SHA-256 hash of data
std::vector<uint8_t> ComputeHash(const std::vector<uint8_t>& data);

// Convert wide string to UTF-8
std::string WideToUtf8(const std::wstring& wstr);

// Convert UTF-8 to wide string
std::wstring Utf8ToWide(const std::string& str);

// Format timestamp to human-readable string
std::string FormatTimestamp(int64_t timestamp);

// Truncate string to specified length with ellipsis
std::string TruncateString(const std::string& str, size_t maxLength, const std::string& ellipsis = "...");

// Check if string contains only whitespace
bool IsWhitespace(const std::string& str);

// Trim whitespace from both ends
std::string Trim(const std::string& str);

// Convert string to lowercase
std::string ToLower(const std::string& str);

// Check if file exists
bool FileExists(const std::string& path);

// Get file size in bytes
int64_t GetFileSize(const std::string& path);

// Format size in bytes to human-readable string
std::string FormatSize(int64_t bytes);

// Generate a preview string from data
std::string GeneratePreview(const std::vector<uint8_t>& data, size_t maxLength = 100);

// Base64 encode
std::string Base64Encode(const std::vector<uint8_t>& data);

// Base64 decode
std::vector<uint8_t> Base64Decode(const std::string& encoded);

} // namespace utils
} // namespace clipx
