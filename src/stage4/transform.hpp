/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file transform.hpp
 * @brief Two-stage transform with a held valid/ready transfer.
 */
#pragma once
#include "payload.hpp"

#include <optional>

#include "types.hpp"

namespace stage4 {
/**
 * @brief Transforms operands through two elastic stages with a valid/ready output.
 *
 * Interface:
 *
 * m_tasksIn --> +---------------------------+ --> m_dataOut / m_validOut
 * (RawTask)     |        Transform          | <-- m_readyIn
 * deadline -->  | magnitude -> ordered slot |
 *               +---------------------------+
 *
 * Protocol:
 * - The two slots each hold one task; signed operands are widened before taking magnitudes.
 * - Output payload keeps the task id and ordered nonnegative magnitudes a >= b.
 * - Transfer only when m_validOut && m_readyIn at the scheduled boundary.
 * - While stalled, hold the ordered slot and output data/valid; stop upstream reads when both slots are full.
 * - The ordered slot and output signals represent the same architectural storage, not duplicate buffers.
 * - The observer must outlive the module; m_validCycles and m_blockedCycles track link use and stalls.
 *
 * Timing:
 * - System schedules advance() at the next actionable timestamp; unchanged intervals are skipped.
 * - At shared T=1 ns, accept at k, compare/swap at k+1, and handshake at k+2 at earliest.
 * - At most one input and one output transfer per scheduled boundary; blocking extends the two-cycle base latency.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction leaves both slots empty, initializes m_validOut=false and zeroes counters.
 * - Output data has no transaction meaning while valid is false.
 */
SC_MODULE(Transform) {
    sc_core::sc_fifo_in<RawTask> m_tasksIn{"tasks_in"};
    sc_core::sc_out<Payload> m_dataOut{"data_out"};
    sc_core::sc_out<bool> m_validOut{"valid_out"};
    sc_core::sc_in<bool> m_readyIn{"ready_in"};
    model::instrumentation::LinkStatistics m_statistics;
    /**
     * @brief 两个内部槽都空；排序槽和输出信号不重复计数。
     */
    [[nodiscard]] bool empty() const {
        return !m_magnitudeStage && !m_orderedStage;
    }
    /**
     * @brief 当前内部任务数，范围 0..2；不包含外部 FIFO。
     */
    [[nodiscard]] unsigned occupancy() const {
        return static_cast<unsigned>(m_magnitudeStage.has_value()) + static_cast<unsigned>(m_orderedStage.has_value());
    }
    Transform(sc_core::sc_module_name name, EventRecorder & recorder);

    void advance();
    [[nodiscard]] std::uint64_t nextDelay() const;
    void accountSkipped(std::uint64_t first, std::uint64_t count);

private:
    std::optional<MagnitudeTask> m_magnitudeStage;
    std::optional<Payload> m_orderedStage;
    EventRecorder & m_recorder;
};
} // namespace stage4
