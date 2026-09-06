cmake_minimum_required(VERSION 3.20)
project(moore_stream_processor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)

# Build the library with the same compiler and C++ standard as our model.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/systemc/CMakeLists.txt")
    message(FATAL_ERROR "SystemC source missing. Run scripts/setup.ps1 first.")
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(BUILD_SOURCE_DOCUMENTATION OFF CACHE BOOL "Skip dependency documentation" FORCE)
add_subdirectory(third_party/systemc EXCLUDE_FROM_ALL)

add_executable(day01_basics src/day01_basics.cpp)
target_link_libraries(day01_basics PRIVATE SystemC::systemc)
if(MSVC)
    target_compile_options(day01_basics PRIVATE /W4)
else()
    target_compile_options(day01_basics PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME day01_signal_timing COMMAND day01_basics)
set_tests_properties(day01_signal_timing PROPERTIES TIMEOUT 10)
