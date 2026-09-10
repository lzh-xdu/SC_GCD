/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file statistics.cpp
 * @brief Accumulates observed occupancy without modifying model resources.
 */
#include "statistics.hpp"

#include <algorithm>

namespace stage4::model::instrumentation {
void Usage::sample(std::uint64_t count) {
    m_sum += count;
    m_peak = std::max(m_peak, count);
    m_last = count;
}
void Usage::hold(std::uint64_t cycles) {
    m_sum += cycles * m_last;
}
} // namespace stage4::model::instrumentation
