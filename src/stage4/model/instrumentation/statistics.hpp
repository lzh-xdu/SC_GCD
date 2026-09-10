/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file statistics.hpp
 * @brief Groups passive module counters and sampled resource occupancy.
 *
 * Interface:
 *
 * module observations --> [ counters / Usage ] --> report
 *
 * Protocol:
 * - Counter values are observational; do not use them to drive acceptance, completion or arbitration.
 * - TaskStatistics records event-boundary latencies independently of optional trace output.
 * - Parser/Output sequence numbers and Collector tags remain model state, outside these counters.
 *
 * Timing:
 * - Sample occupancy after channel updates; counters never advance simulation time.
 *
 * Reset:
 * - Value initialization clears counters; Usage starts with zero sum, peak and last occupancy.
 */
#pragma once

#include "../../common/task_statistics.hpp"

#include <cstdint>

namespace stage4::model::instrumentation {
struct ComputeStatistics {
    std::uint64_t m_busyCycles = 0;
    std::uint64_t m_idleNoInputCycles = 0;
    std::uint64_t m_resultWaitCycles = 0;
    std::uint64_t m_accepted = 0;
};
struct LinkStatistics {
    std::uint64_t m_validCycles = 0;
    std::uint64_t m_blockedCycles = 0;
};
struct DispatchStatistics {
    std::uint64_t m_dispatched = 0;
    std::uint64_t m_windowBlockedCycles = 0;
    std::uint64_t m_windowBlockedWithReadyCycles = 0;
    std::uint64_t m_engineBlockedCycles = 0;
    std::uint64_t m_randomChoices = 0;
};
struct CollectorStatistics {
    std::uint64_t m_orderWaitCycles = 0;
    std::uint64_t m_outputBlockedCycles = 0;
};
struct Usage {
    std::uint64_t m_sum = 0;
    std::uint64_t m_peak = 0;
    std::uint64_t m_last = 0;
    void sample(std::uint64_t count);
    void hold(std::uint64_t cycles);
};
} // namespace stage4::model::instrumentation
