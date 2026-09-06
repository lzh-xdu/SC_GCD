/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file types.cpp
 * @brief Shared task printing and event observation.
 */
#include "types.hpp"

#include <systemc>
#include <ostream>

namespace stage1 {
std::ostream& operator<<(std::ostream& os, const RawTask& t) {
    return os << t.m_id << ':' << t.m_a << ',' << t.m_b;
}
std::ostream& operator<<(std::ostream& os, const OrderedTask& t) {
    return os << t.m_id << ':' << t.m_a << ',' << t.m_b;
}
std::ostream& operator<<(std::ostream& os, const Result& t) {
    return os << t.m_id << ':' << t.m_gcd;
}
std::uint64_t currentCycle() {
    return sc_core::sc_time_stamp().value() / sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS).value();
}
void TestEventLog::record(std::uint64_t testId, const char* testEvent, std::uint64_t testA, std::uint64_t testB,
                          std::uint64_t testValue, std::uint64_t testLatency) const {
    if (m_testStream) {
        *m_testStream << testId << ',' << testEvent << ',' << currentCycle() << ',' << testA << ',' << testB << ','
                      << testValue << ',' << testLatency << '\n';
    }
}
} // namespace stage1
