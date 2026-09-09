/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.hpp
 * @brief Finite tagged result window with issue-time reservations.
 */
#pragma once
#include "dispatcher.hpp"

#include <optional>
#include <vector>

namespace stage4 {
/**
 * @brief Stores completed results in a finite reserved window and retires them in order.
 *
 * Interface:
 *
 * m_resultsIn[0/1] --> +---------------------------+ --> m_resultsOut
 * m_clk           -->  | W slots, indexed by id%W  | --> m_baseOut (to Dispatcher)
 *                      |        Collector          |
 *                      +---------------------------+
 *
 * Protocol:
 * - Dispatch reserves ids in [base, base+W); accept completed results into their reserved slots.
 * - Emit only the result tagged m_nextId; advance base after successful output.
 * - The oldest task always has a reserved slot; no unbounded reorder table is used.
 * - Completed-result occupancy is distinct from reserved but unfinished tasks.
 *   A full reservation window blocks dispatch.
 * - window must be nonzero and match Dispatcher; zero throws invalid_argument.
 * - Out-of-window, duplicate/colliding or wrong-head tags throw logic_error; the observer must outlive the module.
 *
 * Timing:
 * - SC_METHOD(tick) runs only on m_clk.pos(), with dont_initialize.
 * - Retire an old slot before receiving a result: at most one emission and one reception per edge.
 * - A newly received result cannot retire on the same edge.
 * - m_baseOut updates in a delta cycle; freed dispatch capacity is usable at the next edge at earliest.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction creates W empty slots, initializes m_baseOut/m_nextId=0, and zeroes occupancy, poll turn and counters.
 */
SC_MODULE(Collector) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_vector<sc_core::sc_fifo_in<stage4::Result>> m_resultsIn{"results_in", UNIT_COUNT};
    sc_core::sc_fifo_out<stage4::Result> m_resultsOut{"results_out"};
    sc_core::sc_out<std::uint64_t> m_baseOut{"base_out"};
    std::uint64_t m_nextId = 0;
    model::instrumentation::CollectorStatistics m_statistics;
    unsigned occupancy() const {
        return m_completed;
    }
    /**
     * @brief window 是预留槽数量，须与 Dispatcher 一致；日志引用须覆盖模块寿命。
     * @throws std::invalid_argument window 为零；越窗、重复或错序结果抛 logic_error。
     */
    Collector(sc_core::sc_module_name name, stage4::EventRecorder & recorder, unsigned window);

private:
    stage4::EventRecorder& m_recorder;
    std::vector<std::optional<stage4::Result>> m_slots;
    unsigned m_completed = 0;
    unsigned m_pollTurn = 0;
    void retire();
    void receive();
    void tick();
};
} // namespace stage4
