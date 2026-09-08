/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file gcd_plan.hpp
 * @brief Plans the GCD result and modeled computation cycles in one traversal.
 *
 * Interface:
 *
 * ordered a,b --> [ planGcd ] --> GCD value + cycle count
 *
 * Protocol:
 * - Require ordered nonnegative magnitudes a >= b.
 * - Use the functional remainder operation; no instrumentation participates in the plan.
 * - Model Assumption (outside spec): b==0 skips modulo and its formula; zero steps give zero cycles.
 * - For magnitudes, gcd(a,0)=a, gcd(0,b)=b and gcd(0,0)=0 (inputs are ordered before this call).
 *
 * Timing:
 * - Sum max(1,bits(a)-bits(b)+1) per modulo; the caller's state machine enforces completion at k+L.
 *
 * Reset:
 * - Not applicable: planning is stateless and does not advance simulation time.
 */
#pragma once

#include <cstdint>
#include <utility>

namespace model::timing {
std::pair<std::uint64_t, std::uint64_t> planGcd(std::uint64_t a, std::uint64_t b);
} // namespace model::timing
