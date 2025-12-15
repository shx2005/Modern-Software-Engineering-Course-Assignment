// File: Attendance.cpp
// Description: Provides a simple attendance repository implementation.
//              By default this is an in-memory store suitable for
//              development and demonstration. A MySQL-backed version
//              can be enabled by defining HAVE_MYSQL and wiring the
//              connection details in createAttendanceRepository().

#include "backend/Attendance.hpp"
#include "backend/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>

#if defined(HAVE_MYSQL)
#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#elif __has_include(<mysql.h>)
#include <mysql.h>
#else
#error "HAVE_MYSQL is defined but MySQL headers are not available."
#endif

#if __has_include(<mysql/errmsg.h>)
#include <mysql/errmsg.h>
#elif __has_include(<errmsg.h>)
#include <errmsg.h>
#endif

#ifndef CR_SERVER_GONE_ERROR
#define CR_SERVER_GONE_ERROR 2006
#endif
#ifndef CR_SERVER_LOST
#define CR_SERVER_LOST 2013
#endif
#endif

namespace backend {

namespace {

class InMemoryAttendanceRepository : public AttendanceRepository {
public:
    InMemoryAttendanceRepository() {
        // Seed with a few sample students; in a MySQL-backed build,
        // these records should instead来自数据库初始化。
        m_students.push_back(Student{"2023xxxxxxxx1", "haoxiang"});
        m_students.push_back(Student{"2023xxxxxxxx2", "yuyang"});
    }

    std::vector<Student> listStudents() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_students;
    }

    std::optional<Student> findStudentById(const std::string& studentId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = std::find_if(
            m_students.begin(), m_students.end(),
            [&](const Student& s) { return s.studentId == studentId; });
        if (it == m_students.end()) {
            return std::nullopt;
        }
        return *it;
    }

    bool markAttendance(const AttendanceRecord& record) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_records.push_back(record);
        return true;
    }

private:
    std::vector<Student> m_students;
    std::vector<AttendanceRecord> m_records;
    std::mutex m_mutex;
};

#if defined(HAVE_MYSQL)

AttendanceStatus statusFromString(const std::string& value) {
    if (value == "present") {
        return AttendanceStatus::Present;
    }
    if (value == "absent") {
        return AttendanceStatus::Absent;
    }
    if (value == "leave") {
        return AttendanceStatus::Leave;
    }
    return AttendanceStatus::Present;
}

std::string statusToString(AttendanceStatus status) {
    switch (status) {
        case AttendanceStatus::Present:
            return "present";
        case AttendanceStatus::Absent:
            return "absent";
        case AttendanceStatus::Leave:
            return "leave";
    }
    return "present";
}

class MySqlAttendanceRepository : public AttendanceRepository {
public:
    MySqlAttendanceRepository()
        : m_conn(mysql_init(nullptr)) {
        if (!m_conn) {
            throw std::runtime_error("Failed to initialize MySQL handle.");
        }

        const char* host = std::getenv("ATTENDANCE_DB_HOST");
        const char* user = std::getenv("ATTENDANCE_DB_USER");
        const char* password = std::getenv("ATTENDANCE_DB_PASSWORD");
        if (!password) {
            // Common MySQL env var names used by CLI/tools.
            password = std::getenv("MYSQL_PWD");
        }
        if (!password) {
            // Align with Makefile variable naming for convenience.
            password = std::getenv("DB_PASSWORD");
        }
        const char* db = std::getenv("ATTENDANCE_DB_NAME");
        const char* portStr = std::getenv("ATTENDANCE_DB_PORT");

        if (!host) {
            host = "localhost";
        }
        if (!user) {
            user = "root";
        }
        if (!password) {
            password = "";
        }
        if (!db) {
            db = "attendance_db";
        }

        unsigned int port = 3306U;
        if (portStr != nullptr) {
            const int parsed = std::atoi(portStr);
            if (parsed > 0) {
                port = static_cast<unsigned int>(parsed);
            }
        }

        if (!mysql_real_connect(m_conn, host, user, password, db, port, nullptr, 0)) {
            std::string message = "Failed to connect MySQL: ";
            message += mysql_error(m_conn);
            if (std::string(message).find("Access denied") != std::string::npos) {
                message += " (Hint: set ATTENDANCE_DB_PASSWORD / MYSQL_PWD env var)";
            }
            mysql_close(m_conn);
            m_conn = nullptr;
            throw std::runtime_error(message);
        }
    }

    ~MySqlAttendanceRepository() override {
        if (m_conn) {
            mysql_close(m_conn);
            m_conn = nullptr;
        }
    }

