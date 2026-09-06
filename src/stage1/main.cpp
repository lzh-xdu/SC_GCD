#include "modules.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stage1;
struct Usage {
    std::uint64_t sum = 0;
    unsigned peak = 0;
    void sample(unsigned count) { sum += count; peak = std::max(peak, count); }
};

// Simulation harness: observes after channel updates, never moves task data.
SC_MODULE(System) {
    sc_core::sc_clock clk{"clk", sc_core::sc_time(1, sc_core::SC_NS), 0.5,
                         sc_core::sc_time(1, sc_core::SC_NS), true};
    sc_core::sc_fifo<RawTask> parser_to_transform;
    sc_core::sc_fifo<OrderedTask> transform_to_compute;
    sc_core::sc_fifo<Result> compute_to_output;
    Parser parser;
    Transform transform;
    Compute compute;
    Output output;
    Usage queues[3], pipeline;
    std::uint64_t cycles = 0;
    bool finished = false;
    System(sc_core::sc_module_name name, std::istream& in, std::ostream& out,
           EventLog& log, int depth, std::uint64_t period)
        : sc_module(name), parser_to_transform("parser_to_transform", depth),
          transform_to_compute("transform_to_compute", depth),
          compute_to_output("compute_to_output", depth), parser("parser", in, log),
          transform("transform", log), compute("compute", log),
          output("output", out, log, period) {
        parser.clk(clk); transform.clk(clk); compute.clk(clk); output.clk(clk);
        parser.tasks_out(parser_to_transform);
        transform.tasks_in(parser_to_transform);
        transform.tasks_out(transform_to_compute);
        compute.tasks_in(transform_to_compute);
        compute.results_out(compute_to_output);
        output.results_in(compute_to_output);
        SC_THREAD(observe);
    }
    void observe() {
        while (true) {
            wait(clk.posedge_event());
            wait(sc_core::SC_ZERO_TIME);
            cycles = cycle();
            queues[0].sample(parser_to_transform.num_available());
            queues[1].sample(transform_to_compute.num_available());
            queues[2].sample(compute_to_output.num_available());
            pipeline.sample(transform.occupancy());
            if (parser.eof && output.received == parser.sent && transform.empty() &&
                compute.idle() && parser_to_transform.num_available() == 0 &&
                transform_to_compute.num_available() == 0 &&
                compute_to_output.num_available() == 0) {
                finished = true;
                sc_core::sc_stop();
                return;
            }
        }
    }
};

static std::uint64_t positive(const char* text, std::uint64_t limit) {
    const std::string value(text);
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("expected positive integer option");
    const auto number = std::stoull(value);
    if (number == 0 || number > limit) throw std::runtime_error("option out of range");
    return number;
}
static int run(int argc, char** argv) {
    if (argc < 4 || argc > 8) {
        std::cerr << "Usage: stage1_gcd INPUT OUTPUT STATS [DEPTH=2] [EVENTS.csv|-]"
                     " [OUTPUT_PERIOD=1] [MAX_CYCLES=1000000]\n";
        return 2;
    }
    const int depth = argc > 4 ? int(positive(argv[4], 1000000)) : 2;
    const std::uint64_t period = argc > 6 ? positive(argv[6], 100000000) : 1;
    const std::uint64_t max_cycles = argc > 7 ? positive(argv[7], 1000000000) : 1000000;
    const bool trace_enabled = argc > 5 && std::string(argv[5]) != "-";
    // Prevent accidentally truncating an input or using one file for two outputs.
    std::vector<std::filesystem::path> paths;
    const int count = trace_enabled ? 4 : 3;
    for (int i = 0; i < count; ++i) {
        const auto path = std::filesystem::weakly_canonical(i == 3 ? argv[5] : argv[i + 1]);
        for (const auto& previous : paths) {
            if (path == previous || (std::filesystem::exists(path) &&
                std::filesystem::exists(previous) && std::filesystem::equivalent(path, previous)))
                throw std::runtime_error("input, output, stats and events must be distinct files");
        }
        paths.push_back(path);
    }
    std::ifstream input(argv[1]);
    if (!input) throw std::runtime_error("cannot open input");
    std::ofstream output(argv[2]), stats(argv[3]), events;
    if (!output || !stats) throw std::runtime_error("cannot open output or stats");
    EventLog log;
    if (trace_enabled) {
        events.open(argv[5]);
        if (!events) throw std::runtime_error("cannot open events");
        events << "id,event,cycle,a,b,value,latency\n";
        log.stream = &events;
    }
    System system("system", input, output, log, depth, period);
    // Include the last allowed rising edge, but no extra rising edge.
    sc_core::sc_start(sc_core::sc_time(double(max_cycles) + 0.5, sc_core::SC_NS));
    if (!system.finished) throw std::runtime_error("simulation cycle limit exceeded");
    const auto c = system.cycles;
    stats << std::setprecision(12) << "metric,value\n"
          << "cycles," << c << "\ntasks," << system.output.received
          << "\nsimulated_time_ns," << c
          << "\nthroughput_tasks_per_cycle," << double(system.output.received) / c
          << "\ncompute_busy_cycles," << system.compute.busy_cycles
          << "\ncompute_utilization," << double(system.compute.busy_cycles) / c
          << "\ncompute_result_wait_cycles," << system.compute.result_wait_cycles
          << "\noutput_period," << period << '\n';
    const char* names[] = {"parser_to_transform", "transform_to_compute", "compute_to_output"};
    for (int i = 0; i < 3; ++i) {
        stats << names[i] << "_capacity," << depth << '\n'
              << names[i] << "_average," << double(system.queues[i].sum) / c << '\n'
              << names[i] << "_peak," << system.queues[i].peak << '\n';
    }
    stats << "transform_registers_capacity,2\ntransform_registers_average,"
          << double(system.pipeline.sum) / c << "\ntransform_registers_peak,"
          << system.pipeline.peak << '\n';
    output.flush(); stats.flush();
    if (trace_enabled) events.flush();
    if (!output || !stats || (trace_enabled && !events)) throw std::runtime_error("file flush failed");
    std::cout << "PASS: " << system.output.received << " tasks, " << c << " cycles\n";
    return 0;
}
int sc_main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
