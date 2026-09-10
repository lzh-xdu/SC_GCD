/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file verify.cpp
 * @brief Verify runtime contracts through public interfaces, including NDEBUG builds.
 */
#include "../../src/common/contract.hpp"
#include "../../src/stage1/output.hpp"
#include "../../src/stage1/parser.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace {
template <typename Exception, typename Action> void expectException(Action testAction, const char* testMessage) {
    try {
        testAction();
    } catch (const Exception& testError) {
        assertCondition(std::string(testError.what()) == testMessage, "unexpected exception message");
        return;
    }
    throw std::runtime_error("expected exception was not thrown");
}

void checkHelper() {
    int testEvaluations = 0;
    assertCondition(++testEvaluations == 1, "condition evaluated more than once");
    expectException<std::runtime_error>(
        [&testEvaluations]() { assertCondition(++testEvaluations == 0, "runtime failure"); }, "runtime failure");
    assertCondition(testEvaluations == 2, "failed condition evaluation count changed");
    expectException<std::invalid_argument>([]() { assertCondition<std::invalid_argument>(false, "argument failure"); },
                                           "argument failure");
}

void checkStatistics() {
    TaskStatistics testStatistics;
    std::ostringstream testOutput;
    testStatistics.write(testOutput); // Empty input remains legal.
    testStatistics.record(0, "parser_send", 1);
    expectException<std::logic_error>([&testStatistics, &testOutput]() { testStatistics.write(testOutput); },
                                      "unfinished task statistics at drain");
    expectException<std::logic_error>([&testStatistics]() { testStatistics.record(0, "compute_accept", 0); },
                                      "invalid task statistics event sequence");
}

void checkModule(const std::string& testCase) {
    stage1::EventRecorder testLog;
    std::ostringstream testOutput;
    if (testCase == "period") {
        expectException<std::invalid_argument>(
            [&testOutput, &testLog]() { stage1::Output testModule("output", testOutput, testLog, 0); },
            "output period must be nonzero");
    } else if (testCase == "input") {
        std::istringstream testInput;
        testInput.setstate(std::ios::badbit);
        expectException<std::runtime_error>(
            [&testInput, &testLog]() { stage1::Parser testModule("parser", testInput, testLog); },
            "input stream is not readable");
    } else if (testCase == "output") {
        testOutput.setstate(std::ios::badbit);
        expectException<std::runtime_error>(
            [&testOutput, &testLog]() { stage1::Output testModule("output", testOutput, testLog, 1); },
            "output stream is not writable");
    } else {
        assertCondition(testCase == "event", "unknown test case");
        expectException<std::invalid_argument>([&testLog]() { testLog.record(0, nullptr); }, "null event name");
    }
}
} // namespace

int sc_main(int testArgc, char** testArgv) {
    try {
        assertCondition(testArgc == 2, "expected test case argument");
        const std::string testCase(testArgv[1]);
        if (testCase == "helper") {
            checkHelper();
        } else if (testCase == "statistics") {
            checkStatistics();
        } else {
            checkModule(testCase);
        }
        std::cout << "PASS: " << testCase << '\n';
        return 0;
    } catch (const std::exception& testError) {
        std::cerr << "FAIL: " << testError.what() << '\n';
        return 1;
    }
}
