/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file task_statistics.cpp
 * @brief Admission-to-output latency and its four additive phases.
 */
#include "task_statistics.hpp"
#include "contract.hpp"
#include <algorithm>
#include <numeric>
#include <ostream>
#include <stdexcept>

void TaskStatistics::record(std::uint64_t id, std::string_view event, std::uint64_t cycle) {
    constexpr std::array<std::string_view, BOUNDARY_COUNT> EVENTS{"parser_send", "compute_accept", "compute_complete",
                                                                  "compute_emit", "output"};
    const auto found = std::find(EVENTS.begin(), EVENTS.end(), event);
    if (found == EVENTS.end()) {
        return;
    }
    const auto index = static_cast<std::size_t>(found - EVENTS.begin());
    auto& timeline = m_pending[id];
    assertCondition<std::logic_error>(
        !(index != timeline.m_next || (index > 0 && cycle < timeline.m_edges[index - 1])),
        "invalid task statistics event sequence");
    timeline.m_edges[index] = cycle;
    ++timeline.m_next;
    if (timeline.m_next != BOUNDARY_COUNT) {
        return;
    }
    for (std::size_t phase = 0; phase + 1 < BOUNDARY_COUNT; ++phase) {
        m_samples[phase].push_back(timeline.m_edges[phase + 1] - timeline.m_edges[phase]);
    }
    m_samples.back().push_back(cycle - timeline.m_edges.front());
    m_pending.erase(id);
}

void TaskStatistics::write(std::ostream& stream) const {
    assertCondition(static_cast<bool>(stream), "statistics stream is not writable");
    assertCondition<std::logic_error>(m_pending.empty(), "unfinished task statistics at drain");
    constexpr std::array<std::string_view, BOUNDARY_COUNT> NAMES{"precompute", "compute", "result_wait", "delivery",
                                                                 "end_to_end"};
    stream << "task_latency_count," << m_samples.back().size() << '\n';
    for (std::size_t phase = 0; phase < BOUNDARY_COUNT; ++phase) {
        auto sorted = m_samples[phase];
        std::sort(sorted.begin(), sorted.end());
        const auto total = std::accumulate(sorted.begin(), sorted.end(), std::uint64_t{0});
        const auto count = sorted.size();
        // Nearest-rank P95: ceil(19*N/20), expressed without floating-point rounding.
        const auto rank = count - count / 20;
        const auto prefix = "task_latency_";
        stream << prefix << NAMES[phase] << "_sum_cycles," << total << '\n'
               << prefix << NAMES[phase] << "_mean_cycles," << (count ? static_cast<double>(total) / count : 0) << '\n'
               << prefix << NAMES[phase] << "_max_cycles," << (count ? sorted.back() : 0) << '\n'
               << prefix << NAMES[phase] << "_p95_cycles," << (count ? sorted[rank - 1] : 0) << '\n';
    }
}
