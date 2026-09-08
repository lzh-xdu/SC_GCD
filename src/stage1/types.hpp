/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file types.hpp
 * @brief Defines task payloads, the cycle unit and a passive diagnostic observer.
 *
 * Interface:
 *
 * RawTask --> MagnitudeTask --> OrderedTask --> Result
 *                          events |
 *                                 v
 *                          +--------------+ --> optional event CSV
 *                          | TestEventLog | --> TaskStatistics
 *                          +--------------+
 *
 * Protocol:
 * - RawTask holds a sequential id and signed 32-bit operands; all transformed magnitudes/results use uint64_t.
 * - MagnitudeTask is unsorted; OrderedTask requires a >= b >= 0 and preserves the input id.
 * - The wide representation accommodates abs(INT32_MIN)=2147483648 without narrowing.
 * - Model Assumption (outside spec): zero-input GCD returns the other magnitude; gcd(0,0)=0.
 * - TestEventLog cannot change module state or timing; record requires a non-null event name.
 * - A configured event stream must remain alive and writable; statistics are recorded even without an event stream.
 *
 * Timing:
 * - Payloads themselves impose no transfer protocol or latency; their connected modules define both.
 * - CLOCK_PERIOD_NS=1 ns; currentCycle floors current simulation time in that unit and returns zero at time zero.
 * - Observation and cycle queries do not advance simulation time.
 *
 * Reset:
 * - No reset protocol for these plain values or helper functions.
 * - Payload members default to zero; a new TestEventLog has a null stream and empty statistics.
 */
#pragma once

#include "../common/task_statistics.hpp"

#include <cstdint>
#include <iosfwd>

namespace stage1 {
inline constexpr double CLOCK_PERIOD_NS = 1.0;
/**
 * @brief 文件输入任务：从零递增的 id 和两个有符号 32 位整数。
 */
struct RawTask {
    std::uint64_t m_id{};
    std::int32_t m_a{};
    std::int32_t m_b{};
};
/**
 * @brief 取绝对值后的中间槽；宽类型安全容纳 INT32_MIN 的幅值，尚未排序。
 */
struct MagnitudeTask {
    std::uint64_t m_id{};
    std::uint64_t m_a{};
    std::uint64_t m_b{};
};
/**
 * @brief 计算输入：保留原 id，幅值满足 a >= b；零值合法。
 */
struct OrderedTask {
    std::uint64_t m_id{};
    std::uint64_t m_a{};
    std::uint64_t m_b{};
};
/**
 * @brief 最终非负 GCD 及原 id；(0, 0) 的 GCD 约定为零。
 */
struct Result {
    std::uint64_t m_id{};
    std::uint64_t m_gcd{};
};
std::ostream& operator<<(std::ostream&, const RawTask&);
std::ostream& operator<<(std::ostream&, const OrderedTask&);
std::ostream& operator<<(std::ostream&, const Result&);

// Diagnostic observer only: it cannot change module state or timing.
struct TestEventLog {
    std::ostream* m_testStream = nullptr;
    mutable TaskStatistics m_statistics;
    /**
     * @brief testEvent 不得为空指针；未配置流时仍统计延迟，配置的流须存活且可写。
     */
    void record(std::uint64_t testId, const char* testEvent, std::uint64_t testA = 0, std::uint64_t testB = 0,
                std::uint64_t testValue = 0, std::uint64_t testLatency = 0) const;
};
/**
 * @brief 当前仿真时间除以 1 ns，向下取整；零时刻返回零，不推进仿真。
 */
std::uint64_t currentCycle();
} // namespace stage1
