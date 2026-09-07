/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file output.hpp
 * @brief Output module interface and cycle contract.
 */
#pragma once
#include "types.hpp"

#include <systemc>

namespace stage1 {
/**
 * @brief 输出模块：按序接收 GCD，向功能文件每行写一个结果。
 *
 *                         +-----------------------+
 * Compute --> FIFO -----> | Output                | -----> result file
 *              D          | next id = m_received   |         (m_output)
 *             m_resultsIn | no task/result buffer  |
 *               Result    +-----------^-----------+
 *                                     | m_clk.pos()
 *
 * 触发：SC_METHOD(tick)，仅共同时钟上升沿；不在初始化阶段执行。
 * 时钟：T=1 ns，首沿 1 ns；默认每沿最多读取并写文件一条，不增加输出处理延迟。
 * 缓存：无内部跨周期任务缓存；输入 FIFO 在顶层创建，默认 D=2。
 * 周期：Compute 在 k 沿写入 FIFO，本模块最早 k+1 沿读取并写出功能结果。
 * 背压测试：m_testOutputPeriod=N 时，仅 cycle%N==0 的沿尝试接收，默认 N=1。
 *           跳过的沿不弹出 FIFO，等待会沿数据路径向上游传播。
 * 顺序：读取后检查 id==m_received；功能文件只写 gcd，不写 id、调试或统计。
 * 旁路观察：m_testEventLog 记录 output 事件，不影响功能文件内容。
 */
SC_MODULE(Output) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<Result> m_resultsIn{"results_in"};
    std::uint64_t m_received = 0;
    /// @brief output 须可写且覆盖模块寿命；testOutputPeriod 单位为周期，必须大于零。
    /// @throws std::invalid_argument 周期为零；写失败抛 runtime_error，结果乱序抛 logic_error。
    Output(sc_core::sc_module_name name, std::ostream & output, TestEventLog & testEventLog,
           std::uint64_t testOutputPeriod);

private:
    std::ostream& m_output;
    TestEventLog & m_testEventLog;
    std::uint64_t m_testOutputPeriod;
    void tick();
};
} // namespace stage1
