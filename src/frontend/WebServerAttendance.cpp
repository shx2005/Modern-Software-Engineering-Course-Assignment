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

    const std::string studentId = parseDirectory(body);  // reuse to parse key=value
    if (studentId.empty()) {
        statusCode = 400;
        return R"({"success":false,"error":"Missing studentId"})";
    }

    const std::size_t statusPos = body.find("status=");
    if (statusPos == std::string::npos) {
        statusCode = 400;
        return R"({"success":false,"error":"Missing status"})";
    }
    std::size_t statusEnd = body.find('&', statusPos + 7);
    std::string rawStatus =
        statusEnd == std::string::npos ? body.substr(statusPos + 7)
                                       : body.substr(statusPos + 7, statusEnd - (statusPos + 7));
    const std::string statusValue = decodeFormValue(rawStatus);

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
    std::string dateIso;
    const std::size_t datePos = body.find("date=");
    if (datePos != std::string::npos) {
        std::size_t dateEnd = body.find('&', datePos + 5);
        const std::string rawDate =
            dateEnd == std::string::npos ? body.substr(datePos + 5)
                                         : body.substr(datePos + 5, dateEnd - (datePos + 5));
        dateIso = decodeFormValue(rawDate);
    }
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
