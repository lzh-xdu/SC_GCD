/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file event_recorder.cpp
 * @brief Observes model events without participating in flow control.
 */
#include "event_recorder.hpp"
#include "../timing/clock.hpp"
#include "../../common/contract.hpp"

#include <ostream>

namespace model::instrumentation {
void EventRecorder::record(std::uint64_t id, const char* event, std::uint64_t a, std::uint64_t b, std::uint64_t value,
                           std::uint64_t latency) const {
    assertCondition<std::invalid_argument>(event != nullptr, "null event name");
    assertCondition(m_testStream == nullptr || static_cast<bool>(*m_testStream), "event stream is not writable");
    const auto cycle = timing::currentCycle();
    m_statistics.record(id, event, cycle);
    if (m_testStream) {
        *m_testStream << id << ',' << event << ',' << cycle << ',' << a << ',' << b << ',' << value << ',' << latency
                      << '\n';
    }
}
} // namespace model::instrumentation
