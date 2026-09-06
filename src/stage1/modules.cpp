/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file modules.cpp
 * @brief Clocked stage 1 modules and their bounded-stream interfaces.
 */
#include "modules.hpp"
#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

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
static unsigned bits(std::uint64_t value) {
    unsigned count = 0;
    while (value != 0) {
        ++count;
        value >>= 1;
    }
    return count;
}
std::pair<std::uint64_t, std::uint64_t> gcdAndLatency(std::uint64_t a, std::uint64_t b) {
    std::uint64_t latency = 0;
    while (b != 0) {
        constexpr int MIN_REMAINDER_CYCLES = 1;
        constexpr int INCLUSIVE_BIT_POSITION = 1;
        latency += std::max(MIN_REMAINDER_CYCLES,
                            static_cast<int>(bits(a)) - static_cast<int>(bits(b)) + INCLUSIVE_BIT_POSITION);
        const auto remainder = a % b;
        a = b;
        b = remainder;
    }
    return {a, latency};
}
static std::uint64_t magnitude(std::int32_t value) {
    const auto wide = static_cast<std::int64_t>(value); // Promote before negating INT32_MIN.
    return static_cast<std::uint64_t>(wide < 0 ? -wide : wide);
}

Parser::Parser(sc_core::sc_module_name name, std::istream& input, TestEventLog& testEventLog)
    : sc_module(name)
    , m_input(input)
    , m_testEventLog(testEventLog) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Parser::tick() {
    if (m_eof || m_tasksOut.num_free() == 0) {
        return;
    }
    std::string line;
    if (!std::getline(m_input, line)) {
        if (!m_input.eof()) {
            throw std::runtime_error("input file read failed");
        }
        m_eof = true;
        return;
    }
    std::istringstream row(line);
    std::int64_t a;
    std::int64_t b;
    std::string extra;
    if (!(row >> a >> b) || (row >> extra) || a < std::numeric_limits<std::int32_t>::min() ||
        a > std::numeric_limits<std::int32_t>::max() || b < std::numeric_limits<std::int32_t>::min() ||
        b > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("invalid input line " + std::to_string(m_sent + 1));
    }
    if (!m_tasksOut.nb_write({m_sent, static_cast<std::int32_t>(a), static_cast<std::int32_t>(b)})) {
        throw std::logic_error("parser lost reserved FIFO space");
    }
    m_testEventLog.record(m_sent, "parser_send");
    ++m_sent;
}

Transform::Transform(sc_core::sc_module_name name, TestEventLog& testEventLog)
    : sc_module(name)
    , m_testEventLog(testEventLog) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Transform::tick() {
    // Downstream-to-upstream order: a newly accepted task cannot cross two stages.
    if (m_stage2 && m_tasksOut.nb_write(*m_stage2)) {
        m_testEventLog.record(m_stage2->m_id, "transform_emit", m_stage2->m_a, m_stage2->m_b);
        m_stage2.reset();
    }
    if (!m_stage2 && m_stage1) {
        m_stage2 =
            OrderedTask{m_stage1->m_id, std::max(m_stage1->m_a, m_stage1->m_b), std::min(m_stage1->m_a, m_stage1->m_b)};
        m_stage1.reset();
    }
    RawTask task;
    if (!m_stage1 && m_tasksIn.nb_read(task)) {
        m_stage1 = MagnitudeTask{task.m_id, magnitude(task.m_a), magnitude(task.m_b)};
        m_testEventLog.record(task.m_id, "transform_accept");
    }
}

Compute::Compute(sc_core::sc_module_name name, TestEventLog& testEventLog)
    : sc_module(name)
    , m_testEventLog(testEventLog) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Compute::deliver() {
    if (m_resultsOut.nb_write(m_result)) {
        m_testEventLog.record(m_result.m_id, "compute_emit", 0, 0, m_result.m_gcd);
        m_state = State::IDLE;
    } else {
        ++m_resultWaitCycles;
    }
}
void Compute::tick() {
    if (m_state == State::BUSY) {
        ++m_busyCycles;
        if (--m_remaining == 0) {
            m_testEventLog.record(m_result.m_id, "compute_complete", 0, 0, m_result.m_gcd);
            m_state = State::RESULT_PENDING;
            deliver();
        }
        return; // Completion must not fall through into accepting another task.
    }
    if (m_state == State::RESULT_PENDING) {
        deliver();
        return;
    }
    OrderedTask task;
    if (!m_tasksIn.nb_read(task)) {
        return;
    }
    const auto [value, latency] = gcdAndLatency(task.m_a, task.m_b);
    m_result = {task.m_id, value};
    m_remaining = latency;
    m_testEventLog.record(task.m_id, "compute_accept", task.m_a, task.m_b, value, latency);
    if (latency == 0) {
        m_testEventLog.record(task.m_id, "compute_complete", 0, 0, value);
        m_state = State::RESULT_PENDING;
        deliver();
    } else {
        m_state = State::BUSY;
    }
}

Output::Output(sc_core::sc_module_name name, std::ostream& output, TestEventLog& testEventLog,
               std::uint64_t testOutputPeriod)
    : sc_module(name)
    , m_output(output)
    , m_testEventLog(testEventLog)
    , m_testOutputPeriod(testOutputPeriod) {
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Output::tick() {
    if (currentCycle() % m_testOutputPeriod != 0) {
        return;
    }
    Result result;
    if (!m_resultsIn.nb_read(result)) {
        return;
    }
    if (result.m_id != m_received) {
        throw std::logic_error("out-of-order result");
    }
    m_output << result.m_gcd << '\n';
    if (!m_output) {
        throw std::runtime_error("output file write failed");
    }
    m_testEventLog.record(result.m_id, "output", 0, 0, result.m_gcd);
    ++m_received;
}
} // namespace stage1
