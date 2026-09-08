/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file task_statistics.hpp
 * @brief Records passive task latency statistics outside simulated hardware resources.
 *
 * Interface:
 *
 * id / event / cycle --> +------------------+ --> write(stream): CSV
 *                        |  TaskStatistics  |
 *                        +------------------+
 *
 * Protocol:
 * - record accepts parser_send, compute_accept, compute_complete, compute_emit and output boundaries in order.
 * - Ignore non-boundary events; out-of-order boundaries or decreasing cycles throw logic_error.
 * - write requires all recorded tasks to have completed; unfinished tasks throw logic_error.
 * - Empty task sets are valid; an unwritable stream or failed write throws runtime_error.
 * - The maps and samples are observer storage, not hardware buffers or scheduling inputs.
 *
 * Timing:
 * - Calls record supplied cycle numbers and never advance simulated time.
 * - Equal-cycle boundaries are valid; write reports four phase latencies and total latency in cycles.
 *
 * Reset:
 * - No reset method; a newly constructed object has no pending tasks or samples.
 */
#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string_view>
#include <vector>

class TaskStatistics {
public:
    /**
     * @brief 记录任务边界；非边界事件忽略，边界乱序或周期倒退抛 logic_error。
     */
    void record(std::uint64_t id, std::string_view event, std::uint64_t cycle);
    /**
     * @brief 排空后写 CSV；未完成任务抛 logic_error，不可写流抛 runtime_error；空任务集合法。
     */
    void write(std::ostream& stream) const;

private:
    static constexpr std::size_t BOUNDARY_COUNT = 5;
    struct Timeline {
        std::array<std::uint64_t, BOUNDARY_COUNT> m_edges{};
        std::size_t m_next = 0;
    };
    std::map<std::uint64_t, Timeline> m_pending;
    std::array<std::vector<std::uint64_t>, BOUNDARY_COUNT> m_samples;
};
