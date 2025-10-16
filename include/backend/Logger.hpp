// File: Logger.hpp
// Description: Provides a simple thread-safe logging facility that writes
//              messages to both a log file and the console.

#pragma once

#include <mutex>
#include <string>

namespace backend {

class Logger {
public:
    static Logger& instance();

    void initialize(const std::string& logFilePath);
    void log(const std::string& message);

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex m_mutex;
    std::string m_logFilePath;
    bool m_initialized{false};
    struct Impl;
    Impl* m_impl{nullptr};
};

}  // namespace backend
