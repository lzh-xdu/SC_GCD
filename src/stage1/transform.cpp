/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file transform.cpp
 * @brief Transform clock-edge implementation.
 */
#include "transform.hpp"

#include <algorithm>

namespace stage1 {
static std::uint64_t magnitude(std::int32_t value) {
    const auto wide = static_cast<std::int64_t>(value); // Promote before negating INT32_MIN.
    return static_cast<std::uint64_t>(wide < 0 ? -wide : wide);
}

Transform::Transform(sc_core::sc_module_name name, TestEventLog& testEventLog)
    : sc_module(name)
    , m_testEventLog(testEventLog) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Transform::tick() {
    // Downstream-to-upstream order: a newly accepted task cannot cross two stages.
    if (m_stage2 && m_tasksOut.nb_write(*m_stage2)) {
        m_testEventLog.record(m_stage2->m_id, "transform_emit", m_stage2->m_a, m_stage2->m_b);
        m_stage2.reset();
    }
    if (!m_stage2 && m_stage1) {
        m_stage2 =
            OrderedTask{m_stage1->m_id, std::max(m_stage1->m_a, m_stage1->m_b), std::min(m_stage1->m_a, m_stage1->m_b)};
        m_stage1.reset();
    }
    RawTask task;
    if (!m_stage1 && m_tasksIn.nb_read(task)) {
        m_stage1 = MagnitudeTask{task.m_id, magnitude(task.m_a), magnitude(task.m_b)};
        m_testEventLog.record(task.m_id, "transform_accept");
    }
}
} // namespace stage1
