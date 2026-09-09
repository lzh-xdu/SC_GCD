cmake_minimum_required(VERSION 3.20)
project(moore_stream_processor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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

add_library(model_support STATIC
    src/model/functional/operands.cpp
    src/model/timing/gcd_plan.cpp
    src/model/timing/clock.cpp
    src/model/instrumentation/event_recorder.cpp
    src/model/instrumentation/statistics.cpp
    src/common/task_statistics.cpp)
target_link_libraries(model_support PUBLIC SystemC::systemc)
if(MSVC)
    target_compile_options(model_support PRIVATE /W4)
else()
    target_compile_options(model_support PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(stage1_gcd
    src/stage1/main.cpp
    src/stage1/types.cpp
    src/stage1/parser.cpp
    src/stage1/transform.cpp
    src/stage1/compute.cpp
    src/stage1/output.cpp
)
target_link_libraries(stage1_gcd PRIVATE model_support)
target_compile_definitions(stage1_gcd PRIVATE SC_ENABLE_ASSERTIONS)
if(MSVC)
    target_compile_options(stage1_gcd PRIVATE /W4)
else()
    target_compile_options(stage1_gcd PRIVATE -Wall -Wextra -Wpedantic)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME stage1_function_timing_backpressure
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage1/verify.py"
            "$<TARGET_FILE:stage1_gcd>" "${CMAKE_CURRENT_BINARY_DIR}/stage1-tests")
set_tests_properties(stage1_function_timing_backpressure PROPERTIES TIMEOUT 45)
add_test(NAME day01_signal_timing COMMAND day01_basics)
set_tests_properties(day01_signal_timing PROPERTIES TIMEOUT 10)

add_library(handshake_modules STATIC
    src/stage1/types.cpp src/stage1/parser.cpp src/stage1/output.cpp
    src/stage2/payload.cpp src/stage2/transform.cpp src/stage2/compute.cpp)
target_link_libraries(handshake_modules PUBLIC model_support)
target_compile_definitions(handshake_modules PUBLIC SC_ENABLE_ASSERTIONS)
add_executable(stage2_gcd src/stage2/main.cpp)
target_link_libraries(stage2_gcd PRIVATE handshake_modules)
foreach(target handshake_modules stage2_gcd)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

# Stage 4 starts as an independent copy of the verified cycle-based window model.
# Keep all project sources local so later event-model changes cannot alter stages 1-3.
add_executable(stage4_gcd
    src/stage4/main.cpp
    src/stage4/types.cpp src/stage4/parser.cpp src/stage4/output.cpp
    src/stage4/payload.cpp src/stage4/transform.cpp src/stage4/compute.cpp
    src/stage4/dispatcher.cpp src/stage4/collector.cpp
    src/stage4/common/task_statistics.cpp
    src/stage4/model/functional/operands.cpp
    src/stage4/model/timing/gcd_plan.cpp src/stage4/model/timing/simulation_time.cpp
    src/stage4/model/instrumentation/event_recorder.cpp
    src/stage4/model/instrumentation/statistics.cpp)
target_link_libraries(stage4_gcd PRIVATE SystemC::systemc)
target_compile_definitions(stage4_gcd PRIVATE SC_ENABLE_ASSERTIONS)
if(MSVC)
    target_compile_options(stage4_gcd PRIVATE /W4)
else()
    target_compile_options(stage4_gcd PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage4_initial_equivalence
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage4/compare_initial.py"
            "$<TARGET_FILE:stage3_window_gcd>" "$<TARGET_FILE:stage4_gcd>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage4-initial-tests")
set_tests_properties(stage4_initial_equivalence PROPERTIES TIMEOUT 120)
add_executable(stage4_parser_checks
    tests/stage4/parser_timing.cpp src/stage4/parser.cpp src/stage4/types.cpp
    src/stage4/common/task_statistics.cpp
    src/stage4/model/timing/simulation_time.cpp src/stage4/model/instrumentation/event_recorder.cpp)
target_link_libraries(stage4_parser_checks PRIVATE SystemC::systemc)
target_compile_definitions(stage4_parser_checks PRIVATE SC_ENABLE_ASSERTIONS)
if(MSVC)
    target_compile_options(stage4_parser_checks PRIVATE /W4)
else()
    target_compile_options(stage4_parser_checks PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage4_parser_timing COMMAND stage4_parser_checks)
set_tests_properties(stage4_parser_timing PROPERTIES TIMEOUT 10)
add_test(NAME stage4_event_scheduler
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage4/event_scheduler.py"
            "$<TARGET_FILE:stage3_window_gcd>" "$<TARGET_FILE:stage4_gcd>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage4-scheduler-tests")
set_tests_properties(stage4_event_scheduler PROPERTIES TIMEOUT 120)
add_test(NAME stage2_handshake
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/handshake/verify.py"
            "$<TARGET_FILE:stage2_gcd>" "${CMAKE_CURRENT_BINARY_DIR}/stage2-tests" "1")
set_tests_properties(stage2_handshake PROPERTIES TIMEOUT 60)

add_executable(stage3_gcd src/stage3/main.cpp src/stage3/dispatcher.cpp src/stage3/collector.cpp)
target_link_libraries(stage3_gcd PRIVATE handshake_modules)
if(MSVC)
    target_compile_options(stage3_gcd PRIVATE /W4)
else()
    target_compile_options(stage3_gcd PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage3_round_robin_ordering
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/handshake/verify.py"
            "$<TARGET_FILE:stage3_gcd>" "${CMAKE_CURRENT_BINARY_DIR}/stage3-tests" "2")
set_tests_properties(stage3_round_robin_ordering PROPERTIES TIMEOUT 60)
add_test(NAME stage3_performance_comparison
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage3/compare.py"
            "$<TARGET_FILE:stage2_gcd>" "$<TARGET_FILE:stage3_gcd>" "${CMAKE_CURRENT_BINARY_DIR}/stage3-comparison")
set_tests_properties(stage3_performance_comparison PROPERTIES TIMEOUT 60)

add_executable(stage3_window_gcd
    src/stage3_window/main.cpp src/stage3_window/dispatcher.cpp src/stage3_window/collector.cpp)
target_link_libraries(stage3_window_gcd PRIVATE handshake_modules)
if(MSVC)
    target_compile_options(stage3_window_gcd PRIVATE /W4)
else()
    target_compile_options(stage3_window_gcd PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage3_ready_window
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/handshake/verify.py"
            "$<TARGET_FILE:stage3_window_gcd>" "${CMAKE_CURRENT_BINARY_DIR}/stage3-window-tests" "2" "8")
set_tests_properties(stage3_ready_window PROPERTIES TIMEOUT 60)

# Public-interface contract checks must also run with NDEBUG in Release.
add_executable(contract_checks tests/contracts/verify.cpp)
target_link_libraries(contract_checks PRIVATE handshake_modules)
foreach(test_case helper period input output event statistics)
    add_test(NAME contract_${test_case} COMMAND contract_checks ${test_case})
    set_tests_properties(contract_${test_case} PROPERTIES TIMEOUT 10)
endforeach()

# Optional Windows profiling wrappers: measure unchanged model entry points.
# EXCLUDE_FROM_ALL keeps these out of ordinary builds and acceptance tests.
if(WIN32)
    add_executable(stage3_window_profile EXCLUDE_FROM_ALL
        tests/profiling/entry.cpp src/stage3_window/dispatcher.cpp src/stage3_window/collector.cpp)
    target_compile_definitions(stage3_window_profile PRIVATE
        PROFILE_MODEL_SOURCE="../../src/stage3_window/main.cpp")
    target_link_libraries(stage3_window_profile PRIVATE handshake_modules psapi)
    get_target_property(stage4_profile_sources stage4_gcd SOURCES)
    list(REMOVE_ITEM stage4_profile_sources src/stage4/main.cpp)
    add_executable(stage4_profile EXCLUDE_FROM_ALL tests/profiling/entry.cpp ${stage4_profile_sources})
    target_compile_definitions(stage4_profile PRIVATE SC_ENABLE_ASSERTIONS
        PROFILE_MODEL_SOURCE="../../src/stage4/main.cpp")
    target_link_libraries(stage4_profile PRIVATE SystemC::systemc psapi)
endif()
