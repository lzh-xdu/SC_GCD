/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file transform.cpp
 * @brief Edge-based elastic pipeline with stable stalled payload.
 */
#include "transform.hpp"
#include "../common/contract.hpp"

#include <algorithm>

namespace stage2 {
static std::uint64_t magnitude(std::int32_t value) {
    const auto wide = static_cast<std::int64_t>(value);
    return static_cast<std::uint64_t>(wide < 0 ? -wide : wide);
}
Transform::Transform(sc_core::sc_module_name name, TestEventLog& testEventLog)
    : sc_module(name)
    , m_testEventLog(testEventLog) {
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
        ++m_validCycles;
        const auto task = m_dataOut.read();
        m_testEventLog.record(task.m_id, "link", task.m_a, task.m_b, m_readyIn.read());
        if (m_readyIn.read()) {
            m_testEventLog.record(task.m_id, "transform_emit", task.m_a, task.m_b);
            m_orderedStage.reset();
        } else {
            ++m_blockedCycles;
        }
    }
    if (!m_orderedStage && m_magnitudeStage) {
        m_orderedStage = Payload{m_magnitudeStage->m_id, std::max(m_magnitudeStage->m_a, m_magnitudeStage->m_b),
                                 std::min(m_magnitudeStage->m_a, m_magnitudeStage->m_b)};
        m_magnitudeStage.reset();
    }
    RawTask task;
    if (!m_magnitudeStage && m_tasksIn.nb_read(task)) {
        m_magnitudeStage = MagnitudeTask{task.m_id, magnitude(task.m_a), magnitude(task.m_b)};
        m_testEventLog.record(task.m_id, "transform_accept");
    }
    m_validOut.write(m_orderedStage.has_value());
    if (m_orderedStage) {
        m_dataOut.write(*m_orderedStage);
    }
}
} // namespace stage2
