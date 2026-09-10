/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file event_recorder.cpp
 * @brief Observes model events without participating in flow control.
 */
#include "event_recorder.hpp"
#include "../timing/clock.hpp"
#include "../../common/contract.hpp"

#include <charconv>
#include <limits>
#include <ostream>

namespace model::instrumentation {
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

void EventRecorder::flushTrace() const {
    if (m_traceBuffer.empty()) {
        return;
    }
    m_testStream->write(m_traceBuffer.data(), static_cast<std::streamsize>(m_traceBuffer.size()));
    assertCondition(static_cast<bool>(*m_testStream), "event stream is not writable");
    m_traceBuffer.clear();
}

void EventRecorder::writeBuffered(std::uint64_t cycle, std::uint64_t id, std::string_view event, std::uint64_t a,
                                  std::uint64_t b, std::uint64_t value, std::uint64_t latency) const {
    appendNumber(m_traceBuffer, id, ',');
    m_traceBuffer.append(event.data(), event.size());
    m_traceBuffer.push_back(',');
    appendNumber(m_traceBuffer, cycle, ',');
    appendNumber(m_traceBuffer, a, ',');
    appendNumber(m_traceBuffer, b, ',');
    appendNumber(m_traceBuffer, value, ',');
    appendNumber(m_traceBuffer, latency, '\n');
    if (m_traceBuffer.size() >= TRACE_BUFFER_BYTES) {
        flushTrace();
    }
}

void EventRecorder::record(std::uint64_t id, std::string_view event, std::uint64_t a, std::uint64_t b,
                           std::uint64_t value, std::uint64_t latency) const {
    assertCondition(m_testStream == nullptr || static_cast<bool>(*m_testStream), "event stream is not writable");
    const auto cycle = timing::currentCycle();
    m_statistics.record(id, event, cycle);
    if (m_testStream && m_testBufferedTrace) {
        // Clocked records already arrive in CSV order; no interval merge is needed.
        writeBuffered(cycle, id, event, a, b, value, latency);
    } else if (m_testStream) {
        *m_testStream << id << ',' << event << ',' << cycle << ',' << a << ',' << b << ',' << value << ',' << latency
                      << '\n';
    }
}
} // namespace model::instrumentation
