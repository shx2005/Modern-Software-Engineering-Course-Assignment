-- File: sql/attendance_init.sql
-- Description: Initialize MySQL schema and seed data for attendance feature.

CREATE DATABASE IF NOT EXISTS attendance_db
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE attendance_db;

CREATE TABLE IF NOT EXISTS students (
  student_id VARCHAR(32) PRIMARY KEY,
  name       VARCHAR(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS attendance (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  student_id VARCHAR(32) NOT NULL,
  date       DATE        NOT NULL,
  status     VARCHAR(16) NOT NULL,
  CONSTRAINT fk_attendance_student
    FOREIGN KEY (student_id) REFERENCES students(student_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO students (student_id, name) VALUES
  ('2023xxxxxxxx1', 'xxx1'),
  ('2023xxxxxxxx2', 'xxx2');