    std::vector<Student> listStudents() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Student> result;
        ensureConnectedOrQuit("listStudents");

        const char* query = "SELECT student_id, name FROM students ORDER BY student_id";
        if (mysql_query(m_conn, query) != 0) {
            handleDbErrorOrQuit("listStudents mysql_query");
            return result;
        }

        MYSQL_RES* res = mysql_store_result(m_conn);
        if (!res) {
            handleDbErrorOrQuit("listStudents mysql_store_result");
            return result;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            Student s;
            if (row[0]) {
                s.studentId = row[0];
            }
            if (row[1]) {
                s.name = row[1];
            }
            result.push_back(std::move(s));
        }
        mysql_free_result(res);
        return result;
    }

    std::optional<Student> findStudentById(const std::string& studentId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        ensureConnectedOrQuit("findStudentById");

        std::string query = "SELECT student_id, name FROM students WHERE student_id = '";
        query += escape(studentId);
        query += "' LIMIT 1";

        if (mysql_query(m_conn, query.c_str()) != 0) {
            handleDbErrorOrQuit("findStudentById mysql_query");
            return std::nullopt;
        }

        MYSQL_RES* res = mysql_store_result(m_conn);
        if (!res) {
            handleDbErrorOrQuit("findStudentById mysql_store_result");
            return std::nullopt;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row) {
            mysql_free_result(res);
            return std::nullopt;
        }

        Student s;
        if (row[0]) {
            s.studentId = row[0];
        }
        if (row[1]) {
            s.name = row[1];
        }
        mysql_free_result(res);
        return s;
    }

    bool markAttendance(const AttendanceRecord& record) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        ensureConnectedOrQuit("markAttendance");

        std::ostringstream oss;
        oss << "INSERT INTO attendance(student_id, `date`, status) VALUES('"
            << escape(record.studentId) << "','"
            << escape(record.dateIso) << "','"
            << escape(statusToString(record.status)) << "')";

        const std::string sql = oss.str();
        if (mysql_query(m_conn, sql.c_str()) != 0) {
            handleDbErrorOrQuit("markAttendance mysql_query");
            return false;
        }
        return true;
    }

private:
    void ensureConnectedOrQuit(const char* action) {
        if (m_conn) {
            return;
        }
        quitNow(std::string("MySQL connection missing during ") + action + ".");
    }

    static bool isDisconnectError(unsigned int err) {
        return err == CR_SERVER_GONE_ERROR || err == CR_SERVER_LOST || err == 2006U || err == 2013U;
    }

    void handleDbErrorOrQuit(const char* action) {
        if (!m_conn) {
            quitNow(std::string("MySQL connection missing during ") + action + ".");
        }
        const unsigned int err = mysql_errno(m_conn);
        if (isDisconnectError(err)) {
            std::string message = "MySQL disconnected during ";
            message += action;
            message += ": ";
            message += mysql_error(m_conn);
            quitNow(message);
        }
    }

    [[noreturn]] static void quitNow(const std::string& message) {
        backend::Logger::instance().log(std::string("FATAL: ") + message);
        std::cerr << "FATAL: " << message << "\n";
        std::cerr.flush();
        std::quick_exit(EXIT_FAILURE);
    }

    std::string escape(const std::string& value) {
        if (!m_conn) {
            return value;
        }
        std::string out;
        out.resize(value.size() * 2 + 1);
        const unsigned long len =
            mysql_real_escape_string(m_conn, &out[0], value.c_str(),
                                     static_cast<unsigned long>(value.size()));
        out.resize(len);
        return out;
    }

    MYSQL* m_conn;
    std::mutex m_mutex;
};

#endif  // HAVE_MYSQL

}  // namespace

std::unique_ptr<AttendanceRepository> createAttendanceRepository() {
#if defined(HAVE_MYSQL)
    // 当启用 HAVE_MYSQL 时，优先使用 MySQL 版本。
    // 如果连接失败，将在 MySqlAttendanceRepository 构造函数中抛出异常，
    // 由上层统一处理，而不再静默回退到内存实现。
    return std::make_unique<MySqlAttendanceRepository>();
#else
    // 点名功能不允许静默回退到内存实现：若未启用 MySQL，直接失败退出（由上层终止进程）。
    throw std::runtime_error(
        "Attendance requires a database, but this build is missing MySQL support. "
        "Rebuild with WITH_MYSQL=1 (or define HAVE_MYSQL and link mysqlclient).");
#endif
}

}  // namespace backend
