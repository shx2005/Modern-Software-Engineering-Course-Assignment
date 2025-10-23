## File: Makefile
## Description: Build automation script compiling backend and frontend using clang++.

CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude

BACKEND_SRCS := $(wildcard src/backend/*.cpp)
FRONTEND_SRCS := $(wildcard src/frontend/*.cpp)
SRCS := $(BACKEND_SRCS) $(FRONTEND_SRCS)
OBJS := $(SRCS:.cpp=.o)

TARGET := bin/tank_red_envelope

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean run

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
