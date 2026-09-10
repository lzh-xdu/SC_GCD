/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file model_entry.cpp
 * @brief Exercise actual entry-point reporting and option parsing at non-unit duration.
 */
// Override only the reporting translation unit's unit. No simulation is run here;
// this detects a reporter confusing cycles and ns without changing production defaults.
#ifdef TEST_STAGE4
#define CYCLE_DURATION_NS TEST_ORIGINAL_DURATION_NS
#include "../../src/stage4/types.hpp"
#undef CYCLE_DURATION_NS
namespace stage4 {
inline constexpr double CYCLE_DURATION_NS = 2.5;
}
#else
#define CLOCK_PERIOD_NS TEST_ORIGINAL_DURATION_NS
#include "../../src/stage1/types.hpp"
#undef CLOCK_PERIOD_NS
namespace stage1 {
inline constexpr double CLOCK_PERIOD_NS = 2.5;
}
#endif

#define sc_main testModelMain
#include TEST_MODEL_SOURCE
#undef sc_main

#include <limits>
#include <sstream>

namespace {
void checkReporting() {
    constexpr std::uint64_t TEST_OBSERVED_CYCLES = 7;
    std::istringstream testInput;
    std::ostringstream testOutput;
    std::ostringstream testStats;
    EventRecorder testRecorder;
    Options testOptions;
    System testSystem("test_system", testInput, testOutput, testRecorder, testOptions);
    testSystem.m_cycles = TEST_OBSERVED_CYCLES;
    writeStats(testStats, testSystem, testOptions);
    assertCondition(testStats.str().find("\ncycles,7\n") != std::string::npos, "cycle count changed");
    assertCondition(testStats.str().find("\nsimulated_time_ns,17.5\n") != std::string::npos,
                    "7 cycles at 2.5 ns must report 17.5 ns");
}
void checkOptions() {
    constexpr std::uint64_t TEST_LIMIT = 100;
    constexpr auto TEST_UINT64_MAX = std::numeric_limits<std::uint64_t>::max();
    for (const auto& testText : {std::string("999999999999999999999999999999"), std::string("101"), std::string("0")}) {
        bool testRejected = false;
        try {
            parsePositiveInteger(testText.c_str(), TEST_LIMIT);
        } catch (const std::runtime_error& testError) {
            assertCondition(std::string(testError.what()) == "option out of range", "unexpected range diagnostic");
            testRejected = true;
        }
        assertCondition(testRejected, "out-of-range option accepted");
    }
    assertCondition(parsePositiveInteger("100", TEST_LIMIT) == TEST_LIMIT, "inclusive limit rejected");
    assertCondition(parsePositiveInteger("0000000000000000000000000000007", TEST_LIMIT) == 7, "leading zeros rejected");
    assertCondition(parsePositiveInteger("18446744073709551615", TEST_UINT64_MAX) == TEST_UINT64_MAX,
                    "uint64 maximum rejected");
}
} // namespace

extern "C" int sc_main(int testArgc, char** testArgv) {
    try {
        assertCondition(testArgc == 2, "expected reporting or options test");
        if (std::string(testArgv[1]) == "reporting") {
            checkReporting();
        } else {
            checkOptions();
        }
        std::cout << "PASS " << testArgv[1] << '\n';
        return 0;
    } catch (const std::exception& testError) {
        std::cerr << "FAIL: " << testError.what() << '\n';
        return 1;
    }
}
