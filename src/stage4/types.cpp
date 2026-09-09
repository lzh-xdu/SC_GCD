/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file types.cpp
 * @brief Shared task printing and event observation.
 */
#include "types.hpp"

#include <ostream>

namespace stage4 {
std::ostream& operator<<(std::ostream& os, const RawTask& t) {
    return os << t.m_id << ':' << t.m_a << ',' << t.m_b;
}
std::ostream& operator<<(std::ostream& os, const OrderedTask& t) {
    return os << t.m_id << ':' << t.m_a << ',' << t.m_b;
}
std::ostream& operator<<(std::ostream& os, const Result& t) {
    return os << t.m_id << ':' << t.m_gcd;
}
} // namespace stage4
