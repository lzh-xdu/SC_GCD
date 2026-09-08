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
 *                          +---------------+ --> optional event CSV
 *                          | EventRecorder | --> TaskStatistics
 *                          +---------------+
 *
 * Protocol:
 * - RawTask holds a sequential id and signed 32-bit operands; all transformed magnitudes/results use uint64_t.
 * - MagnitudeTask is unsorted; OrderedTask requires a >= b >= 0 and preserves the input id.
 * - The wide representation accommodates abs(INT32_MIN)=2147483648 without narrowing.
 * - Model Assumption (outside spec): zero-input GCD returns the other magnitude; gcd(0,0)=0.
 * - EventRecorder cannot change module state or timing; record requires a non-null event name.
 * - A configured event stream must remain alive and writable; statistics are recorded even without an event stream.
 *
 * Timing:
 * - Payloads themselves impose no transfer protocol or latency; their connected modules define both.
 * - CLOCK_PERIOD_NS=1 ns; currentCycle floors current simulation time in that unit and returns zero at time zero.
 * - Observation and cycle queries do not advance simulation time.
 *
 * Reset:
 * - No reset protocol for these plain values or helper functions.
 * - Payload members default to zero; a new EventRecorder has a null stream and empty statistics.
 */
#pragma once

#include "../model/instrumentation/event_recorder.hpp"
#include "../model/timing/clock.hpp"

#include <cstdint>
#include <iosfwd>

namespace stage1 {
using model::instrumentation::EventRecorder;
using model::timing::CLOCK_PERIOD_NS;
using model::timing::currentCycle;
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

} // namespace stage1
