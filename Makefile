## File: Makefile
## Description: Build automation script compiling backend and frontend using clang++.

CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude

BACKEND_SRCS := $(wildcard src/backend/*.cpp)
FRONTEND_SRCS := $(wildcard src/frontend/*.cpp)
SRCS := $(BACKEND_SRCS) $(FRONTEND_SRCS)
OBJS := $(SRCS:.cpp=.o)

TARGET := bin/tank_red_envelope

.PHONY: all clean run db-init

# MySQL CLI configuration for attendance feature.
# 使用前请根据本机环境修改 DB_USER/DB_PASSWORD 等变量。
DB_HOST ?= localhost
DB_PORT ?= 3306
DB_USER ?= root
DB_PASSWORD ?=
DB_NAME ?= attendance_db

DB_FLAGS := -h$(DB_HOST) -P$(DB_PORT) -u$(DB_USER)
ifneq ($(DB_PASSWORD),)
DB_FLAGS += -p$(DB_PASSWORD)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

db-init:
	@echo "Initializing MySQL attendance schema in database '$(DB_NAME)'..."
	@mysql $(DB_FLAGS) < sql/attendance_init.sql

run: $(TARGET)
	@$(MAKE) db-init
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
