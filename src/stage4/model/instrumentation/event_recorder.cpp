/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file event_recorder.cpp
 * @brief Observes model events without participating in flow control.
 */
#include "event_recorder.hpp"
#include "../timing/simulation_time.hpp"
#include "../../common/contract.hpp"

#include <charconv>
#include <limits>
#include <ostream>
#include <queue>

namespace stage4::model::instrumentation {
namespace {
constexpr std::size_t TRACE_BUFFER_BYTES = 64 * 1024;
constexpr std::size_t DECIMAL_BUFFER_BYTES = std::numeric_limits<std::uint64_t>::digits10 + 1;

void appendNumber(std::string& buffer, std::uint64_t value, char separator) {
    char digits[DECIMAL_BUFFER_BYTES];
    const auto converted = std::to_chars(digits, digits + DECIMAL_BUFFER_BYTES, value);
    buffer.append(digits, converted.ptr);
    buffer.push_back(separator);
}
} // namespace
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
    m_trace.push_back({cycle, 1, id, std::string(event), a, b, value, latency});
}
void EventRecorder::repeat(std::uint64_t first, std::uint64_t count, std::uint64_t id, std::string_view event,
                           std::uint64_t a, std::uint64_t b, std::uint64_t value) {
    // Own the event name, including views into caller-local strings. Expand only while writing.
    if (m_testStream && count != 0) {
        m_trace.push_back({first, count, id, std::string(event), a, b, value, 0});
    }
}
void EventRecorder::writeBuffer() {
    if (m_traceBuffer.empty()) {
        return;
    }
    m_testStream->write(m_traceBuffer.data(), static_cast<std::streamsize>(m_traceBuffer.size()));
    assertCondition(static_cast<bool>(*m_testStream), "event stream is not writable");
    m_traceBuffer.clear();
}
void EventRecorder::writeRow(const TraceRow& row) {
    appendNumber(m_traceBuffer, row.m_id, ',');
    m_traceBuffer.append(row.m_event);
    m_traceBuffer.push_back(',');
    appendNumber(m_traceBuffer, row.m_cycle, ',');
    appendNumber(m_traceBuffer, row.m_a, ',');
    appendNumber(m_traceBuffer, row.m_b, ',');
    appendNumber(m_traceBuffer, row.m_value, ',');
    appendNumber(m_traceBuffer, row.m_latency, '\n');
    if (m_traceBuffer.size() >= TRACE_BUFFER_BYTES) {
        writeBuffer();
    }
}
void EventRecorder::flushBatch() {
    if (m_trace.empty()) {
        return;
    }
    // Original insertion index breaks same-cycle ties exactly like the old stable_sort.
    const auto later = [this](std::size_t left, std::size_t right) {
        return m_trace[left].m_cycle != m_trace[right].m_cycle ? m_trace[left].m_cycle > m_trace[right].m_cycle
                                                               : left > right;
    };
    std::priority_queue<std::size_t, std::vector<std::size_t>, decltype(later)> pending(later);
    for (std::size_t index = 0; index < m_trace.size(); ++index) {
        pending.push(index);
    }
    while (!pending.empty()) {
        const auto index = pending.top();
        pending.pop();
        auto& row = m_trace[index];
        writeRow(row);
        if (--row.m_count != 0) {
            ++row.m_cycle;
            pending.push(index);
        }
    }
    m_trace.clear();
}
void EventRecorder::flushTrace() {
    flushBatch();
    writeBuffer();
}
} // namespace stage4::model::instrumentation
