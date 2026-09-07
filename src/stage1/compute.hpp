/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file compute.hpp
 * @brief Compute module interface and cycle contract.
 */
#pragma once
#include "types.hpp"

#include <systemc>

namespace stage1 {
/**
 * @brief 计算模块：单任务 GCD，按规定的取余延迟模拟忙碌时间。
 *
 *                  +------------------------------------+
 * FIFO ----------> | IDLE -> BUSY -> RESULT_PENDING     | ----------> FIFO
 *  D    m_tasksIn  |         m_remaining                | m_resultsOut  D
 *       OrderedTask|         m_result (one task/result) | Result
 *                  +----------------^-------------------+
 *                                   | m_clk.pos()
 *
 * 触发：SC_METHOD(tick)，仅共同时钟上升沿；dont_initialize。
 * 时钟：T=1 ns，首沿 1 ns；一次只处理一条任务，不能把数值预计算当成提前完成。
 * 缓存：一份 m_result 和一个剩余周期计数，无内部任务队列；外部 FIFO 默认 D=2。
 * 周期：k 沿接收，k+L 沿完成；L 为各次 max(1,bits(a)-bits(b)+1) 的总和。
 *        无取余时 L=0（本实现约定）；完成沿可写出，但绝不接收另一条任务。
 *        写出后的下一沿才可接受新任务；Output 最早也在写出下一沿读取。
 * 背压：输出 FIFO 满则进入 RESULT_PENDING，保持结果；交付沿也不接收新任务。
 * 统计：m_busyCycles 记录计算周期，m_resultWaitCycles 单独记录结果等待。
 * 旁路观察：m_testEventLog 仅记录开始、完成、交付等事件。
 */
SC_MODULE(Compute) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<OrderedTask> m_tasksIn{"tasks_in"};
    sc_core::sc_fifo_out<Result> m_resultsOut{"results_out"};
    std::uint64_t m_busyCycles = 0;
    std::uint64_t m_idleNoInputCycles = 0;
    std::uint64_t m_resultWaitCycles = 0;
    /// @brief 是否可在后续上升沿接收任务；待交付结果也算非空闲。
    bool idle() const {
        return m_state == State::IDLE;
    }
    Compute(sc_core::sc_module_name name, TestEventLog & testEventLog);

private:
    enum class State { IDLE, BUSY, RESULT_PENDING };
    State m_state = State::IDLE;
    Result m_result;
    std::uint64_t m_remaining = 0;
    TestEventLog & m_testEventLog;
    /// @brief 上升沿推进；BUSY 必须有剩余周期，异常状态抛 logic_error。
    void tick();
    /// @brief 仅在 RESULT_PENDING 尝试交付；FIFO 满是正常背压并保留结果。
    void deliver();
};
} // namespace stage1
