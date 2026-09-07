/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file types.hpp
 * @brief Shared task payloads, cycle unit and diagnostic observer.
 */
#pragma once

#include "../common/task_statistics.hpp"

#include <cstdint>
#include <iosfwd>

namespace stage1 {
inline constexpr double CLOCK_PERIOD_NS = 1.0;
struct RawTask {
    std::uint64_t m_id{};
    std::int32_t m_a{};
    std::int32_t m_b{};
};
struct MagnitudeTask {
    std::uint64_t m_id{};
    std::uint64_t m_a{};
    std::uint64_t m_b{};
};
struct OrderedTask {
    std::uint64_t m_id{};
    std::uint64_t m_a{};
    std::uint64_t m_b{};
};
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
    void record(std::uint64_t testId, const char* testEvent, std::uint64_t testA = 0, std::uint64_t testB = 0,
                std::uint64_t testValue = 0, std::uint64_t testLatency = 0) const;
};
std::uint64_t currentCycle();
} // namespace stage1
