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
 * - Recording mutates the task statistics and trace buffers; the methods are non-const by design.
 * - Statistics remain enabled when the trace stream is null; trace-off is not a total instrumentation switch.
 * - This observer owns no model state and must outlive modules borrowing it.
 *
 * Timing:
 * - Read the current cycle without advancing simulation time or modifying scheduling.
 * - flushBatch merges complete time intervals; later batches must not precede already emitted cycles.
 * - Skipped control intervals stay compact until CSV emission; flushTrace writes the final buffered bytes.
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
#include <vector>

namespace stage4::model::instrumentation {
struct EventRecorder {
    struct TraceRow {
        std::uint64_t m_cycle;
        std::uint64_t m_count;
        std::uint64_t m_id;
        std::string m_event;
        std::uint64_t m_a;
        std::uint64_t m_b;
        std::uint64_t m_value;
        std::uint64_t m_latency;
    };
    std::ostream* m_testStream = nullptr;
    TaskStatistics m_statistics;
    std::vector<TraceRow> m_trace;
    std::string m_traceBuffer;
    void record(std::uint64_t id, std::string_view event, std::uint64_t a = 0, std::uint64_t b = 0,
                std::uint64_t value = 0, std::uint64_t latency = 0);
    void repeat(std::uint64_t first, std::uint64_t count, std::uint64_t id, std::string_view event, std::uint64_t a = 0,
                std::uint64_t b = 0, std::uint64_t value = 0);
    void flushTrace();
    void flushBatch();
    void writeRow(const TraceRow& row);
    void writeBuffer();
    void append(std::uint64_t cycle, std::uint64_t id, std::string_view event, std::uint64_t a, std::uint64_t b,
                std::uint64_t value, std::uint64_t latency);
};
} // namespace stage4::model::instrumentation
