/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file compute.cpp
 * @brief Handshake-controlled GCD latency model.
 */
#include "compute.hpp"
#include "common/contract.hpp"
#include "model/timing/gcd_plan.hpp"

#include <algorithm>
#include <utility>

namespace stage4 {
Compute::Compute(sc_core::sc_module_name name, EventRecorder& recorder, unsigned unit)
    : sc_module(name)
    , m_recorder(recorder)
    , m_unit(unit) {
    m_readyOut.initialize(true);
}
void Compute::deliver() {
    requireCondition<std::logic_error>(m_state == State::RESULT_PENDING, "compute result is not ready");
    if (m_resultsOut.nb_write(m_result)) {
        m_recorder.record(m_result.m_id, "compute_emit", 0, 0, m_result.m_gcd);
        m_state = State::IDLE;
    } else {
        ++m_statistics.m_resultWaitCycles;
    }
}
void Compute::accept() {
    requireCondition<std::logic_error>(m_state == State::IDLE && m_validIn.read() && m_readyOut.read(),
                                       "compute acceptance requires idle valid/ready handshake");
    const auto task = m_dataIn.read();
    const auto [value, latency] = model::timing::planGcd(task.m_a, task.m_b);
    m_result = {task.m_id, value};
    m_completeCycle = currentCycle() + latency;
    ++m_statistics.m_accepted;
    m_recorder.record(task.m_id, "compute_unit", m_unit);
    m_recorder.record(task.m_id, "compute_accept", task.m_a, task.m_b, value, latency);
    m_state = State::BUSY;
    if (latency == 0) {
        m_recorder.record(task.m_id, "compute_complete", 0, 0, value);
        m_state = State::RESULT_PENDING;
        deliver();
    }
}
void Compute::advance() {
    requireCondition<std::logic_error>(m_state != State::BUSY || currentCycle() <= m_completeCycle,
                                       "missed compute completion deadline");
    if (m_state == State::BUSY) {
        ++m_statistics.m_busyCycles;
        if (currentCycle() == m_completeCycle) {
            m_recorder.record(m_result.m_id, "compute_complete", 0, 0, m_result.m_gcd);
            m_state = State::RESULT_PENDING;
            deliver();
        }
    } else if (m_state == State::RESULT_PENDING) {
        deliver();
    } else if (m_validIn.read() && m_readyOut.read()) {
        accept();
    } else {
        ++m_statistics.m_idleNoInputCycles;
    }
    // else-if prevents completion falling through into acceptance on this edge.
    m_readyOut.write(m_state == State::IDLE);
}

std::uint64_t Compute::nextDelay() const {
    if (m_state == State::BUSY) {
        return m_completeCycle - currentCycle();
    }
    if (m_state == State::RESULT_PENDING) {
        return m_resultsOut.num_free() > 0 ? 1 : model::timing::NO_DEADLINE;
    }
    return m_validIn.read() && m_readyOut.read() ? 1 : model::timing::NO_DEADLINE;
}
void Compute::accountSkipped(std::uint64_t, std::uint64_t count) {
    if (m_state == State::BUSY) {
        requireCondition(m_completeCycle > currentCycle() + count, "skipped compute completion deadline");
        m_statistics.m_busyCycles += count;
    } else if (m_state == State::RESULT_PENDING) {
        m_statistics.m_resultWaitCycles += count;
    } else {
        m_statistics.m_idleNoInputCycles += count;
    }
}
} // namespace stage4
