/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.cpp
 * @brief Preserve input order while retaining early completions in their own queues.
 */
#include "collector.hpp"
#include "../common/contract.hpp"

#include <stdexcept>

namespace stage3 {
Collector::Collector(sc_core::sc_module_name name, stage1::EventRecorder& recorder)
    : sc_module(name)
    , m_recorder(recorder) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Collector::tick() {
    const auto selected = static_cast<unsigned>(m_nextId % UNIT_COUNT);
    const auto other = (selected + 1) % UNIT_COUNT;
    if (m_resultsIn[selected].num_available() == 0) {
        if (m_resultsIn[other].num_available() != 0) {
            ++m_statistics.m_orderWaitCycles;
            m_recorder.record(m_nextId, "reorder_wait", selected);
        }
        return;
    }
    if (m_resultsOut.num_free() == 0) {
        ++m_statistics.m_outputBlockedCycles;
        m_recorder.record(m_nextId, "collector_blocked");
        return;
    }
    stage1::Result result;
    requireCondition<std::logic_error>(m_resultsIn[selected].nb_read(result),
                                       "collector lost reserved channel capacity");
    requireCondition<std::logic_error>(result.m_id == m_nextId, "collector received unexpected result id");
    requireCondition<std::logic_error>(m_resultsOut.nb_write(result), "collector lost reserved output capacity");
    m_recorder.record(result.m_id, "reorder_emit", 0, 0, result.m_gcd);
    ++m_nextId;
}
} // namespace stage3
