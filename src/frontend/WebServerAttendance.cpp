// File: WebServerAttendance.cpp
// Description: Attendance-related API handling separated from core WebServer routing.

#include "frontend/WebServer.hpp"

#include "backend/Attendance.hpp"

#include <chrono>
#include <ctime>

namespace {

std::string todayIsoDate() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buffer[16];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm) != 0) {
        return std::string(buffer);
    }
    return "1970-01-01";
}

}  // anonymous namespace

namespace frontend {

std::string WebServer::handleAttendanceMark(const std::string& body,
                                            std::string& contentType,
                                            int& statusCode) {
    contentType = "application/json";

    if (!m_attendanceRepo) {
        statusCode = 500;
        return R"({"success":false,"error":"Attendance repository not configured"})";
    }

    const std::string studentId = parseFormValue(body, "studentId");
    if (studentId.empty()) {
        statusCode = 400;
        return R"({"success":false,"error":"Missing studentId"})";
    }

    const std::string statusValue = parseFormValue(body, "status");
    if (statusValue.empty()) {
        statusCode = 400;
        return R"({"success":false,"error":"Missing status"})";
    }

    backend::AttendanceStatus status = backend::AttendanceStatus::Present;
    if (statusValue == "present") {
        status = backend::AttendanceStatus::Present;
    } else if (statusValue == "absent") {
        status = backend::AttendanceStatus::Absent;
    } else if (statusValue == "leave") {
        status = backend::AttendanceStatus::Leave;
    } else {
        statusCode = 400;
        return R"({"success":false,"error":"Invalid status"})";
    }

    // Optional date=YYYY-MM-DD, default to today.
    std::string dateIso = parseFormValue(body, "date");
    if (dateIso.empty()) {
        dateIso = todayIsoDate();
    }

    backend::AttendanceRecord record{studentId, dateIso, status};
    const bool ok = m_attendanceRepo->markAttendance(record);
    if (!ok) {
        statusCode = 500;
        return R"({"success":false,"error":"Failed to persist attendance record"})";
    }
    return R"({"success":true})";
}

}  // namespace frontend
