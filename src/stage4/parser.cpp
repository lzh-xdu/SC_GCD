/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file parser.cpp
 * @brief Input-paced Parser with event-based backpressure waiting.
 */
#include "parser.hpp"
#include "common/contract.hpp"

#include <istream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace stage4 {
Parser::Parser(sc_core::sc_module_name name, std::istream& input, EventRecorder& recorder, bool scheduledBySystem)
    : sc_module(name)
    , m_input(input)
    , m_recorder(recorder) {
    requireCondition(static_cast<bool>(input), "input stream is not readable");
    if (!scheduledBySystem) {
        SC_THREAD(run);
    }
}
bool Parser::canRead() const {
    return !m_eof && m_tasksOut.num_free() > 0;
}
void Parser::waitForInputBoundary() {
    const auto period = sc_core::sc_time(CYCLE_DURATION_NS, sc_core::SC_NS).value();
    const auto offset = sc_core::sc_time_stamp().value() % period;
    wait(sc_core::sc_time::from_value(period - offset));
    // Match the old edge evaluation phase: a FIFO write becomes visible after this phase,
    // so a downstream reader at this boundary cannot consume the new task immediately.
    wait(sc_core::SC_ZERO_TIME);
}
void Parser::run() {
    waitForInputBoundary();
    while (!m_eof) {
        if (m_tasksOut.num_free() == 0) {
            // No timed polling while blocked; a read releases space in the FIFO update phase.
            wait(m_tasksOut.data_read_event());
            waitForInputBoundary();
            continue;
        }
        readAndSend();
        if (!m_eof) {
            waitForInputBoundary();
        }
    }
}
void Parser::readAndSend() {
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
    m_recorder.record(m_sent, "parser_send");
    ++m_sent;
}
} // namespace stage4
