/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file compute.hpp
 * @brief Single-task GCD engine with explicit acceptance.
 */
#pragma once
#include "payload.hpp"

#include "types.hpp"

namespace stage4 {
/**
 * @brief Computes one GCD at a time using an explicit valid/ready acceptance interface.
 *
 * Interface:
 *
 * m_dataIn / m_validIn --> +-----------------------+ --> m_resultsOut (Result FIFO)
 * m_readyOut          <--  |       Compute         |
 * event deadline      -->  | IDLE / BUSY / PENDING |
 *                          +-----------------------+
 *
 * Protocol:
 * - Accept only at a scheduled boundary with m_validIn && m_readyOut; require ordered nonnegative magnitudes.
 * - Keep one task/result and an absolute completion deadline, with no extra task queue.
 * - While computing or holding a blocked result, ready is false and the result is retained.
 * - The observer must outlive the module; unit labels the engine in diagnostic events.
 * - Busy cycles and result-wait cycles are counted separately.
 *
 * Timing:
 * - System schedules advance() at the next actionable timestamp; unchanged intervals are skipped.
 * - Accept at k and complete at k+L; L sums max(1, bits(a)-bits(b)+1) over modulo steps.
 * - Model Assumption (outside spec): b==0 skips modulo and its latency formula; zero steps give L=0.
 * - For nonnegative magnitudes, gcd(a,0)=a, gcd(0,b)=b and gcd(0,0)=0.
 * - L=0 completes and attempts delivery at acceptance; completion/delivery boundaries accept no other task.
 * - After successful result delivery, the next boundary is the earliest new acceptance.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction initializes IDLE, m_readyOut=true, a zero result/deadline and zero counters.
 */
SC_MODULE(Compute) {
    sc_core::sc_in<Payload> m_dataIn{"data_in"};
    sc_core::sc_in<bool> m_validIn{"valid_in"};
    sc_core::sc_out<bool> m_readyOut{"ready_out"};
    sc_core::sc_fifo_out<Result> m_resultsOut{"results_out"};
    model::instrumentation::ComputeStatistics m_statistics;
    /**
     * @brief 只读内部空闲状态；实际接收仍以该调度边界 valid && ready 为准。
     */
    [[nodiscard]] bool idle() const {
        return m_state == State::IDLE;
    }
    Compute(sc_core::sc_module_name name, EventRecorder & recorder, unsigned unit = 0);

    /**
     * @brief 事件推进；BUSY 不得超过完成时刻，异常状态抛 logic_error。
     */
    void advance();
    [[nodiscard]] std::uint64_t nextDelay() const;
    void accountSkipped(std::uint64_t first, std::uint64_t count);

private:
    enum class State { IDLE, BUSY, RESULT_PENDING };
    State m_state = State::IDLE;
    Result m_result;
    std::uint64_t m_completeCycle = 0;
    EventRecorder & m_recorder;
    unsigned m_unit;
    /**
     * @brief 仅在 RESULT_PENDING 尝试交付；FIFO 满是正常背压并保留结果。
     */
    void deliver();
    /**
     * @brief 仅在 IDLE 且 valid && ready 时调用；输入幅值必须有序。
     */
    void accept();
};
} // namespace stage4
