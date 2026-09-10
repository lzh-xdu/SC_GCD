/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file trace_ordering.cpp
 * @brief Compare compact Trace emission against independent stable per-row CSV ordering.
 */
#include "../../src/stage4/model/instrumentation/event_recorder.hpp"
#include "../../src/stage4/common/contract.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <systemc>
#include <utility>
#include <vector>

namespace {
constexpr std::uint64_t TEST_INTERVAL_CYCLES = 3000;
constexpr auto TEST_MAX_VALUE = std::numeric_limits<std::uint64_t>::max();
using TestRows = std::vector<std::pair<std::uint64_t, std::string>>;

void addExpected(TestRows& testRows, std::uint64_t testFirst, std::uint64_t testCount, std::uint64_t testId,
                 const std::string& testEvent, std::uint64_t testLatency = 0) {
    for (std::uint64_t testOffset = 0; testOffset < testCount; ++testOffset) {
        const auto testCycle = testFirst + testOffset;
        std::ostringstream testRow;
        testRow << testId << ',' << testEvent << ',' << testCycle << ',' << TEST_MAX_VALUE << ",0," << TEST_MAX_VALUE
                << ',' << testLatency << '\n';
        testRows.emplace_back(testCycle, testRow.str());
    }
}

void checkOrdering() {
    stage4::model::instrumentation::EventRecorder testRecorder;
    std::ostringstream testStream;
    testRecorder.m_testStream = &testStream;
    TestRows testRows;
    std::string testEvent = "temporary_event_name_with_owned_storage";
    testRecorder.repeat(2, TEST_INTERVAL_CYCLES, 0, testEvent, TEST_MAX_VALUE, 0, TEST_MAX_VALUE);
    addExpected(testRows, 2, TEST_INTERVAL_CYCLES, 0, testEvent);
    testEvent.assign("changed"); // A borrowed string_view would corrupt deferred rows.
    testRecorder.repeat(1, TEST_INTERVAL_CYCLES, 1, "second", TEST_MAX_VALUE, 0, TEST_MAX_VALUE);
    addExpected(testRows, 1, TEST_INTERVAL_CYCLES, 1, "second");
    testRecorder.append(2, TEST_MAX_VALUE, "single", TEST_MAX_VALUE, 0, TEST_MAX_VALUE, TEST_MAX_VALUE);
    addExpected(testRows, 2, 1, TEST_MAX_VALUE, "single", TEST_MAX_VALUE);
    testRecorder.repeat(0, 0, 9, "empty");
    testRecorder.flushBatch();
    testRecorder.append(TEST_INTERVAL_CYCLES + 2, 2, "next_batch", TEST_MAX_VALUE, 0, TEST_MAX_VALUE, 0);
    addExpected(testRows, TEST_INTERVAL_CYCLES + 2, 1, 2, "next_batch");
    testRecorder.flushTrace();
    testRecorder.flushTrace(); // Drain is idempotent, including already written full byte buffers.
    std::stable_sort(testRows.begin(), testRows.end(),
                     [](const auto& testLeft, const auto& testRight) { return testLeft.first < testRight.first; });
    std::string testExpected;
    for (const auto& testRow : testRows) {
        testExpected += testRow.second;
    }
    assertCondition(testStream.str() == testExpected, "trace content, tie ordering or batch boundary changed");
}

void checkStreamFailure() {
    stage4::model::instrumentation::EventRecorder testRecorder;
    std::ostringstream testStream;
    testRecorder.m_testStream = &testStream;
    testRecorder.append(1, 0, "test", 0, 0, 0, 0);
    testStream.setstate(std::ios::badbit);
    try {
        testRecorder.flushTrace();
    } catch (const std::runtime_error& testError) {
        assertCondition(std::string(testError.what()) == "event stream is not writable", "wrong trace error");
        return;
    }
    throw std::runtime_error("failed trace stream was silently accepted");
}
} // namespace

int sc_main(int, char**) {
    try {
        checkOrdering();
        checkStreamFailure();
        std::cout << "PASS: interval merge, stable ties, owned names, uint64 CSV, buffered drain and I/O errors\n";
        return 0;
    } catch (const std::exception& testError) {
        std::cerr << "FAIL: " << testError.what() << '\n';
        return 1;
    }
}
