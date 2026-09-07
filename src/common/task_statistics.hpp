/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file task_statistics.hpp
 * @brief Passive task latency accounting, outside simulated hardware resources.
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
    void record(std::uint64_t id, std::string_view event, std::uint64_t cycle);
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
