/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file parser_timing.cpp
 * @brief Verify input pacing and FIFO wakeups without any clock in the test system.
 */
#include "../../src/stage4/parser.hpp"
#include "../../src/stage4/common/contract.hpp"

#include <iostream>
#include <sstream>

namespace {
constexpr int TEST_FIFO_CAPACITY = 1;

SC_MODULE(ParserTest) {
    std::istringstream m_testInput{"48 18\n100 25\n"};
    stage4::EventRecorder m_testRecorder;
    sc_core::sc_fifo<stage4::RawTask> m_testFifo{"fifo", TEST_FIFO_CAPACITY};
    stage4::Parser m_testParser{"parser", m_testInput, m_testRecorder};
    bool m_testFinished = false;

    SC_CTOR(ParserTest) {
        m_testParser.m_tasksOut(m_testFifo);
        SC_THREAD(checkTiming);
    }

    void checkTiming() {
        wait(sc_core::sc_time(1.5, sc_core::SC_NS));
        assertCondition(m_testParser.m_sent == 1 && m_testFifo.num_available() == 1, "first input must be at 1 ns");
        // At 2 ns the parser observes full and sleeps; no clock or periodic observer exists.
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        const auto testDeltaBefore = sc_core::sc_delta_count();
        wait(sc_core::sc_time(3, sc_core::SC_NS));
        assertCondition(sc_core::sc_delta_count() == testDeltaBefore + 1, "blocked parser is polling");
        assertCondition(m_testParser.m_sent == 1, "full FIFO must prevent further input");
        stage4::RawTask testTask;
        assertCondition(m_testFifo.nb_read(testTask) && testTask.m_id == 0 && testTask.m_a == 48 && testTask.m_b == 18,
                         "first task lost or changed");
        wait(sc_core::sc_time(0.25, sc_core::SC_NS));
        assertCondition(m_testParser.m_sent == 1, "5.5 ns release must not send before 6 ns");
        wait(sc_core::sc_time(0.5, sc_core::SC_NS));
        assertCondition(m_testParser.m_sent == 2 && m_testFifo.num_available() == 1, "must resume at 6 ns");
        wait(sc_core::sc_time(1.25, sc_core::SC_NS));
        assertCondition(m_testFifo.nb_read(testTask) && testTask.m_id == 1 && testTask.m_a == 100 &&
                             testTask.m_b == 25,
                         "second task lost or changed");
        wait(sc_core::sc_time(0.25, sc_core::SC_NS));
        assertCondition(!m_testParser.m_eof, "EOF must wait for the 8 ns input boundary");
        wait(sc_core::sc_time(0.5, sc_core::SC_NS));
        assertCondition(m_testParser.m_eof, "EOF must terminate the input thread");
        const auto testDeltaAtEof = sc_core::sc_delta_count();
        wait(sc_core::sc_time(3, sc_core::SC_NS));
        assertCondition(sc_core::sc_delta_count() == testDeltaAtEof + 1, "parser still wakes after EOF");
        m_testFinished = true;
        sc_core::sc_stop();
    }
};
} // namespace

int sc_main(int, char**) {
    ParserTest testSystem("test_system");
    sc_core::sc_start();
    assertCondition(testSystem.m_testFinished, "parser timing test did not finish");
    std::cout << "PASS clockless parser: first=1 ns, release=5.5 ns, resume=6 ns, EOF=8 ns, no blocked/EOF polling\n";
    return 0;
}
