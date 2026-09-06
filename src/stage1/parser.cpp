/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file parser.cpp
 * @brief Parser clock-edge implementation.
 */
#include "parser.hpp"

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
} // namespace stage1
