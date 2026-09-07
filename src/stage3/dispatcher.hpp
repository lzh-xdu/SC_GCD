/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file dispatcher.hpp
 * @brief Strict round-robin routing with edge-based selection.
 */
#pragma once
#include "../stage2/payload.hpp"

namespace stage3 {
inline constexpr unsigned UNIT_COUNT = 2;
/**
 * Transform data/valid -> [ turn=0/1, no data buffer ] -> Compute[0/1] data/valid
 *           ready     <- [ combinational routing   ] <- Compute[0/1] ready
 * route 对数据/选择/ready敏感，仅组合接线；tick只在clk.pos()更新轮次。
 * 成功握手才0/1交替，被选单元忙时等待；无额外延迟、无内部任务缓存。
 */
SC_MODULE(Dispatcher) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_in<stage2::Payload> m_dataIn{"data_in"};
    sc_core::sc_in<bool> m_validIn{"valid_in"};
    sc_core::sc_out<bool> m_readyOut{"ready_out"};
    sc_core::sc_vector<sc_core::sc_out<stage2::Payload>> m_dataOut{"data_out", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_out<bool>> m_validOut{"valid_out", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_in<bool>> m_readyIn{"ready_in", UNIT_COUNT};
    std::uint64_t m_idleOtherBlockedCycles = 0;
    /// @brief 初始选择 Compute0；所有端口须在 sc_start 前绑定，日志引用须覆盖模块寿命。
    Dispatcher(sc_core::sc_module_name name, stage1::TestEventLog & testEventLog);

private:
    sc_core::sc_signal<unsigned> m_turn{"turn", 0};
    stage1::TestEventLog& m_testEventLog;
    void route();
    void tick();
};
} // namespace stage3
