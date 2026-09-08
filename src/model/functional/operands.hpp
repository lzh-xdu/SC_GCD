/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file operands.hpp
 * @brief Provides pure operand transformations and one Euclidean step.
 *
 * Interface:
 *
 * int32 inputs --> [ magnitude / order ] --> nonnegative operands
 * nonzero rhs  --> [ remainder step    ] --> next operands
 *
 * Protocol:
 * - magnitude widens before negation; INT32_MIN is supported.
 * - order returns the greater operand first; remainder requires b != 0.
 *
 * Timing:
 * - No clock, state machine, logging or statistics; functions do not advance simulation time.
 *
 * Reset:
 * - Not applicable: all functions are stateless.
 */
#pragma once

#include <cstdint>
#include <utility>

namespace model::functional {
std::uint64_t magnitude(std::int32_t value);
std::pair<std::uint64_t, std::uint64_t> order(std::uint64_t a, std::uint64_t b);
std::uint64_t remainder(std::uint64_t a, std::uint64_t b);
} // namespace model::functional
