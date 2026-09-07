/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file contract.hpp
 * @brief 始终生效的运行时契约：入口参数、内部不变量和操作结果检查。
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
inline void requireCondition(bool condition, const Message& message) {
    if (!condition) {
        throw Exception(message);
    }
}
