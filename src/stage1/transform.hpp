/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file transform.hpp
 * @brief Transform module interface and cycle contract.
 */
#pragma once
#include "types.hpp"

#include <systemc>
#include <optional>

// stage1 names the assignment version, not a pipeline register or a hardware module.
namespace stage1 {
/**
 * @brief 变换模块：两级弹性流水完成绝对值和比较交换。
 *
 *                  +---------------------------------------------+
 * FIFO --RawTask-->| stage1: abs(a), abs(b) --> stage2: max, min | --> FIFO
 *  D     m_tasksIn |       one task                  one task    | m_tasksOut
 *                  +--------------------^------------------------+ OrderedTask
 *                                       | m_clk.pos()                D
 *
 * 触发：SC_METHOD(tick)，仅共同时钟上升沿；不在初始化阶段执行。
 * 时钟：T=1 ns，首沿 1 ns；无阻塞时每周期最多接收/写出一条任务。
 * 缓存：m_magnitudeStage 与 m_orderedStage 各保存一条，optional 表示是否有效；两侧 FIFO 在模块外。
 * 周期：k 沿接收并取绝对值 -> k+1 沿比较交换 -> k+2 沿写出，基础延迟严格为 2T。
 *        输出 FIFO 的下游最早 k+3 沿读取。D 是外部 FIFO 容量，默认 2。
 * 背压：排序级写不出则保持；绝对值级空时还能接收一条，两级满后停止接收。
 *
 * 更新：先输出旧排序级，再移动旧绝对值级，最后接收新任务，禁止新数据同沿穿两级。
 * 旁路观察：m_testEventLog
 * 不作为硬件资源或调度输入。
 */
SC_MODULE(Transform) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<RawTask> m_tasksIn{"tasks_in"};
    sc_core::sc_fifo_out<OrderedTask> m_tasksOut{"tasks_out"};
    bool empty() const {
        return !m_magnitudeStage && !m_orderedStage;
    }
    unsigned occupancy() const {
        return static_cast<unsigned>(m_magnitudeStage.has_value()) + static_cast<unsigned>(m_orderedStage.has_value());
    }
    Transform(sc_core::sc_module_name name, TestEventLog & testEventLog);

private:
    std::optional<MagnitudeTask> m_magnitudeStage;
    std::optional<OrderedTask> m_orderedStage;
    TestEventLog & m_testEventLog;
    void tick();
};
} // namespace stage1
