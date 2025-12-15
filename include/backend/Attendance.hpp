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

// Factory used by WebServer. In a MySQL-enabled build (HAVE_MYSQL),
// attendance data is persisted in MySQL; if MySQL support is missing,
// the factory will fail fast instead of silently falling back to memory.
std::unique_ptr<AttendanceRepository> createAttendanceRepository();

}  // namespace backend
