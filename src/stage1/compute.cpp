/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file compute.cpp
 * @brief Compute clock-edge implementation.
 */
#include "compute.hpp"
#include "../common/contract.hpp"

#include <algorithm>
#include <utility>

namespace stage1 {
static unsigned bits(std::uint64_t value) {
    unsigned count = 0;
    while (value != 0) {
        ++count;
        value >>= 1;
    }
    return count;
}
static std::pair<std::uint64_t, std::uint64_t> gcdAndLatency(std::uint64_t a, std::uint64_t b) {
    requireCondition<std::logic_error>(a >= b, "compute requires ordered magnitudes");
    std::uint64_t latency = 0;
    while (b != 0) {
        constexpr int MIN_REMAINDER_CYCLES = 1;
        constexpr int INCLUSIVE_BIT_POSITION = 1;
        latency += std::max(MIN_REMAINDER_CYCLES,
                            static_cast<int>(bits(a)) - static_cast<int>(bits(b)) + INCLUSIVE_BIT_POSITION);
        const auto remainder = a % b;
        a = b;
        b = remainder;
    }
    return {a, latency};
}

Compute::Compute(sc_core::sc_module_name name, TestEventLog& testEventLog)
    : sc_module(name)
    , m_testEventLog(testEventLog) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Compute::deliver() {
    requireCondition<std::logic_error>(m_state == State::RESULT_PENDING, "compute result is not ready");
    if (m_resultsOut.nb_write(m_result)) {
        m_testEventLog.record(m_result.m_id, "compute_emit", 0, 0, m_result.m_gcd);
        m_state = State::IDLE;
    } else {
        ++m_resultWaitCycles;
    }
}
void Compute::tick() {
    requireCondition<std::logic_error>(m_state != State::BUSY || m_remaining > 0,
                                       "busy compute has no remaining cycles");
    if (m_state == State::BUSY) {
        ++m_busyCycles;
        if (--m_remaining == 0) {
            m_testEventLog.record(m_result.m_id, "compute_complete", 0, 0, m_result.m_gcd);
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
        ++m_idleNoInputCycles;
        return;
    }
    const auto [value, latency] = gcdAndLatency(task.m_a, task.m_b);
    m_result = {task.m_id, value};
    m_remaining = latency;
    m_testEventLog.record(task.m_id, "compute_accept", task.m_a, task.m_b, value, latency);
    if (latency == 0) {
        m_testEventLog.record(task.m_id, "compute_complete", 0, 0, value);
        m_state = State::RESULT_PENDING;
        deliver();
    } else {
        m_state = State::BUSY;
    }
}
} // namespace stage1
