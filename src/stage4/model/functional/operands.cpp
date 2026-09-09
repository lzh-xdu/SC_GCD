/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file operands.cpp
 * @brief Pure numerical operations, independent of SystemC.
 */
#include "operands.hpp"
#include "../../common/contract.hpp"

#include <algorithm>

namespace stage4::model::functional {
std::uint64_t magnitude(std::int32_t value) {
    const auto wide = static_cast<std::int64_t>(value); // Promote before negating INT32_MIN.
    return static_cast<std::uint64_t>(wide < 0 ? -wide : wide);
}
std::pair<std::uint64_t, std::uint64_t> order(std::uint64_t a, std::uint64_t b) {
    return {std::max(a, b), std::min(a, b)};
}
std::uint64_t remainder(std::uint64_t a, std::uint64_t b) {
    requireCondition<std::logic_error>(b != 0, "remainder requires nonzero divisor");
    return a % b;
}
} // namespace stage4::model::functional
