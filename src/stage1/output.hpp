/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file output.hpp
 * @brief Output module interface and cycle contract.
 */
#pragma once
#include "types.hpp"

#include <systemc>

namespace stage1 {
/**
 * @brief Writes ordered GCD results to the functional output stream.
 *
 * Interface:
 *
 * m_resultsIn --> +------------------+
 * (Result FIFO)   |      Output      |-----> m_output (one GCD per line)
 * m_clk --------->|                  |
 *                 +------------------+
 *
 * Protocol:
 * - Read at most one result on an eligible rising edge; require id == m_received.
 * - The functional stream contains GCD values only; EventRecorder records the output event separately.
 * - No internal task/result buffer; the top level owns the input FIFO (default depth 2).
 * - The output stream and observer must outlive the module.
 * - A zero testOutputPeriod throws invalid_argument; write failure throws runtime_error.
 * - An unexpected result id throws logic_error.
 *
 * Timing:
 * - SC_METHOD(tick) runs only on m_clk.pos(), with dont_initialize; T=1 ns, first edge at 1 ns.
 * - Default testOutputPeriod=1 allows one result per edge without extra processing latency.
 * - For period N, attempt a read only when cycle % N == 0; skipped edges propagate backpressure.
 * - A result written into the FIFO at edge k can be read at k+1 or later.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction initializes m_received=0.
 */
SC_MODULE(Output) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<Result> m_resultsIn{"results_in"};
    std::uint64_t m_received = 0;
    /**
     * @brief output 须可写且覆盖模块寿命；testOutputPeriod 单位为周期，必须大于零。
     * @throws std::invalid_argument 周期为零；写失败抛 runtime_error，结果乱序抛 logic_error。
     */
    Output(sc_core::sc_module_name name, std::ostream & output, EventRecorder & recorder,
           std::uint64_t testOutputPeriod);

private:
    std::ostream& m_output;
    EventRecorder & m_recorder;
    std::uint64_t m_testOutputPeriod;
    void tick();
};
} // namespace stage1
