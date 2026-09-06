/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file day01_basics.cpp
 * @brief Test clocked registers and delta-cycle signal visibility.
 */
#include <systemc>
#include <iostream>

namespace {
constexpr int TEST_FIRST_INPUT = 10;
constexpr int TEST_SECOND_INPUT = 20;
constexpr int TEST_THIRD_INPUT = 30;
constexpr int TEST_END_TIME_NS = 25;
constexpr int TEST_CLOCK_PERIOD_NS = 10;
constexpr double TEST_DUTY_CYCLE = 0.5;
constexpr int TEST_FIRST_EDGE_NS = 5;
constexpr int TEST_TRACE_UNIT_NS = 1;
constexpr int TEST_TIMEOUT_NS = 100;
} // namespace

// Both methods run at the same rising edge. sc_signal writes become visible
// during the subsequent update phase, so stage2 reads stage1's previous value.
SC_MODULE(TwoRegisters) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_in<int> m_dataIn{"data_in"};
    sc_core::sc_out<int> m_dataOut{"data_out"};
    sc_core::sc_signal<int> m_stage1{"stage1"};

    void capture() {
        m_stage1.write(m_dataIn.read());
    }
    void forward() {
        m_dataOut.write(m_stage1.read());
    }

    SC_CTOR(TwoRegisters) {
        SC_METHOD(capture);
        sensitive << m_clk.pos();
        dont_initialize();
        SC_METHOD(forward);
        sensitive << m_clk.pos();
        dont_initialize();
    }
};

SC_MODULE(Testbench) {
    sc_core::sc_in<bool> m_testClk{"clk"};
    sc_core::sc_out<int> m_testStimulusOut{"stimulus_out"};
    sc_core::sc_in<int> m_testResultIn{"result_in"};
    int m_testFailures = 0;

    void check(int testExpected, const char* testPhase) {
        std::cout << sc_core::sc_time_stamp() << " | " << testPhase << " | output=" << m_testResultIn.read()
                  << " expected=" << testExpected << '\n';
        if (m_testResultIn.read() != testExpected) {
            ++m_testFailures;
            std::cerr << "FAIL: " << testPhase << '\n';
        }
    }

    void run() {
        m_testStimulusOut.write(TEST_FIRST_INPUT);
        wait(m_testClk.posedge_event()); // 5 ns: stage1 captures 10, output stays 0.
        check(0, "edge 1 before update");
        wait(sc_core::SC_ZERO_TIME);
        check(0, "edge 1 after update");

        wait(m_testClk.negedge_event()); // Drive away from the sampling edge.
        m_testStimulusOut.write(TEST_SECOND_INPUT);
        wait(m_testClk.posedge_event()); // 15 ns: stage1 captures 20, output gets 10.
        check(0, "edge 2 before update");
        wait(sc_core::SC_ZERO_TIME);
        check(TEST_FIRST_INPUT, "edge 2 after update");

        wait(m_testClk.negedge_event());
        m_testStimulusOut.write(TEST_THIRD_INPUT);
        wait(m_testClk.posedge_event()); // 25 ns: output gets 20.
        check(TEST_FIRST_INPUT, "edge 3 before update");
        wait(sc_core::SC_ZERO_TIME);
        check(TEST_SECOND_INPUT, "edge 3 after update");
        if (sc_core::sc_time_stamp() != sc_core::sc_time(TEST_END_TIME_NS, sc_core::SC_NS)) {
            ++m_testFailures;
        }
        sc_core::sc_stop();
    }

    SC_CTOR(Testbench) {
        SC_THREAD(run);
    }
};

int sc_main(int, char**) {
    sc_core::sc_clock testClk("clk", sc_core::sc_time(TEST_CLOCK_PERIOD_NS, sc_core::SC_NS), TEST_DUTY_CYCLE,
                              sc_core::sc_time(TEST_FIRST_EDGE_NS, sc_core::SC_NS), true);
    sc_core::sc_signal<int> testTbToRegistersData("tb_to_registers_data");
    sc_core::sc_signal<int> testRegistersToTbData("registers_to_tb_data");
    TwoRegisters testModel("model");
    testModel.m_clk(testClk);
    testModel.m_dataIn(testTbToRegistersData);
    testModel.m_dataOut(testRegistersToTbData);
    Testbench testBench("tb");
    testBench.m_testClk(testClk);
    testBench.m_testStimulusOut(testTbToRegistersData);
    testBench.m_testResultIn(testRegistersToTbData);

    auto* testTrace = sc_core::sc_create_vcd_trace_file("day01_basics");
    testTrace->set_time_unit(TEST_TRACE_UNIT_NS, sc_core::SC_NS);
    sc_core::sc_trace(testTrace, testClk, "clk");
    sc_core::sc_trace(testTrace, testTbToRegistersData, "tb_to_registers_data");
    sc_core::sc_trace(testTrace, testModel.m_stage1, "stage1");
    sc_core::sc_trace(testTrace, testRegistersToTbData, "registers_to_tb_data");
    sc_core::sc_start(sc_core::sc_time(TEST_TIMEOUT_NS, sc_core::SC_NS));
    sc_core::sc_close_vcd_trace_file(testTrace);
    if (!sc_core::sc_end_of_simulation_invoked()) {
        std::cerr << "FAIL: test did not finish before timeout\n";
        return 1;
    }
    std::cout << (testBench.m_testFailures == 0 ? "PASS" : "FAIL") << '\n';
    return testBench.m_testFailures == 0 ? 0 : 1;
}
