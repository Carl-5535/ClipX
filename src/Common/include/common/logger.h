#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace clipx {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3
};

class Logger {
public:
    static Logger& Instance();

    void Init(const std::string& path, LogLevel level = LogLevel::Info);
    void Shutdown();

    void Log(LogLevel level, const std::string& message);
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);

    void SetLevel(LogLevel level);
    LogLevel GetLevel() const;

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string LevelToString(LogLevel level);
    std::string GetCurrentTimeString();

    std::ofstream m_file;
    LogLevel m_level = LogLevel::Info;
    std::mutex m_mutex;
    bool m_initialized = false;
};

// Convenience macros
#define LOG_DEBUG(msg) clipx::Logger::Instance().Debug(msg)
#define LOG_INFO(msg) clipx::Logger::Instance().Info(msg)
#define LOG_WARN(msg) clipx::Logger::Instance().Warning(msg)
#define LOG_ERROR(msg) clipx::Logger::Instance().Error(msg)

} // namespace clipx
