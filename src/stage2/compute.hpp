/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file compute.hpp
 * @brief Single-task GCD engine with explicit acceptance.
 */
#pragma once
#include "payload.hpp"

namespace stage2 {
/**
 * data/valid -> [ IDLE -> BUSY -> RESULT_PENDING ] -> Result FIFO(D)
 * ready     <- [ one task/result + countdown    ]
 *                          clk.pos()
 * 接收只在上升沿 valid&&ready；k 接收、k+L 完成，完成沿不接下一项。
 * 结果满时保持一份结果、ready=false；成功写出后下一沿可接收。
 * L=0 在接收沿完成该项；初始 ready=true；内部无额外队列。
 */
SC_MODULE(Compute) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_in<Payload> m_dataIn{"data_in"};
    sc_core::sc_in<bool> m_validIn{"valid_in"};
    sc_core::sc_out<bool> m_readyOut{"ready_out"};
    sc_core::sc_fifo_out<Result> m_resultsOut{"results_out"};
    std::uint64_t m_busyCycles = 0;
    std::uint64_t m_idleNoInputCycles = 0;
    std::uint64_t m_resultWaitCycles = 0;
    std::uint64_t m_accepted = 0;
    bool idle() const {
        return m_state == State::IDLE;
    }
    Compute(sc_core::sc_module_name name, TestEventLog & testEventLog, unsigned unit = 0);

private:
    enum class State { IDLE, BUSY, RESULT_PENDING };
    State m_state = State::IDLE;
    Result m_result;
    std::uint64_t m_remaining = 0;
    TestEventLog & m_testEventLog;
    unsigned m_unit;
    void tick();
    void deliver();
    void accept();
};
} // namespace stage2
