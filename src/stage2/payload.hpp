/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file payload.hpp
 * @brief Handshake payload and shared stage 1 building blocks.
 */
#pragma once
#include "../stage1/parser.hpp"
#include "../stage1/output.hpp"

#include <systemc>

namespace stage2 {
using stage1::CLOCK_PERIOD_NS;
using stage1::currentCycle;
using stage1::MagnitudeTask;
using stage1::Output;
using stage1::Parser;
using stage1::RawTask;
using stage1::Result;
using stage1::TestEventLog;

struct Payload {
    std::uint64_t m_id{};
    std::uint64_t m_a{};
    std::uint64_t m_b{};
    bool operator==(const Payload& other) const {
        return m_id == other.m_id && m_a == other.m_a && m_b == other.m_b;
    }
};
std::ostream& operator<<(std::ostream& stream, const Payload& task);
void sc_trace(sc_core::sc_trace_file* trace, const Payload& task, const std::string& name);
} // namespace stage2
