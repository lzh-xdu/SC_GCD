/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.cpp
 * @brief Retire in sequence without denying the oldest task its reserved slot.
 */
#include "collector.hpp"
#include "common/contract.hpp"

#include <stdexcept>

namespace stage4 {
Collector::Collector(sc_core::sc_module_name name, stage4::EventRecorder& recorder, unsigned window)
    : sc_module(name)
    , m_recorder(recorder)
    , m_slots(window) {
    requireCondition<std::invalid_argument>(window > 0, "window must be nonzero");
    m_baseOut.initialize(0);
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
    stage4::Result result;
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
void Collector::advance() {
    requireCondition<std::logic_error>(!m_slots.empty() && m_completed <= m_slots.size(),
                                       "invalid collector window state");
    retire();
    receive();
    m_baseOut.write(m_nextId);
}

std::uint64_t Collector::nextDelay() const {
    const bool canRetire = m_slots[m_nextId % m_slots.size()] && m_resultsOut.num_free() > 0;
    const bool canReceive = m_resultsIn[0].num_available() > 0 || m_resultsIn[1].num_available() > 0;
    return canRetire || canReceive ? 1 : model::timing::NO_DEADLINE;
}
void Collector::accountSkipped(std::uint64_t first, std::uint64_t count) {
    if (m_slots[m_nextId % m_slots.size()]) {
        requireCondition(m_resultsOut.num_free() == 0, "skipped a collector retirement");
        m_statistics.m_outputBlockedCycles += count;
        m_recorder.repeat(first, count, m_nextId, "collector_blocked");
    } else if (m_completed != 0) {
        m_statistics.m_orderWaitCycles += count;
        m_recorder.repeat(first, count, m_nextId, "reorder_wait");
    }
}
} // namespace stage4
