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
 * @brief Computes GCD of ordered operand magnitudes with modeled modulo latency.
 *
 * Interface:
 *
 * m_tasksIn --> +---------------------------+ --> m_resultsOut
 * (OrderedTask) | IDLE / BUSY / PENDING     |     (Result FIFO)
 * m_clk ------> |         Compute           |
 *               +---------------------------+
 *
 * Protocol:
 * - Accept at most one task while IDLE; require a >= b >= 0.
 * - Keep one task/result and a remaining-cycle counter, with no internal task queue.
 * - If the output FIFO is full, retain the result in RESULT_PENDING.
 * - m_statistics counts computation and blocked result delivery separately.
 * - EventRecorder observes events only and must outlive the module; external FIFOs default to depth 2.
 *
 * Timing:
 * - SC_METHOD(tick) runs only on m_clk.pos(), with dont_initialize; T=1 ns, first edge at 1 ns.
 * - Accept at edge k and complete at k+L; L sums max(1, bits(a)-bits(b)+1) over modulo steps.
 * - Model Assumption (outside spec): b==0 skips modulo and its latency formula; zero steps give L=0.
 * - For nonnegative magnitudes, gcd(a,0)=a, gcd(0,b)=b and gcd(0,0)=0.
 * - L=0 completes and attempts delivery on the acceptance edge; numerical precomputation is not early completion.
 * - Neither a completion edge nor a later delivery edge accepts another task.
 * - After delivery, accept at the next edge at earliest; the FIFO consumer also waits until the next edge.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction initializes IDLE, a zero result/countdown and zero counters.
 */
SC_MODULE(Compute) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<OrderedTask> m_tasksIn{"tasks_in"};
    sc_core::sc_fifo_out<Result> m_resultsOut{"results_out"};
    model::instrumentation::ComputeStatistics m_statistics;
    /**
     * @brief 是否可在后续上升沿接收任务；待交付结果也算非空闲。
     */
    bool idle() const {
        return m_state == State::IDLE;
    }
    Compute(sc_core::sc_module_name name, EventRecorder & recorder);

private:
    enum class State { IDLE, BUSY, RESULT_PENDING };
    State m_state = State::IDLE;
    Result m_result;
    std::uint64_t m_remaining = 0;
    EventRecorder & m_recorder;
    /**
     * @brief 上升沿推进；BUSY 必须有剩余周期，异常状态抛 logic_error。
     */
    void tick();
    /**
     * @brief 仅在 RESULT_PENDING 尝试交付；FIFO 满是正常背压并保留结果。
     */
    void deliver();
};
} // namespace stage1
