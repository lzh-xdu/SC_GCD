/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file parser.hpp
 * @brief Parser module interface and input pacing contract.
 */
#pragma once
#include "types.hpp"

#include <systemc>

namespace stage4 {
/**
 * @brief Parses signed 32-bit integer pairs at timed input boundaries without a clock port.
 *
 * Interface:
 *
 * m_input -----> +------------------+
 *                |      Parser      |-----> m_tasksOut (RawTask FIFO)
 *                |  timer / space   |
 *                +------------------+
 *
 * Protocol:
 * - Read at most one line and send at most one task per 1 ns input boundary.
 * - Do not read the file while the output FIFO is full; stop sending after normal EOF.
 * - No internal task buffer; m_sent counts sent tasks and m_eof records EOF.
 * - The input stream and EventRecorder reference must outlive the module.
 * - Invalid rows or input failures throw runtime_error; the observer does not control hardware.
 *
 * Timing:
 * - Production System schedules readAndSend(); canRead() advertises the next input opportunity.
 * - Standalone mode uses SC_THREAD(run), first input at 1 ns; both modes keep integer-ns boundaries.
 * - A full FIFO removes the production input deadline; the standalone thread waits on data_read_event.
 * - Space released at time t permits input only at the next strictly later input boundary.
 * - Standalone mode waits one delta at each boundary; production System commits all channels together.
 * - The FIFO updates after the send phase; the downstream reader consumes at k+1 or later.
 * - EOF terminates the standalone thread or removes the production input deadline.
 * - The external FIFO is owned by the top level (default depth 2); no extra parser delay.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction initializes m_sent=0 and m_eof=false.
 */
SC_MODULE(Parser) {
    sc_core::sc_fifo_out<RawTask> m_tasksOut{"tasks_out"};
    std::uint64_t m_sent = 0;
    bool m_eof = false;
    /**
     * @brief input 须可读且覆盖模块寿命；读取失败/非法整数行抛 runtime_error，正常 EOF 停止发送。
     * @param scheduledBySystem true 时不注册独立线程，由 System 在输入边界调用 readAndSend。
     */
    Parser(sc_core::sc_module_name name, std::istream & input, EventRecorder & recorder,
           bool scheduledBySystem = false);

    bool canRead() const;
    void readAndSend();

private:
    std::istream& m_input;
    EventRecorder & m_recorder;
    void run();
    void waitForInputBoundary();
};
} // namespace stage4
