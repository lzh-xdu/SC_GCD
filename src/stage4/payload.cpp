/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file payload.cpp
 * @brief Diagnostic support for a typed SystemC signal.
 */
#include "payload.hpp"

namespace stage4 {
std::ostream& operator<<(std::ostream& stream, const Payload& task) {
    return stream << task.m_id << ':' << task.m_a << ',' << task.m_b;
}
void sc_trace(sc_core::sc_trace_file* trace, const Payload& task, const std::string& name) {
    sc_core::sc_trace(trace, task.m_id, name + ".id");
    sc_core::sc_trace(trace, task.m_a, name + ".a");
    sc_core::sc_trace(trace, task.m_b, name + ".b");
}
} // namespace stage4
