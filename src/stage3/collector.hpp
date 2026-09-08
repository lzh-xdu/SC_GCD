/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.hpp
 * @brief Ordered merge of independent bounded result queues.
 */
#pragma once
#include "dispatcher.hpp"

namespace stage3 {
/**
 * @brief Merges two bounded result queues in original task order.
 *
 * Interface:
 *
 * m_resultsIn[0] --> +----------------------+ --> m_resultsOut (Result FIFO)
 * m_resultsIn[1] --> |      Collector       |
 * m_clk          --> | next id selects FIFO |
 *                    +----------------------+
 *
 * Protocol:
 * - Read from the engine selected by m_nextId % UNIT_COUNT only when output has space.
 * - Require the returned id to equal m_nextId; a mismatch throws logic_error.
 * - Hold later results in their independent input FIFOs until their turn.
 * - Separate input queues allow the older task's engine to write without a shared FIFO head blocking it.
 * - No internal result buffer; track the next id and order/output wait counters.
 * - The input mapping must match the round-robin Dispatcher; the observer must outlive the module.
 *
 * Timing:
 * - SC_METHOD(tick) runs only on m_clk.pos(), with dont_initialize.
 * - Move at most one result per rising edge; no same-edge bypass across external FIFOs.
 * - Input result FIFO capacity R and output FIFO capacity D are configured outside this module.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction initializes m_nextId=0 and zero wait counters.
 */
SC_MODULE(Collector) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_vector<sc_core::sc_fifo_in<stage1::Result>> m_resultsIn{"results_in", UNIT_COUNT};
    sc_core::sc_fifo_out<stage1::Result> m_resultsOut{"results_out"};
    std::uint64_t m_nextId = 0;
    model::instrumentation::CollectorStatistics m_statistics;
    /**
     * @brief 输入通道必须遵守 id % UNIT_COUNT 派发映射；读到非预期编号抛 logic_error。
     */
    Collector(sc_core::sc_module_name name, stage1::EventRecorder & recorder);

private:
    stage1::EventRecorder& m_recorder;
    void tick();
};
} // namespace stage3
