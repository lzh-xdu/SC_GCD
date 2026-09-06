/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.hpp
 * @brief Ordered merge of independent bounded result queues.
 */
#pragma once
#include "dispatcher.hpp"

namespace stage3 {
/**
 * Compute0 -> FIFO(R) --\
 *                       [ next_id, select next_id%2 ] -> FIFO(D) -> Output
 * Compute1 -> FIFO(R) --/            clk.pos()
 * 每上升沿最多读写一个结果；只有对应输入有数据且输出有空位才移动。
 * 自身无结果缓存，只保存下一个编号；FIFO间传递不能同沿旁路。
 * 后编号先完成时留在所属FIFO；先编号通道可独立写入，避免共享队列头阻塞。
 */
SC_MODULE(Collector) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_vector<sc_core::sc_fifo_in<stage1::Result>> m_resultsIn{"results_in", UNIT_COUNT};
    sc_core::sc_fifo_out<stage1::Result> m_resultsOut{"results_out"};
    std::uint64_t m_nextId = 0;
    std::uint64_t m_orderWaitCycles = 0;
    std::uint64_t m_outputBlockedCycles = 0;
    Collector(sc_core::sc_module_name name, stage1::TestEventLog & testEventLog);

private:
    stage1::TestEventLog& m_testEventLog;
    void tick();
};
} // namespace stage3
