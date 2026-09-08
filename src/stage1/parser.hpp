/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file parser.hpp
 * @brief Parser module interface and cycle contract.
 */
#pragma once
#include "types.hpp"

#include <systemc>

namespace stage1 {
/**
 * @brief Parses signed 32-bit integer pairs into a clocked task stream.
 *
 * Interface:
 *
 * m_input -----> +------------------+
 *                |      Parser      |-----> m_tasksOut (RawTask FIFO)
 * m_clk -------->|                  |
 *                +------------------+
 *
 * Protocol:
 * - Read at most one line and send at most one task per rising edge.
 * - Do not read the file while the output FIFO is full; stop sending after normal EOF.
 * - No internal task buffer; m_sent counts sent tasks and m_eof records EOF.
 * - The input stream and EventRecorder reference must outlive the module.
 * - Invalid rows or input failures throw runtime_error; the observer does not control hardware.
 *
 * Timing:
 * - SC_METHOD(tick) runs only on m_clk.pos(); dont_initialize prevents an initial call.
 * - The shared clock has T=1 ns and its first edge at 1 ns.
 * - A task written at edge k is visible to the downstream FIFO reader at k+1 or later.
 * - The external FIFO is owned by the top level (default depth 2); no extra parser delay.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction initializes m_sent=0 and m_eof=false.
 */
SC_MODULE(Parser) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_out<RawTask> m_tasksOut{"tasks_out"};
    std::uint64_t m_sent = 0;
    bool m_eof = false;
    /**
     * @brief input 须可读且覆盖模块寿命；读取失败/非法整数行抛 runtime_error，正常 EOF 停止发送。
     */
    Parser(sc_core::sc_module_name name, std::istream & input, EventRecorder & recorder);

private:
    std::istream& m_input;
    EventRecorder & m_recorder;
    void tick();
};
} // namespace stage1
