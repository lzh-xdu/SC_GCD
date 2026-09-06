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
 * @brief 解析模块：把文件中的整数对转换为逐拍任务流。
 *
 *   input file (m_input)
 *          |
 *          v
 *   +----------------------+  m_tasksOut: RawTask
 *   | Parser               |----------------------> FIFO --> Transform
 *   | m_sent / m_eof       |                        depth = D (default 2)
 *   | no task buffer       |
 *   +----------^-----------+
 *              | m_clk.pos()
 *
 * 触发：SC_METHOD(tick)，仅时钟上升沿；dont_initialize 禁止初始化时额外执行。
 * 时钟：与其他模块共用 T=1 ns，首沿 1 ns；每沿最多一次 getline 和一次 nb_write。
 * 缓存：模块内无跨周期任务缓存；文件流是仿真环境资源，输出 FIFO 由顶层创建。
 * 周期：第 k 沿读取并写入一条任务，下游最早 k+1 沿读取；不增加额外解析延迟。
 * 背压：输出 FIFO 满时不读文件；EOF 后不再发送。m_sent 是发送计数，不是缓存。
 * 旁路观察：m_testEventLog 只记录事件，不参与任务传递或改变时序。
 */
SC_MODULE(Parser) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_out<RawTask> m_tasksOut{"tasks_out"};
    std::uint64_t m_sent = 0;
    bool m_eof = false;
    Parser(sc_core::sc_module_name name, std::istream & input, TestEventLog & testEventLog);

private:
    std::istream& m_input;
    TestEventLog & m_testEventLog;
    void tick();
};
} // namespace stage1
