/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/**
 * @file contract.hpp
 * @brief Checks runtime contracts consistently in Debug and Release.
 *
 * Interface:
 *
 * condition / message --> +------------------+ --> normal return (true)
 *                         | assertCondition | --> Exception(message) (false)
 *                         +------------------+
 *
 * Protocol:
 * - Evaluate the condition argument once; throw the selected Exception if false (default runtime_error).
 * - message must construct Exception; a pointer message must be non-null.
 * - Internal invariants may select logic_error; this function does not replace the standard assert macro.
 * - Checks with I/O side effects remain active in Release; the function throws rather than terminating the process.
 *
 * Timing:
 * - An ordinary synchronous C++ call; no clock, handshake or simulated latency.
 * - The check does not advance simulation time.
 *
 * Reset:
 * - Not applicable: this stateless function has no reset or retained state.
 */
#pragma once

#include <stdexcept>

/**
 * @brief condition 为 false 时抛出 Exception；Debug/Release 行为相同。
 * @param condition 必须成立的条件；调用表达式只求值一次。
 * @param message 非空指针指向的错误说明，或 std::string；须可构造 Exception。
 * @throws Exception 默认 runtime_error；内部不变量可显式选择 logic_error。
 * @note 不替代标准 assert 宏，不终止进程；有副作用的 I/O 检查也始终执行。
 */
template <typename Exception = std::runtime_error, typename Message>
inline void assertCondition(bool condition, const Message& message) {
    if (!condition) {
        throw Exception(message);
    }
}
