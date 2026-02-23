#include "common/logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace clipx {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    Shutdown();
}

void Logger::Init(const std::string& path, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return;
    }

    m_level = level;

    // Create log directory if it doesn't exist
    std::filesystem::path filePath(path);
    std::filesystem::path dir = filePath.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    m_file.open(path, std::ios::out | std::ios::app);
    if (m_file.is_open()) {
        m_initialized = true;
        // Don't call Log() here as it would cause a deadlock
        // Write directly to the file
        std::string timeStr = GetCurrentTimeString();
        m_file << "[" << timeStr << "] [INFO] Logger initialized\n";
        m_file.flush();
    } else {
        std::cerr << "Failed to open log file: " << path << std::endl;
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        // Don't call Log() here as it would cause a deadlock
        std::string timeStr = GetCurrentTimeString();
        m_file << "[" << timeStr << "] [INFO] Logger shutting down\n";
        m_file.flush();
        m_file.close();
    }
    m_initialized = false;
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || level < m_level) {
        return;
    }

    std::string timeStr = GetCurrentTimeString();
    std::string levelStr = LevelToString(level);

    std::string logLine = "[" + timeStr + "] [" + levelStr + "] " + message + "\n";

    if (m_file.is_open()) {
        m_file << logLine;
        m_file.flush();
    }

    // Also output to console for debug builds
#ifdef _DEBUG
    std::cout << logLine;
#endif
}

void Logger::Debug(const std::string& message) {
    Log(LogLevel::Debug, message);
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::Info, message);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::Warning, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::Error, message);
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

LogLevel Logger::GetLevel() const {
    return m_level;
}

std::string Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

} // namespace clipx
