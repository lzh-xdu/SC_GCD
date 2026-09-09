/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file simulation_time.cpp
 * @brief Converts SystemC timestamps to the shared cycle unit.
 */
#include "simulation_time.hpp"

#include <systemc>

namespace stage4::model::timing {
std::uint64_t currentCycle() {
    return sc_core::sc_time_stamp().value() / sc_core::sc_time(CYCLE_DURATION_NS, sc_core::SC_NS).value();
}
} // namespace stage4::model::timing
