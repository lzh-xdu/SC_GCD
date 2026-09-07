/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file dispatcher.hpp
 * @brief Ready-first arbitration with reproducible random ties and window credits.
 */
#pragma once
#include "../stage2/payload.hpp"

namespace stage3_window {
inline constexpr unsigned UNIT_COUNT = 2;
/**
 * data/valid -> [ window credit + ready selection ] -> Compute[0/1]
 * ready     <- [ both ready: seeded random choice ] <- ready[0/1]
 * base      <- Collector (next result to retire)
 * route为组合接线；仅成功握手的上升沿推进随机状态/计数，无任务缓存。
 * id-base<W才能派发，提前预留结果槽；无ready或窗口满则上游保持。
 * 只有接收端ready时才向它给valid，避免已声明但未接收的传输换目的地。
 */
SC_MODULE(Dispatcher) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_in<stage2::Payload> m_dataIn{"data_in"};
    sc_core::sc_in<bool> m_validIn{"valid_in"};
    sc_core::sc_out<bool> m_readyOut{"ready_out"};
    sc_core::sc_in<std::uint64_t> m_baseIn{"base_in"};
    sc_core::sc_vector<sc_core::sc_out<stage2::Payload>> m_dataOut{"data_out", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_out<bool>> m_validOut{"valid_out", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_in<bool>> m_readyIn{"ready_in", UNIT_COUNT};
    std::uint64_t m_dispatched = 0;
    std::uint64_t m_windowBlockedCycles = 0;
    std::uint64_t m_windowBlockedWithReadyCycles = 0;
    std::uint64_t m_engineBlockedCycles = 0;
    std::uint64_t m_randomChoices = 0;
    Dispatcher(sc_core::sc_module_name name, stage1::TestEventLog & testEventLog, unsigned window, std::uint32_t seed);

private:
    sc_core::sc_signal<std::uint32_t> m_randomState{"random_state"};
    stage1::TestEventLog& m_testEventLog;
    unsigned m_window;
    bool hasCredit() const;
    unsigned selectedUnit() const;
    void route();
    void tick();
};
} // namespace stage3_window
