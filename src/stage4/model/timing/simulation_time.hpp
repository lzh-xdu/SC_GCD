/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file simulation_time.hpp
 * @brief Defines the shared simulated cycle unit.
 *
 * Interface:
 *
 * SystemC timestamp --> [ currentCycle ] --> integer cycle
 *
 * Protocol:
 * - CYCLE_DURATION_NS is the shared 1 ns model period.
 *
 * Timing:
 * - Return floor(timestamp / 1 ns); zero time returns zero, and the query does not advance the kernel.
 *
 * Reset:
 * - Not applicable: the SystemC kernel owns simulation time.
 */
#pragma once

#include <cstdint>
#include <limits>

namespace stage4::model::timing {
inline constexpr double CYCLE_DURATION_NS = 1.0;
inline constexpr std::uint64_t NO_DEADLINE = std::numeric_limits<std::uint64_t>::max();
std::uint64_t currentCycle();
} // namespace stage4::model::timing
