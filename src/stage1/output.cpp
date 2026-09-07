/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file output.cpp
 * @brief Output clock-edge implementation.
 */
#include "output.hpp"
#include "../common/contract.hpp"

#include <ostream>
#include <stdexcept>

namespace stage1 {
Output::Output(sc_core::sc_module_name name, std::ostream& output, TestEventLog& testEventLog,
               std::uint64_t testOutputPeriod)
    : sc_module(name)
    , m_output(output)
    , m_testEventLog(testEventLog)
    , m_testOutputPeriod(testOutputPeriod) {
    requireCondition<std::invalid_argument>(testOutputPeriod > 0, "output period must be nonzero");
    requireCondition(static_cast<bool>(output), "output stream is not writable");
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Output::tick() {
    requireCondition<std::logic_error>(m_testOutputPeriod > 0, "output period must be nonzero");
    requireCondition(static_cast<bool>(m_output), "output file write failed");
    if (currentCycle() % m_testOutputPeriod != 0) {
        return;
    }
    Result result;
    if (!m_resultsIn.nb_read(result)) {
        return;
    }
    requireCondition<std::logic_error>(result.m_id == m_received, "out-of-order result");
    m_output << result.m_gcd << '\n';
    requireCondition(static_cast<bool>(m_output), "output file write failed");
    m_testEventLog.record(result.m_id, "output", 0, 0, result.m_gcd);
    ++m_received;
}
} // namespace stage1
