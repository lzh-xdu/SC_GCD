#include "modules.hpp"
#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace stage1 {
std::ostream& operator<<(std::ostream& os, const RawTask& t) {
    return os << t.id << ':' << t.a << ',' << t.b;
}
std::ostream& operator<<(std::ostream& os, const OrderedTask& t) {
    return os << t.id << ':' << t.a << ',' << t.b;
}
std::ostream& operator<<(std::ostream& os, const Result& t) {
    return os << t.id << ':' << t.gcd;
}
std::uint64_t cycle() {
    return sc_core::sc_time_stamp().value() / sc_core::sc_time(1, sc_core::SC_NS).value();
}
void EventLog::record(std::uint64_t id, const char* event, std::uint64_t a,
                      std::uint64_t b, std::uint64_t value, std::uint64_t latency) const {
    if (stream) {
        *stream << id << ',' << event << ',' << cycle() << ',' << a << ',' << b
                << ',' << value << ',' << latency << '\n';
    }
}
static unsigned bits(std::uint64_t value) {
    unsigned count = 0;
    while (value != 0) { ++count; value >>= 1; }
    return count;
}
std::pair<std::uint64_t, std::uint64_t> gcd_and_latency(std::uint64_t a,
                                                       std::uint64_t b) {
    std::uint64_t latency = 0;
    while (b != 0) {
        latency += std::max(1, int(bits(a)) - int(bits(b)) + 1);
        const auto remainder = a % b;
        a = b;
        b = remainder;
    }
    return {a, latency};
}
static std::uint64_t magnitude(std::int32_t value) {
    const auto wide = std::int64_t(value); // Promote before negating INT32_MIN.
    return std::uint64_t(wide < 0 ? -wide : wide);
}

Parser::Parser(sc_core::sc_module_name name, std::istream& input, EventLog& log)
    : sc_module(name), input_(input), log_(log) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
}
void Parser::tick() {
    if (eof || tasks_out.num_free() == 0) return;
    std::string line;
    if (!std::getline(input_, line)) {
        if (!input_.eof()) throw std::runtime_error("input file read failed");
        eof = true;
        return;
    }
    std::istringstream row(line);
    std::int64_t a, b;
    std::string extra;
    if (!(row >> a >> b) || (row >> extra) ||
        a < std::numeric_limits<std::int32_t>::min() ||
        a > std::numeric_limits<std::int32_t>::max() ||
        b < std::numeric_limits<std::int32_t>::min() ||
        b > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("invalid input line " + std::to_string(sent + 1));
    }
    if (!tasks_out.nb_write({sent, std::int32_t(a), std::int32_t(b)})) {
        throw std::logic_error("parser lost reserved FIFO space");
    }
    log_.record(sent, "parser_send");
    ++sent;
}

Transform::Transform(sc_core::sc_module_name name, EventLog& log)
    : sc_module(name), log_(log) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
}
void Transform::tick() {
    // Downstream-to-upstream order: a newly accepted task cannot cross two stages.
    if (stage2_ && tasks_out.nb_write(*stage2_)) {
        log_.record(stage2_->id, "transform_emit", stage2_->a, stage2_->b);
        stage2_.reset();
    }
    if (!stage2_ && stage1_) {
        stage2_ = OrderedTask{stage1_->id, std::max(stage1_->a, stage1_->b),
                             std::min(stage1_->a, stage1_->b)};
        stage1_.reset();
    }
    RawTask task;
    if (!stage1_ && tasks_in.nb_read(task)) {
        stage1_ = MagnitudeTask{task.id, magnitude(task.a), magnitude(task.b)};
        log_.record(task.id, "transform_accept");
    }
}

Compute::Compute(sc_core::sc_module_name name, EventLog& log)
    : sc_module(name), log_(log) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
}
void Compute::deliver() {
    if (results_out.nb_write(result_)) {
        log_.record(result_.id, "compute_emit", 0, 0, result_.gcd);
        state_ = State::Idle;
    } else {
        ++result_wait_cycles;
    }
}
void Compute::tick() {
    if (state_ == State::Busy) {
        ++busy_cycles;
        if (--remaining_ == 0) {
            log_.record(result_.id, "compute_complete", 0, 0, result_.gcd);
            state_ = State::ResultPending;
            deliver();
        }
        return; // Completion must not fall through into accepting another task.
    }
    if (state_ == State::ResultPending) {
        deliver();
        return;
    }
    OrderedTask task;
    if (!tasks_in.nb_read(task)) return;
    const auto [value, latency] = gcd_and_latency(task.a, task.b);
    result_ = {task.id, value};
    remaining_ = latency;
    log_.record(task.id, "compute_accept", task.a, task.b, value, latency);
    if (latency == 0) {
        log_.record(task.id, "compute_complete", 0, 0, value);
        state_ = State::ResultPending;
        deliver();
    } else {
        state_ = State::Busy;
    }
}

Output::Output(sc_core::sc_module_name name, std::ostream& output, EventLog& log,
               std::uint64_t period)
    : sc_module(name), output_(output), log_(log), period_(period) {
    SC_METHOD(tick);
    sensitive << clk.pos();
    dont_initialize();
}
void Output::tick() {
    if (cycle() % period_ != 0) return;
    Result result;
    if (!results_in.nb_read(result)) return;
    if (result.id != received) throw std::logic_error("out-of-order result");
    output_ << result.gcd << '\n';
    if (!output_) throw std::runtime_error("output file write failed");
    log_.record(result.id, "output", 0, 0, result.gcd);
    ++received;
}
} // namespace stage1
