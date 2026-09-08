/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file payload.hpp
 * @brief Defines the valid/ready task payload and reuses stage 1 building blocks.
 *
 * Interface:
 *
 *                  +------------------------+
 * data (Payload) ->| receiver samples at edge|
 * valid          ->|     valid && ready     |
 * ready          <-|                        |
 *                  +------------------------+
 *
 * Protocol:
 * - Payload contains the original id and ordered nonnegative magnitudes a >= b.
 * - When valid is false, data has no transaction meaning.
 * - The sender holds data/valid while stalled; connected modules implement the handshake.
 * - Equality compares id and both operands; stream output and sc_trace support inspection.
 * - Parser, Output, raw/result types, the observer and clock helpers are reused from stage1.
 *
 * Timing:
 * - Payload is a plain value, with no clock process or latency of its own.
 * - Connected modules transfer it at rising edges where valid && ready is true.
 *
 * Reset:
 * - No reset protocol in Payload; default construction initializes all fields to zero.
 * - Connected module documentation specifies initial valid/ready values.
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

/**
 * @brief 握手任务：id 是原始顺序号，a >= b 为非负幅值；valid 为 false 时数据无业务含义。
 */
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
