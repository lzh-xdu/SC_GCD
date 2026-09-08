/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.cpp
 * @brief Retire in sequence without denying the oldest task its reserved slot.
 */
#include "collector.hpp"
#include "../common/contract.hpp"

#include <stdexcept>

namespace stage3_window {
Collector::Collector(sc_core::sc_module_name name, stage1::EventRecorder& recorder, unsigned window)
    : sc_module(name)
    , m_recorder(recorder)
    , m_slots(window) {
    requireCondition<std::invalid_argument>(window > 0, "window must be nonzero");
    m_baseOut.initialize(0);
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Collector::retire() {
    requireCondition<std::logic_error>(!m_slots.empty() && m_completed <= m_slots.size(),
                                       "invalid collector window state");
    auto& head = m_slots[m_nextId % m_slots.size()];
    if (!head) {
        if (m_completed != 0) {
            ++m_statistics.m_orderWaitCycles;
            m_recorder.record(m_nextId, "reorder_wait");
        }
        return;
    }
    requireCondition<std::logic_error>(head->m_id == m_nextId, "window head tag mismatch");
    if (!m_resultsOut.nb_write(*head)) {
        ++m_statistics.m_outputBlockedCycles;
        m_recorder.record(m_nextId, "collector_blocked");
        return;
    }
    m_recorder.record(m_nextId, "reorder_emit", 0, 0, head->m_gcd);
    head.reset();
    --m_completed;
    ++m_nextId;
}
void Collector::receive() {
    requireCondition<std::logic_error>(!m_slots.empty() && m_completed <= m_slots.size(),
                                       "invalid collector window state");
    requireCondition<std::logic_error>(m_pollTurn < UNIT_COUNT, "collector poll turn out of range");
    const auto other = (m_pollTurn + 1) % UNIT_COUNT;
    const auto selected = m_resultsIn[m_pollTurn].num_available() != 0 ? m_pollTurn : other;
    stage1::Result result;
    if (!m_resultsIn[selected].nb_read(result)) {
        return;
    }
    requireCondition<std::logic_error>(result.m_id >= m_nextId && result.m_id - m_nextId < m_slots.size(),
                                       "result outside reserved window");
    auto& slot = m_slots[result.m_id % m_slots.size()];
    requireCondition<std::logic_error>(!slot, "duplicate or colliding window result");
    slot = result;
    ++m_completed;
    m_pollTurn = (selected + 1) % UNIT_COUNT;
    m_recorder.record(result.m_id, "window_store", selected, 0, result.m_gcd);
}
void Collector::tick() {
    requireCondition<std::logic_error>(!m_slots.empty() && m_completed <= m_slots.size(),
                                       "invalid collector window state");
    retire();
    receive();
    m_baseOut.write(m_nextId);
}
} // namespace stage3_window
