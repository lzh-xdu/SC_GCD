/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file transform.cpp
 * @brief Transform clock-edge implementation.
 */
#include "transform.hpp"
#include "../common/contract.hpp"
#include "../model/functional/operands.hpp"

namespace stage1 {
Transform::Transform(sc_core::sc_module_name name, EventRecorder& recorder)
    : sc_module(name)
    , m_recorder(recorder) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Transform::tick() {
    // Backpressure propagates upstream; a slot can be refilled on the edge it drains.
    const bool emit = m_orderedStage && m_tasksOut.num_free() > 0;
    const bool orderedReady = !m_orderedStage || emit;
    const bool magnitudeReady = !m_magnitudeStage || orderedReady;
    auto nextMagnitudeStage = m_magnitudeStage;
    auto nextOrderedStage = m_orderedStage;

    // Read current registers, write next registers: new input cannot cross two stages.
    RawTask task;
    const bool accept = magnitudeReady && m_tasksIn.nb_read(task);
    if (accept) {
        nextMagnitudeStage =
            MagnitudeTask{task.m_id, model::functional::magnitude(task.m_a), model::functional::magnitude(task.m_b)};
    } else if (magnitudeReady) {
        nextMagnitudeStage.reset();
    }
    if (orderedReady) {
        if (m_magnitudeStage) {
            const auto [a, b] = model::functional::order(m_magnitudeStage->m_a, m_magnitudeStage->m_b);
            nextOrderedStage = OrderedTask{m_magnitudeStage->m_id, a, b};
        } else {
            nextOrderedStage.reset();
        }
    }
    if (emit) {
        // This SC_METHOD never yields between checking FIFO space and writing it.
        const bool written = m_tasksOut.nb_write(*m_orderedStage);
        requireCondition<std::logic_error>(written, "transform output FIFO readiness changed within tick");
        m_recorder.record(m_orderedStage->m_id, "transform_emit", m_orderedStage->m_a, m_orderedStage->m_b);
    }
    if (accept) {
        m_recorder.record(task.m_id, "transform_accept");
    }
    // Unlike sc_signal, optional assignments take effect immediately; commit only here.
    m_magnitudeStage = nextMagnitudeStage;
    m_orderedStage = nextOrderedStage;
}
} // namespace stage1
