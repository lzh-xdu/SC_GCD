/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file dispatcher.cpp
 * @brief Stateful random ties are advanced only at accepted transfers.
 */
#include "dispatcher.hpp"

#include <stdexcept>

namespace stage3_window {
static std::uint32_t nextRandom(std::uint32_t state) {
    constexpr unsigned LEFT_SHIFT_FIRST = 13;
    constexpr unsigned RIGHT_SHIFT = 17;
    constexpr unsigned LEFT_SHIFT_LAST = 5;
    state ^= state << LEFT_SHIFT_FIRST;
    state ^= state >> RIGHT_SHIFT;
    state ^= state << LEFT_SHIFT_LAST;
    return state;
}
Dispatcher::Dispatcher(sc_core::sc_module_name name, stage1::TestEventLog& testEventLog, unsigned window,
                       std::uint32_t seed)
    : sc_module(name)
    , m_testEventLog(testEventLog)
    , m_window(window) {
    if (window == 0 || seed == 0) {
        throw std::invalid_argument("window and seed must be nonzero");
    }
    m_randomState.write(seed);
    SC_METHOD(route);
    sensitive << m_dataIn << m_validIn << m_baseIn << m_randomState;
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        sensitive << m_readyIn[unit];
    }
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
bool Dispatcher::hasCredit() const {
    const auto id = m_dataIn.read().m_id;
    return id >= m_baseIn.read() && id - m_baseIn.read() < m_window;
}
unsigned Dispatcher::selectedUnit() const {
    if (m_readyIn[0].read() && m_readyIn[1].read()) {
        return m_randomState.read() % UNIT_COUNT;
    }
    return m_readyIn[0].read() ? 0 : 1;
}
void Dispatcher::route() {
    const auto selected = selectedUnit();
    const bool ready = hasCredit() && m_readyIn[selected].read();
    m_readyOut.write(ready);
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        m_dataOut[unit].write(m_dataIn.read());
        m_validOut[unit].write(m_validIn.read() && ready && unit == selected);
    }
}
void Dispatcher::tick() {
    if (!m_validIn.read()) {
        return;
    }
    const auto id = m_dataIn.read().m_id;
    if (!hasCredit()) {
        ++m_windowBlockedCycles;
        m_testEventLog.record(id, "window_blocked");
    } else if (!m_readyIn[selectedUnit()].read()) {
        ++m_engineBlockedCycles;
        m_testEventLog.record(id, "engines_blocked");
    } else {
        ++m_dispatched;
        if (m_readyIn[0].read() && m_readyIn[1].read()) {
            ++m_randomChoices;
            m_testEventLog.record(id, "random_choice", selectedUnit(), m_randomState.read());
            m_randomState.write(nextRandom(m_randomState.read()));
        }
    }
}
} // namespace stage3_window
