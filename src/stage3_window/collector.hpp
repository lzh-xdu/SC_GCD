/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file collector.hpp
 * @brief Finite tagged result window with issue-time reservations.
 */
#pragma once
#include "dispatcher.hpp"

#include <optional>
#include <vector>

namespace stage3_window {
/**
 * Compute result FIFOs -> [ W slots: id%W, optional<Result> ] -> Output FIFO
 *                         [ base=m_nextId; reserved at issue ] -> Dispatcher base
 * 仅clk.pos()推进。每沿最多收一个、发一个；先发旧槽再收，禁止同沿穿透。
 * 只发base对应结果；派发时保留[base,base+W)槽，最早任务总能写入预留槽。
 * 完成结果数与已预留但未完成的任务数分开统计；窗口满阻止新派发。
 * 基址信号在delta更新，释放的credit最早下一沿可用；无无限重排表。
 */
SC_MODULE(Collector) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_vector<sc_core::sc_fifo_in<stage1::Result>> m_resultsIn{"results_in", UNIT_COUNT};
    sc_core::sc_fifo_out<stage1::Result> m_resultsOut{"results_out"};
    sc_core::sc_out<std::uint64_t> m_baseOut{"base_out"};
    std::uint64_t m_nextId = 0;
    std::uint64_t m_orderWaitCycles = 0;
    std::uint64_t m_outputBlockedCycles = 0;
    unsigned occupancy() const {
        return m_completed;
    }
    /// @brief window 是预留槽数量，须与 Dispatcher 一致；日志引用须覆盖模块寿命。
    /// @throws std::invalid_argument window 为零；越窗、重复或错序结果抛 logic_error。
    Collector(sc_core::sc_module_name name, stage1::TestEventLog & testEventLog, unsigned window);

private:
    stage1::TestEventLog& m_testEventLog;
    std::vector<std::optional<stage1::Result>> m_slots;
    unsigned m_completed = 0;
    unsigned m_pollTurn = 0;
    void retire();
    void receive();
    void tick();
};
} // namespace stage3_window
