/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file compute.cpp
 * @brief Handshake-controlled GCD latency model.
 */
#include "compute.hpp"
#include "../common/contract.hpp"
#include "../model/timing/gcd_plan.hpp"

#include <algorithm>
#include <utility>

namespace stage2 {
Compute::Compute(sc_core::sc_module_name name, EventRecorder& recorder, unsigned unit)
    : sc_module(name)
    , m_recorder(recorder)
    , m_unit(unit) {
    m_readyOut.initialize(true);
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Compute::deliver() {
    assertCondition<std::logic_error>(m_state == State::RESULT_PENDING, "compute result is not ready");
    if (m_resultsOut.nb_write(m_result)) {
        m_recorder.record(m_result.m_id, "compute_emit", 0, 0, m_result.m_gcd);
        m_state = State::IDLE;
    } else {
        ++m_statistics.m_resultWaitCycles;
    }
}
void Compute::accept() {
    assertCondition<std::logic_error>(m_state == State::IDLE && m_validIn.read() && m_readyOut.read(),
                                       "compute acceptance requires idle valid/ready handshake");
    const auto task = m_dataIn.read();
    const auto [value, latency] = model::timing::planGcd(task.m_a, task.m_b);
    m_result = {task.m_id, value};
    m_remaining = latency;
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
void Compute::tick() {
    assertCondition<std::logic_error>(m_state != State::BUSY || m_remaining > 0,
                                       "busy compute has no remaining cycles");
    if (m_state == State::BUSY) {
        ++m_statistics.m_busyCycles;
        if (--m_remaining == 0) {
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
} // namespace stage2
