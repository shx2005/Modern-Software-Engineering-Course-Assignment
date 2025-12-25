## File: Makefile
## Description: Build automation script compiling backend and frontend using clang++.

CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
LDLIBS := -lcurl

.DEFAULT_GOAL := all

# Rebuild objects when feature flags change (e.g. WITH_MYSQL).
BUILD_DIR := build
CONFIG_STAMP := $(BUILD_DIR)/config.$(WITH_MYSQL).stamp

$(CONFIG_STAMP): FORCE
	@mkdir -p $(BUILD_DIR)
	@echo 'CXXFLAGS=$(CXXFLAGS)' > $(CONFIG_STAMP).tmp
	@echo 'LDLIBS=$(LDLIBS)' >> $(CONFIG_STAMP).tmp
	@if [ ! -f $(CONFIG_STAMP) ] || ! cmp -s $(CONFIG_STAMP) $(CONFIG_STAMP).tmp; then \
		mv $(CONFIG_STAMP).tmp $(CONFIG_STAMP); \
	else \
		rm -f $(CONFIG_STAMP).tmp; \
	fi

FORCE:

# Attendance feature: build with MySQL client by default (no in-memory fallback).
WITH_MYSQL ?= 1
MYSQL_CONFIG ?= mysql_config

ifeq ($(WITH_MYSQL),1)
MYSQL_CFLAGS := $(shell $(MYSQL_CONFIG) --cflags 2>/dev/null)
MYSQL_LIBS := $(shell $(MYSQL_CONFIG) --libs 2>/dev/null)
ifeq ($(strip $(MYSQL_LIBS)),)
MYSQL_CFLAGS := $(shell pkg-config --cflags mysqlclient 2>/dev/null)
MYSQL_LIBS := $(shell pkg-config --libs mysqlclient 2>/dev/null)
endif
ifeq ($(strip $(MYSQL_LIBS)),)
$(error MySQL client dev libs not found. Install MySQL/MariaDB client dev package (mysql_config or pkg-config mysqlclient), or build with WITH_MYSQL=0)
endif
# Homebrew on macOS often keeps dependencies (e.g. zstd/ssl) in a shared lib dir.
ifneq ($(wildcard /opt/homebrew/lib),)
MYSQL_LIBS := -L/opt/homebrew/lib $(MYSQL_LIBS)
endif
ifneq ($(wildcard /usr/local/lib),)
MYSQL_LIBS := -L/usr/local/lib $(MYSQL_LIBS)
endif
CXXFLAGS += -DHAVE_MYSQL $(MYSQL_CFLAGS)
LDLIBS += $(MYSQL_LIBS)
endif

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
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJS): $(CONFIG_STAMP)

db-init:
	@echo "Initializing MySQL attendance schema in database '$(DB_NAME)'..."
	@sed 's/`attendance_db`/`$(DB_NAME)`/g' sql/attendance_init.sql | mysql $(DB_FLAGS)

run: $(TARGET)
	@$(MAKE) db-init
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
