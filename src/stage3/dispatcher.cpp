/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file dispatcher.cpp
 * @brief Route one transfer to the selected engine without skipping busy engines.
 */
#include "dispatcher.hpp"

namespace stage3 {
Dispatcher::Dispatcher(sc_core::sc_module_name name, stage1::TestEventLog& testEventLog)
    : sc_module(name)
    , m_testEventLog(testEventLog) {
    SC_METHOD(route);
    sensitive << m_turn << m_dataIn << m_validIn;
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        sensitive << m_readyIn[unit];
    }
    SC_METHOD(tick);
    sensitive << m_clk.pos();
    dont_initialize();
}
void Dispatcher::route() {
    const auto selected = m_turn.read();
    m_readyOut.write(m_readyIn[selected].read());
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        m_dataOut[unit].write(m_dataIn.read());
        m_validOut[unit].write(m_validIn.read() && unit == selected);
    }
}
void Dispatcher::tick() {
    if (!m_validIn.read()) {
        return;
    }
    const auto selected = m_turn.read();
    const auto next = (selected + 1) % UNIT_COUNT;
    if (m_readyIn[selected].read()) {
        m_turn.write(next);
    } else if (m_readyIn[next].read()) {
        ++m_idleOtherBlockedCycles;
        m_testEventLog.record(m_dataIn.read().m_id, "dispatch_idle_other", selected);
    }
}
} // namespace stage3
