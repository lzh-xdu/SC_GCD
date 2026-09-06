/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file transform.hpp
 * @brief Two-stage transform with a held valid/ready transfer.
 */
#pragma once
#include "payload.hpp"

#include <optional>

namespace stage2 {
/**
 * FIFO(D) -> [ magnitude slot -> ordered slot ] -> m_dataOut/m_validOut
 *                      clk.pos()              <- m_readyIn
 * 两槽各一项；k 沿接收，最早 k+2 沿握手，每沿至多一项。
 * SC_METHOD 仅上升沿执行；未握手保持排序槽和 data/valid，上游满则停止读。
 * 排序槽与输出信号是同一份架构存储的模型表示，不重复计为缓存。
 */
SC_MODULE(Transform) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<RawTask> m_tasksIn{"tasks_in"};
    sc_core::sc_out<Payload> m_dataOut{"data_out"};
    sc_core::sc_out<bool> m_validOut{"valid_out"};
    sc_core::sc_in<bool> m_readyIn{"ready_in"};
    std::uint64_t m_validCycles = 0;
    std::uint64_t m_blockedCycles = 0;
    bool empty() const {
        return !m_magnitudeStage && !m_orderedStage;
    }
    unsigned occupancy() const {
        return static_cast<unsigned>(m_magnitudeStage.has_value()) + static_cast<unsigned>(m_orderedStage.has_value());
    }
    Transform(sc_core::sc_module_name name, TestEventLog & testEventLog);

private:
    std::optional<MagnitudeTask> m_magnitudeStage;
    std::optional<Payload> m_orderedStage;
    TestEventLog & m_testEventLog;
    void tick();
};
} // namespace stage2
