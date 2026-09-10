/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file compute.cpp
 * @brief Compute clock-edge implementation.
 */
#include "compute.hpp"
#include "../common/contract.hpp"
#include "../model/timing/gcd_plan.hpp"

#include <algorithm>
#include <utility>

namespace stage1 {
Compute::Compute(sc_core::sc_module_name name, EventRecorder& recorder)
    : sc_module(name)
    , m_recorder(recorder) {
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
        return; // Completion must not fall through into accepting another task.
    }
    if (m_state == State::RESULT_PENDING) {
        deliver();
        return;
    }
    OrderedTask task;
    if (!m_tasksIn.nb_read(task)) {
        ++m_statistics.m_idleNoInputCycles;
        return;
    }
    const auto [value, latency] = model::timing::planGcd(task.m_a, task.m_b);
    m_result = {task.m_id, value};
    m_remaining = latency;
    m_recorder.record(task.m_id, "compute_accept", task.m_a, task.m_b, value, latency);
    if (latency == 0) {
        m_recorder.record(task.m_id, "compute_complete", 0, 0, value);
        m_state = State::RESULT_PENDING;
        deliver();
    } else {
        m_state = State::BUSY;
    }
}
} // namespace stage1
