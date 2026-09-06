/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.cpp
 * @brief Retire in sequence without denying the oldest task its reserved slot.
 */
#include "collector.hpp"

#include <stdexcept>

namespace stage3_window {
Collector::Collector(sc_core::sc_module_name name, stage1::TestEventLog& testEventLog, unsigned window)
    : sc_module(name)
    , m_testEventLog(testEventLog)
    , m_slots(window) {
    if (window == 0) {
        throw std::invalid_argument("window must be nonzero");
    }
    m_baseOut.initialize(0);
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Collector::retire() {
    auto& head = m_slots[m_nextId % m_slots.size()];
    if (!head) {
        if (m_completed != 0) {
            ++m_orderWaitCycles;
            m_testEventLog.record(m_nextId, "reorder_wait");
        }
        return;
    }
    if (head->m_id != m_nextId) {
        throw std::logic_error("window head tag mismatch");
    }
    if (!m_resultsOut.nb_write(*head)) {
        ++m_outputBlockedCycles;
        m_testEventLog.record(m_nextId, "collector_blocked");
        return;
    }
    m_testEventLog.record(m_nextId, "reorder_emit", 0, 0, head->m_gcd);
    head.reset();
    --m_completed;
    ++m_nextId;
}
void Collector::receive() {
    const auto other = (m_pollTurn + 1) % UNIT_COUNT;
    const auto selected = m_resultsIn[m_pollTurn].num_available() != 0 ? m_pollTurn : other;
    stage1::Result result;
    if (!m_resultsIn[selected].nb_read(result)) {
        return;
    }
    if (result.m_id < m_nextId || result.m_id - m_nextId >= m_slots.size()) {
        throw std::logic_error("result outside reserved window");
    }
    auto& slot = m_slots[result.m_id % m_slots.size()];
    if (slot) {
        throw std::logic_error("duplicate or colliding window result");
    }
    slot = result;
    ++m_completed;
    m_pollTurn = (selected + 1) % UNIT_COUNT;
    m_testEventLog.record(result.m_id, "window_store", selected, 0, result.m_gcd);
}
void Collector::tick() {
    retire();
    receive();
    m_baseOut.write(m_nextId);
}
} // namespace stage3_window
