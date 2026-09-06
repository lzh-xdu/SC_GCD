#pragma once
#include <systemc>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <utility>

namespace stage1 {
struct RawTask { std::uint64_t id{}; std::int32_t a{}, b{}; };
struct MagnitudeTask { std::uint64_t id{}, a{}, b{}; };
struct OrderedTask { std::uint64_t id{}, a{}, b{}; };
struct Result { std::uint64_t id{}, gcd{}; };
std::ostream& operator<<(std::ostream&, const RawTask&);
std::ostream& operator<<(std::ostream&, const OrderedTask&);
std::ostream& operator<<(std::ostream&, const Result&);

// Diagnostic observer only: it cannot change module state or timing.
struct EventLog {
    std::ostream* stream = nullptr;
    void record(std::uint64_t id, const char* event, std::uint64_t a = 0,
                std::uint64_t b = 0, std::uint64_t value = 0,
                std::uint64_t latency = 0) const;
};
std::uint64_t cycle();
std::pair<std::uint64_t, std::uint64_t> gcd_and_latency(std::uint64_t a,
                                                       std::uint64_t b);

SC_MODULE(Parser) {
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_fifo_out<RawTask> tasks_out{"tasks_out"};
    std::uint64_t sent = 0;
    bool eof = false;
    Parser(sc_core::sc_module_name name, std::istream& input, EventLog& log);
private:
    std::istream& input_;
    EventLog& log_;
    void tick();
};

SC_MODULE(Transform) {
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_fifo_in<RawTask> tasks_in{"tasks_in"};
    sc_core::sc_fifo_out<OrderedTask> tasks_out{"tasks_out"};
    bool empty() const { return !stage1_ && !stage2_; }
    unsigned occupancy() const { return unsigned(bool(stage1_)) + unsigned(bool(stage2_)); }
    Transform(sc_core::sc_module_name name, EventLog& log);
private:
    std::optional<MagnitudeTask> stage1_;
    std::optional<OrderedTask> stage2_;
    EventLog& log_;
    void tick();
};

SC_MODULE(Compute) {
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_fifo_in<OrderedTask> tasks_in{"tasks_in"};
    sc_core::sc_fifo_out<Result> results_out{"results_out"};
    std::uint64_t busy_cycles = 0, result_wait_cycles = 0;
    bool idle() const { return state_ == State::Idle; }
    Compute(sc_core::sc_module_name name, EventLog& log);
private:
    enum class State { Idle, Busy, ResultPending };
    State state_ = State::Idle;
    Result result_;
    std::uint64_t remaining_ = 0;
    EventLog& log_;
    void tick();
    void deliver();
};

SC_MODULE(Output) {
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_fifo_in<Result> results_in{"results_in"};
    std::uint64_t received = 0;
    Output(sc_core::sc_module_name name, std::ostream& output, EventLog& log,
           std::uint64_t period);
private:
    std::ostream& output_;
    EventLog& log_;
    std::uint64_t period_;
    void tick();
};
} // namespace stage1
