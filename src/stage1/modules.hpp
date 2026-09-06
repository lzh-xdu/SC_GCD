/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file modules.hpp
 * @brief Clocked stage 1 modules and their bounded-stream interfaces.
 */
#pragma once
#include <systemc>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <utility>

namespace stage1 {
inline constexpr double CLOCK_PERIOD_NS = 1.0;
struct RawTask {
    std::uint64_t m_id{};
    std::int32_t m_a{};
    std::int32_t m_b{};
};
struct MagnitudeTask {
    std::uint64_t m_id{};
    std::uint64_t m_a{};
    std::uint64_t m_b{};
};
struct OrderedTask {
    std::uint64_t m_id{};
    std::uint64_t m_a{};
    std::uint64_t m_b{};
};
struct Result {
    std::uint64_t m_id{};
    std::uint64_t m_gcd{};
};
std::ostream& operator<<(std::ostream&, const RawTask&);
std::ostream& operator<<(std::ostream&, const OrderedTask&);
std::ostream& operator<<(std::ostream&, const Result&);

// Diagnostic observer only: it cannot change module state or timing.
struct TestEventLog {
    std::ostream* m_testStream = nullptr;
    void record(std::uint64_t testId, const char* testEvent, std::uint64_t testA = 0, std::uint64_t testB = 0,
                std::uint64_t testValue = 0, std::uint64_t testLatency = 0) const;
};
std::uint64_t currentCycle();
std::pair<std::uint64_t, std::uint64_t> gcdAndLatency(std::uint64_t a, std::uint64_t b);

SC_MODULE(Parser) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_out<RawTask> m_tasksOut{"tasks_out"};
    std::uint64_t m_sent = 0;
    bool m_eof = false;
    Parser(sc_core::sc_module_name name, std::istream & input, TestEventLog & testEventLog);

private:
    std::istream& m_input;
    TestEventLog & m_testEventLog;
    void tick();
};

SC_MODULE(Transform) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<RawTask> m_tasksIn{"tasks_in"};
    sc_core::sc_fifo_out<OrderedTask> m_tasksOut{"tasks_out"};
    bool empty() const {
        return !m_stage1 && !m_stage2;
    }
    unsigned occupancy() const {
        return static_cast<unsigned>(m_stage1.has_value()) + static_cast<unsigned>(m_stage2.has_value());
    }
    Transform(sc_core::sc_module_name name, TestEventLog & testEventLog);

private:
    std::optional<MagnitudeTask> m_stage1;
    std::optional<OrderedTask> m_stage2;
    TestEventLog & m_testEventLog;
    void tick();
};

SC_MODULE(Compute) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<OrderedTask> m_tasksIn{"tasks_in"};
    sc_core::sc_fifo_out<Result> m_resultsOut{"results_out"};
    std::uint64_t m_busyCycles = 0;
    std::uint64_t m_resultWaitCycles = 0;
    bool idle() const {
        return m_state == State::IDLE;
    }
    Compute(sc_core::sc_module_name name, TestEventLog & testEventLog);

private:
    enum class State { IDLE, BUSY, RESULT_PENDING };
    State m_state = State::IDLE;
    Result m_result;
    std::uint64_t m_remaining = 0;
    TestEventLog & m_testEventLog;
    void tick();
    void deliver();
};

SC_MODULE(Output) {
    sc_core::sc_in<bool> m_clk{"clk"};
    sc_core::sc_fifo_in<Result> m_resultsIn{"results_in"};
    std::uint64_t m_received = 0;
    Output(sc_core::sc_module_name name, std::ostream & output, TestEventLog & testEventLog,
           std::uint64_t testOutputPeriod);

private:
    std::ostream& m_output;
    TestEventLog & m_testEventLog;
    std::uint64_t m_testOutputPeriod;
    void tick();
};
} // namespace stage1
