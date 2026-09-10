/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file output.cpp
 * @brief Output service deadlines without periodic polling.
 */
#include "output.hpp"
#include "common/contract.hpp"

#include <ostream>
#include <stdexcept>

namespace stage4 {
Output::Output(sc_core::sc_module_name name, std::ostream& output, EventRecorder& recorder,
               std::uint64_t testOutputPeriod)
    : sc_module(name)
    , m_output(output)
    , m_recorder(recorder)
    , m_testOutputPeriod(testOutputPeriod) {
    assertCondition<std::invalid_argument>(testOutputPeriod > 0, "output period must be nonzero");
    assertCondition(static_cast<bool>(output), "output stream is not writable");
}
void Output::advance() {
    assertCondition<std::logic_error>(m_testOutputPeriod > 0, "output period must be nonzero");
    assertCondition(static_cast<bool>(m_output), "output file write failed");
    if (currentCycle() % m_testOutputPeriod != 0) {
        return;
    }
    Result result;
    if (!m_resultsIn.nb_read(result)) {
        return;
    }
    assertCondition<std::logic_error>(result.m_id == m_received, "out-of-order result");
    m_output << result.m_gcd << '\n';
    assertCondition(static_cast<bool>(m_output), "output file write failed");
    m_recorder.record(result.m_id, "output", 0, 0, result.m_gcd);
    ++m_received;
}

std::uint64_t Output::nextDelay() const {
    return m_resultsIn.num_available() > 0 ? m_testOutputPeriod - currentCycle() % m_testOutputPeriod
                                           : model::timing::NO_DEADLINE;
}
void Output::accountSkipped(std::uint64_t, std::uint64_t) {
    // No output-side per-cycle counters; no result is consumed between eligible events.
}
} // namespace stage4
