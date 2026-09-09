/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file clock.hpp
 * @brief Defines the shared simulated cycle unit.
 *
 * Interface:
 *
 * SystemC timestamp --> [ currentCycle ] --> integer cycle
 *
 * Protocol:
 * - CLOCK_PERIOD_NS is the shared 1 ns model period.
 *
 * Timing:
 * - Return floor(timestamp / 1 ns); zero time returns zero, and the query does not advance the kernel.
 *
 * Reset:
 * - Not applicable: the SystemC kernel owns simulation time.
 */
#pragma once

#include <cstdint>

namespace stage4::model::timing {
inline constexpr double CLOCK_PERIOD_NS = 1.0;
std::uint64_t currentCycle();
} // namespace stage4::model::timing
