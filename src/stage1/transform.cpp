/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file transform.cpp
 * @brief Transform clock-edge implementation.
 */
#include "transform.hpp"
#include "../model/functional/operands.hpp"

#include <algorithm>

namespace stage1 {
Transform::Transform(sc_core::sc_module_name name, EventRecorder& recorder)
    : sc_module(name)
    , m_recorder(recorder) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Transform::tick() {
    // Downstream-to-upstream order: a newly accepted task cannot cross two stages.
    // These optional values update immediately, unlike sc_signal's deferred writes.
    if (m_orderedStage && m_tasksOut.nb_write(*m_orderedStage)) {
        m_recorder.record(m_orderedStage->m_id, "transform_emit", m_orderedStage->m_a, m_orderedStage->m_b);
        m_orderedStage.reset();
    }
    if (!m_orderedStage && m_magnitudeStage) {
        const auto [a, b] = model::functional::order(m_magnitudeStage->m_a, m_magnitudeStage->m_b);
        m_orderedStage = OrderedTask{m_magnitudeStage->m_id, a, b};
        m_magnitudeStage.reset();
    }
    RawTask task;
    if (!m_magnitudeStage && m_tasksIn.nb_read(task)) {
        m_magnitudeStage =
            MagnitudeTask{task.m_id, model::functional::magnitude(task.m_a), model::functional::magnitude(task.m_b)};
        m_recorder.record(task.m_id, "transform_accept");
    }
}
} // namespace stage1
