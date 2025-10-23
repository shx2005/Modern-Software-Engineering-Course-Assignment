// File: Logger.cpp
// Description: Implements the logging utility with timestamped output mirrored
//              to both file and console streams.

#include "backend/Logger.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace backend {

struct Logger::Impl {
    std::ofstream logStream;
};

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> guard(m_mutex);
    delete m_impl;
    m_impl = nullptr;
}

void Logger::initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_impl) {
        m_impl = new Impl();
    }

    const std::filesystem::path targetPath(logFilePath);
    if (const auto parent = targetPath.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    m_impl->logStream.close();
    m_impl->logStream.open(targetPath, std::ios::out | std::ios::trunc);
    if (!m_impl->logStream.is_open()) {
        throw std::runtime_error("Failed to open log file: " + targetPath.string());
    }

    m_logFilePath = targetPath.string();
    m_initialized = true;
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_initialized || !m_impl) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << " | " << message;
    const std::string line = oss.str();

    m_impl->logStream << line << '\n';
    m_impl->logStream.flush();
    std::cout << line << std::endl;
}

}  // namespace backend
