/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file event_recorder.cpp
 * @brief Observes model events without participating in flow control.
 */
#include "event_recorder.hpp"
#include "../timing/simulation_time.hpp"
#include "../../common/contract.hpp"

#include <ostream>
#include <sstream>
#include <algorithm>

namespace stage4::model::instrumentation {
void EventRecorder::record(std::uint64_t id, std::string_view event, std::uint64_t a, std::uint64_t b,
                           std::uint64_t value, std::uint64_t latency) {
    assertCondition(m_testStream == nullptr || static_cast<bool>(*m_testStream), "event stream is not writable");
    const auto cycle = timing::currentCycle();
    m_statistics.record(id, event, cycle);
    append(cycle, id, event, a, b, value, latency);
}
void EventRecorder::append(std::uint64_t cycle, std::uint64_t id, std::string_view event, std::uint64_t a,
                           std::uint64_t b, std::uint64_t value, std::uint64_t latency) {
    if (!m_testStream) {
        return;
    }
    std::ostringstream row;
    row << id << ',' << event << ',' << cycle << ',' << a << ',' << b << ',' << value << ',' << latency << '\n';
    m_trace.push_back({cycle, row.str()});
}
void EventRecorder::repeat(std::uint64_t first, std::uint64_t count, std::uint64_t id, std::string_view event,
                           std::uint64_t a, std::uint64_t b, std::uint64_t value) {
    // Expand legacy per-cycle diagnostics only when tracing. This never advances model time.
    if (m_testStream) {
        for (std::uint64_t offset = 0; offset < count; ++offset) {
            append(first + offset, id, event, a, b, value, 0);
        }
    }
}
void EventRecorder::flushTrace() {
    if (!m_testStream) {
        return;
    }
    std::stable_sort(m_trace.begin(), m_trace.end(),
                     [](const TraceRow& left, const TraceRow& right) { return left.m_cycle < right.m_cycle; });
    for (const auto& row : m_trace) {
        *m_testStream << row.m_text;
    }
    m_trace.clear();
}
} // namespace stage4::model::instrumentation
