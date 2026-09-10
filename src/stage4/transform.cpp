/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file transform.cpp
 * @brief Edge-based elastic pipeline with stable stalled payload.
 */
#include "transform.hpp"
#include "common/contract.hpp"
#include "model/functional/operands.hpp"

#include <algorithm>

namespace stage4 {
Transform::Transform(sc_core::sc_module_name name, EventRecorder& recorder)
    : sc_module(name)
    , m_recorder(recorder) {
    m_validOut.initialize(false);
}
void Transform::advance() {
    assertCondition<std::logic_error>(m_validOut.read() == m_orderedStage.has_value(),
                                       "transform valid does not match ordered slot");
    assertCondition<std::logic_error>(!m_orderedStage || m_dataOut.read() == *m_orderedStage,
                                       "transform signal does not match held payload");
    // Read old signal values; ordinary optional storage is updated immediately.
    if (m_validOut.read()) {
        ++m_statistics.m_validCycles;
        const auto task = m_dataOut.read();
        const bool ready = m_readyIn.read();
        m_recorder.record(task.m_id, "link", task.m_a, task.m_b, ready);
        if (ready) {
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

std::uint64_t Transform::nextDelay() const {
    const bool canEmit = m_orderedStage && m_readyIn.read();
    const bool canOrder = !m_orderedStage && m_magnitudeStage;
    const bool canAccept = !m_magnitudeStage && m_tasksIn.num_available() > 0;
    return canEmit || canOrder || canAccept ? 1 : model::timing::NO_DEADLINE;
}
void Transform::accountSkipped(std::uint64_t first, std::uint64_t count) {
    if (m_orderedStage) {
        assertCondition<std::logic_error>(!m_readyIn.read(), "skipped a ready transform transfer");
        m_statistics.m_validCycles += count;
        m_statistics.m_blockedCycles += count;
        m_recorder.repeat(first, count, m_orderedStage->m_id, "link", m_orderedStage->m_a, m_orderedStage->m_b, 0);
    }
}
} // namespace stage4
