/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file event_recorder.hpp
 * @brief Routes model observations to optional trace output and task statistics.
 *
 * Interface:
 *
 * id / event / values --> [ EventRecorder ] --> optional event CSV
 *                                           --> TaskStatistics
 *
 * Protocol:
 * - record accepts any event name view; a configured stream must remain alive and writable.
 * - Statistics remain enabled when the trace stream is null; trace-off is not a total instrumentation switch.
 * - This observer owns no model state and must outlive modules borrowing it.
 *
 * Timing:
 * - Read the current cycle without advancing simulation time or modifying scheduling.
 *
 * Reset:
 * - A new recorder has a null trace stream and empty task statistics; no runtime reset protocol.
 */
#pragma once

#include "statistics.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace model::instrumentation {
struct EventRecorder {
    std::ostream* m_testStream = nullptr;
    mutable TaskStatistics m_statistics;
    // Stage 3 opts into the same decimal encoder and byte-buffer threshold as Stage 4.
    // Call flushTrace at drain; stages 1/2 retain their original direct stream path.
    bool m_testBufferedTrace = false;
    mutable std::string m_traceBuffer;
    void flushTrace() const;
    void writeBuffered(std::uint64_t cycle, std::uint64_t id, std::string_view event, std::uint64_t a, std::uint64_t b,
                       std::uint64_t value, std::uint64_t latency) const;
    void record(std::uint64_t id, std::string_view event, std::uint64_t a = 0, std::uint64_t b = 0,
                std::uint64_t value = 0, std::uint64_t latency = 0) const;
};
} // namespace model::instrumentation
