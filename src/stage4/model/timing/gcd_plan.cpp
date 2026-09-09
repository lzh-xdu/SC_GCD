/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file gcd_plan.cpp
 * @brief Shared modulo latency policy for all clocked Compute variants.
 */
#include "gcd_plan.hpp"
#include "../functional/operands.hpp"
#include "../../common/contract.hpp"

#include <algorithm>

namespace stage4::model::timing {
static unsigned bits(std::uint64_t value) {
    unsigned count = 0;
    while (value != 0) {
        ++count;
        value >>= 1;
    }
    return count;
}
std::pair<std::uint64_t, std::uint64_t> planGcd(std::uint64_t a, std::uint64_t b) {
    requireCondition<std::logic_error>(a >= b, "compute requires ordered magnitudes");
    std::uint64_t latency = 0;
    while (b != 0) {
        constexpr int MIN_REMAINDER_CYCLES = 1;
        constexpr int INCLUSIVE_BIT_POSITION = 1;
        latency += std::max(MIN_REMAINDER_CYCLES,
                            static_cast<int>(bits(a)) - static_cast<int>(bits(b)) + INCLUSIVE_BIT_POSITION);
        const auto remainder = functional::remainder(a, b);
        a = b;
        b = remainder;
    }
    return {a, latency};
}
} // namespace stage4::model::timing
