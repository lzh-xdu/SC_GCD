/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file parser.cpp
 * @brief Parser clock-edge implementation.
 */
#include "parser.hpp"
#include "../common/contract.hpp"

#include <istream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace stage1 {
Parser::Parser(sc_core::sc_module_name name, std::istream& input, TestEventLog& testEventLog)
    : sc_module(name)
    , m_input(input)
    , m_testEventLog(testEventLog) {
    requireCondition(static_cast<bool>(input), "input stream is not readable");
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Parser::tick() {
    requireCondition(!m_input.bad() && (m_eof || !m_input.fail()), "input file read failed");
    if (m_eof || m_tasksOut.num_free() == 0) {
        return;
    }
    std::string line;
    if (!std::getline(m_input, line)) {
        requireCondition(m_input.eof(), "input file read failed");
        m_eof = true;
        return;
    }
    std::istringstream row(line);
    std::int64_t a;
    std::int64_t b;
    std::string extra;
    requireCondition((row >> a >> b) && !(row >> extra) && a >= std::numeric_limits<std::int32_t>::min() &&
                         a <= std::numeric_limits<std::int32_t>::max() &&
                         b >= std::numeric_limits<std::int32_t>::min() && b <= std::numeric_limits<std::int32_t>::max(),
                     "invalid input line " + std::to_string(m_sent + 1));
    requireCondition<std::logic_error>(
        m_tasksOut.nb_write({m_sent, static_cast<std::int32_t>(a), static_cast<std::int32_t>(b)}),
        "parser lost reserved FIFO space");
    m_testEventLog.record(m_sent, "parser_send");
    ++m_sent;
}
} // namespace stage1
