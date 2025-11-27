// File: Attendance.hpp
// Description: Declares attendance domain models and repository abstraction.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace backend {

enum class AttendanceStatus { Present, Absent, Leave };

struct Student {
    std::string studentId;
    std::string name;
};

struct AttendanceRecord {
    std::string studentId;
    std::string dateIso;  // YYYY-MM-DD
    AttendanceStatus status;
};

class AttendanceRepository {
public:
    virtual ~AttendanceRepository() = default;

    virtual std::vector<Student> listStudents() = 0;
    virtual std::optional<Student> findStudentById(const std::string& studentId) = 0;
    virtual bool markAttendance(const AttendanceRecord& record) = 0;
};

// Factory used by WebServer. In the default build this returns an
// in-memory repository; if HAVE_MYSQL is defined and the runtime
// environment is configured correctly, it can be wired to MySQL.
std::unique_ptr<AttendanceRepository> createAttendanceRepository();

}  // namespace backend

