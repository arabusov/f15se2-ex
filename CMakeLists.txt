cmake_minimum_required(VERSION 3.22)
project(F15SE2-EX C CXX)

# Force C++20 or C++23
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL3 (Works seamlessly across all OS package managers)
find_package(SDL3 REQUIRED)

add_executable(f15se2 src/main.c)

# Link SDL3 natively
target_link_libraries(f15se2 PRIVATE SDL3::SDL3)
