/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file transform.cpp
 * @brief Edge-based elastic pipeline with stable stalled payload.
 */
#include "transform.hpp"
#include "../model/functional/operands.hpp"
#include "../common/contract.hpp"

#include <algorithm>

namespace stage2 {
Transform::Transform(sc_core::sc_module_name name, EventRecorder& recorder)
    : sc_module(name)
    , m_recorder(recorder) {
    m_validOut.initialize(false);
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Transform::tick() {
    requireCondition<std::logic_error>(m_validOut.read() == m_orderedStage.has_value(),
                                       "transform valid does not match ordered slot");
    requireCondition<std::logic_error>(!m_orderedStage || m_dataOut.read() == *m_orderedStage,
                                       "transform signal does not match held payload");
    // Read old signal values; ordinary optional storage is updated immediately.
    if (m_validOut.read()) {
        ++m_statistics.m_validCycles;
        const auto task = m_dataOut.read();
        m_recorder.record(task.m_id, "link", task.m_a, task.m_b, m_readyIn.read());
        if (m_readyIn.read()) {
            m_recorder.record(task.m_id, "transform_emit", task.m_a, task.m_b);
            m_orderedStage.reset();
        } else {
            ++m_statistics.m_blockedCycles;
        }
    }
    if (!m_orderedStage && m_magnitudeStage) {
        const auto [a, b] = model::functional::order(m_magnitudeStage->m_a, m_magnitudeStage->m_b);
        m_orderedStage = Payload{m_magnitudeStage->m_id, a, b};
        m_magnitudeStage.reset();
    }
    RawTask task;
    if (!m_magnitudeStage && m_tasksIn.nb_read(task)) {
        m_magnitudeStage =
            MagnitudeTask{task.m_id, model::functional::magnitude(task.m_a), model::functional::magnitude(task.m_b)};
        m_recorder.record(task.m_id, "transform_accept");
    }
    m_validOut.write(m_orderedStage.has_value());
    if (m_orderedStage) {
        m_dataOut.write(*m_orderedStage);
    }
}
} // namespace stage2
