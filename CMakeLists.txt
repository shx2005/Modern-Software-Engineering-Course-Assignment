cmake_minimum_required(VERSION 3.10)
project(TankRedEnvelope)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Include directories
include_directories(include)

# Collect source files
file(GLOB_RECURSE SOURCES
    "src/backend/*.cpp"
    "src/frontend/*.cpp"
)

# Create executable
add_executable(tank_red_envelope ${SOURCES})

# Set output directory
set_target_properties(tank_red_envelope PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

