/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file transform.hpp
 * @brief Transform module interface and cycle contract.
 */
#pragma once
#include "types.hpp"

#include <systemc>
#include <optional>

// stage1 names the assignment version, not a pipeline register or a hardware module.
namespace stage1 {
/**
 * @brief Computes magnitudes and orders operands in a two-stage elastic pipeline.
 *
 * Interface:
 *
 * m_tasksIn --> +-----------------------------------+ --> m_tasksOut
 * (RawTask)     | magnitude slot --> ordered slot   |     (OrderedTask)
 * m_clk ----->  |            Transform              |
 *               +-----------------------------------+
 *
 * Protocol:
 * - Each optional slot stores one task; external FIFOs are separate (default depth 2).
 * - Widen signed input before taking its absolute value to support INT32_MIN.
 * - Output a >= b >= 0 while preserving the original task id.
 * - Hold the ordered slot if output is blocked; accept into an empty magnitude slot until both slots are full.
 * - Update output, then magnitude, then input: freed slots propagate upstream for same-edge refill.
 * - Optional updates are immediate; downstream-first execution prevents new input from crossing both stages.
 * - EventRecorder is an observer, not hardware storage or a scheduling input, and must outlive the module.
 *
 * Timing:
 * - SC_METHOD(tick) runs only on m_clk.pos(), with dont_initialize; T=1 ns, first edge at 1 ns.
 * - Accept and take magnitudes at k, compare/swap at k+1, and write the FIFO at k+2 when unblocked.
 * - The downstream FIFO reader sees that task at k+3 or later; backpressure adds waiting.
 * - At most one task is accepted and one emitted per edge; the base module latency is exactly 2T.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Both optional pipeline slots are empty after construction.
 */
SC_MODULE(Transform) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<RawTask> m_tasksIn{"tasks_in"};
    sc_core::sc_fifo_out<OrderedTask> m_tasksOut{"tasks_out"};
    /**
     * @brief 两个内部槽都空；不包含顶层输入/输出 FIFO。
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

private:
    std::optional<MagnitudeTask> m_magnitudeStage;
    std::optional<OrderedTask> m_orderedStage;
    EventRecorder & m_recorder;
    void tick();
};
} // namespace stage1
