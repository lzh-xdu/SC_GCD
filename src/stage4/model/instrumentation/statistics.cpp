/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file statistics.cpp
 * @brief Accumulates observed occupancy without modifying model resources.
 */
#include "statistics.hpp"

#include <algorithm>

namespace stage4::model::instrumentation {
void Usage::sample(unsigned count) {
    m_sum += count;
    m_peak = std::max(m_peak, count);
}
} // namespace stage4::model::instrumentation
