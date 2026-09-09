/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file clock.cpp
 * @brief Converts SystemC timestamps to the shared cycle unit.
 */
#include "clock.hpp"

#include <systemc>

namespace stage4::model::timing {
std::uint64_t currentCycle() {
    return sc_core::sc_time_stamp().value() / sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS).value();
}
} // namespace stage4::model::timing
