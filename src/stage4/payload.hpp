/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file payload.hpp
 * @brief Defines the valid/ready task payload and uses the local stage 4 building blocks.
 *
 * Interface:
 *
 *                  +-------------------------+
 * data (Payload) ->| receiver samples payload|
 * valid          ->|     valid && ready      |
 * ready          <-|                         |
 *                  +-------------------------+
 *
 * Protocol:
 * - Payload contains the original id and ordered nonnegative magnitudes a >= b.
 * - When valid is false, data has no transaction meaning.
 * - The sender holds data/valid while stalled; connected modules implement the handshake.
 * - Equality compares id and both operands; stream output and sc_trace support inspection.
 * - Parser, Output, raw/result types, the observer and simulation-time helpers are reused from stage4.
 *
 * Timing:
 * - Payload is a plain value, with no clock process or latency of its own.
 * - Connected modules transfer it at scheduled boundaries where valid && ready is true.
 *
 * Reset:
 * - No reset protocol in Payload; default construction initializes all fields to zero.
 * - Connected module documentation specifies initial valid/ready values.
 */
#pragma once
#include "parser.hpp"
#include "output.hpp"

#include <systemc>

namespace stage4 {

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
} // namespace stage4
