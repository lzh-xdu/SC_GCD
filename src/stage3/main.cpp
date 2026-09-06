/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file main.cpp
 * @brief Connect and observe the stage 3 dual-engine simulation.
 */
#include "../stage2/compute.hpp"
#include "dispatcher.hpp"
#include "collector.hpp"
#include "../stage2/transform.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stage2;
using stage3::Collector;
using stage3::Dispatcher;
using stage3::UNIT_COUNT;
namespace {
constexpr int MIN_ARGUMENTS = 4;
constexpr int MAX_ARGUMENTS = 9;
constexpr int RESULT_DEPTH_ARGUMENT = 8;
constexpr int DEFAULT_RESULT_DEPTH = 2;
constexpr int INPUT_ARGUMENT = 1;
constexpr int OUTPUT_ARGUMENT = 2;
constexpr int STATS_ARGUMENT = 3;
constexpr int DEPTH_ARGUMENT = 4;
constexpr int TEST_EVENTS_ARGUMENT = 5;
constexpr int TEST_OUTPUT_PERIOD_ARGUMENT = 6;
constexpr int MAX_CYCLES_ARGUMENT = 7;
constexpr int DEFAULT_DEPTH = 2;
constexpr int MAX_DEPTH = 1000000;
constexpr std::uint64_t DEFAULT_MAX_CYCLES = 1000000;
constexpr std::uint64_t MAX_CYCLE_LIMIT = 1000000000;
constexpr std::uint64_t TEST_DEFAULT_OUTPUT_PERIOD = 1;
constexpr std::uint64_t TEST_MAX_OUTPUT_PERIOD = 100000000;
constexpr unsigned QUEUE_COUNT = 4;
constexpr unsigned TRANSFORM_CAPACITY = 2;
constexpr unsigned TASK_PAYLOAD_BITS = 128;  // 64-bit ID + two 32-bit magnitudes.
constexpr unsigned RESULT_PAYLOAD_BITS = 96; // 64-bit ID + 32-bit unsigned GCD.
constexpr int STATS_PRECISION = 12;
constexpr double CLOCK_DUTY_CYCLE = 0.5;
constexpr double FINAL_EDGE_MARGIN_NS = 0.5;

struct Options {
    int m_resultDepth = DEFAULT_RESULT_DEPTH;
    int m_depth = DEFAULT_DEPTH;
    std::uint64_t m_testOutputPeriod = TEST_DEFAULT_OUTPUT_PERIOD;
    std::uint64_t m_maxCycles = DEFAULT_MAX_CYCLES;
    bool m_testTraceEnabled = false;
};

struct Usage {
    std::uint64_t m_sum = 0;
    unsigned m_peak = 0;
    void sample(unsigned count) {
        m_sum += count;
        m_peak = std::max(m_peak, count);
    }
};

// Simulation harness: observes after channel updates, never moves task data.
SC_MODULE(System) {
    sc_core::sc_clock m_clk;
    sc_core::sc_fifo<RawTask> m_parserToTransform;
    sc_core::sc_signal<Payload> m_transformToComputeData{"transform_to_compute_data"};
    sc_core::sc_signal<bool> m_transformToComputeValid{"transform_to_compute_valid"};
    sc_core::sc_signal<bool> m_computeToTransformReady{"compute_to_transform_ready"};
    sc_core::sc_vector<sc_core::sc_signal<Payload>> m_dispatchToComputeData{"dispatch_to_compute_data", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_signal<bool>> m_dispatchToComputeValid{"dispatch_to_compute_valid", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_signal<bool>> m_computeToDispatchReady{"compute_to_dispatch_ready", UNIT_COUNT};
    sc_core::sc_fifo<Result> m_compute0Results;
    sc_core::sc_fifo<Result> m_compute1Results;
    sc_core::sc_fifo<Result> m_collectorToOutput;
    Parser m_parser;
    Transform m_transform;
    Dispatcher m_dispatcher;
    Compute m_compute0;
    Compute m_compute1;
    Collector m_collector;
    Output m_output;
    std::array<Usage, QUEUE_COUNT> m_queues;
    Usage m_pipeline;
    std::uint64_t m_cycles = 0;
    bool m_finished = false;
    System(sc_core::sc_module_name name, std::istream & input, std::ostream & output, TestEventLog & testEventLog,
           const Options& options);
    void connectModules();
    void connectComputes();
    void observe();
    bool drained() const;
};

System::System(sc_core::sc_module_name name, std::istream& input, std::ostream& output, TestEventLog& testEventLog,
               const Options& options)
    : sc_module(name)
    , m_clk("clk", sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS), CLOCK_DUTY_CYCLE,
            sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS), true)
    , m_parserToTransform("parser_to_transform", options.m_depth)
    , m_compute0Results("compute0_results", options.m_resultDepth)
    , m_compute1Results("compute1_results", options.m_resultDepth)
    , m_collectorToOutput("collector_to_output", options.m_depth)
    , m_parser("parser", input, testEventLog)
    , m_transform("transform", testEventLog)
    , m_dispatcher("dispatcher", testEventLog)
    , m_compute0("compute0", testEventLog, 0)
    , m_compute1("compute1", testEventLog, 1)
    , m_collector("collector", testEventLog)
    , m_output("output", output, testEventLog, options.m_testOutputPeriod) {
    connectModules();
    SC_THREAD(observe);
}

void System::connectModules() {
    m_parser.m_clk(m_clk);
    m_transform.m_clk(m_clk);
    m_dispatcher.m_clk(m_clk);
    m_collector.m_clk(m_clk);
    m_output.m_clk(m_clk);
    m_parser.m_tasksOut(m_parserToTransform);
    m_transform.m_tasksIn(m_parserToTransform);
    m_transform.m_dataOut(m_transformToComputeData);
    m_transform.m_validOut(m_transformToComputeValid);
    m_transform.m_readyIn(m_computeToTransformReady);
    m_dispatcher.m_dataIn(m_transformToComputeData);
    m_dispatcher.m_validIn(m_transformToComputeValid);
    m_dispatcher.m_readyOut(m_computeToTransformReady);
    m_collector.m_resultsOut(m_collectorToOutput);
    m_output.m_resultsIn(m_collectorToOutput);
    connectComputes();
}

void System::connectComputes() {
    const std::array<Compute*, UNIT_COUNT> computes{&m_compute0, &m_compute1};
    const std::array<sc_core::sc_fifo<Result>*, UNIT_COUNT> results{&m_compute0Results, &m_compute1Results};
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        computes[unit]->m_clk(m_clk);
        computes[unit]->m_dataIn(m_dispatchToComputeData[unit]);
        computes[unit]->m_validIn(m_dispatchToComputeValid[unit]);
        computes[unit]->m_readyOut(m_computeToDispatchReady[unit]);
        computes[unit]->m_resultsOut(*results[unit]);
        m_dispatcher.m_dataOut[unit](m_dispatchToComputeData[unit]);
        m_dispatcher.m_validOut[unit](m_dispatchToComputeValid[unit]);
        m_dispatcher.m_readyIn[unit](m_computeToDispatchReady[unit]);
        m_collector.m_resultsIn[unit](*results[unit]);
    }
}

bool System::drained() const {
    return m_parser.m_eof && m_output.m_received == m_parser.m_sent && m_transform.empty() &&
           (m_compute0.idle() && m_compute1.idle()) && m_parserToTransform.num_available() == 0 &&
           m_compute0Results.num_available() == 0 && m_compute1Results.num_available() == 0 &&
           m_collectorToOutput.num_available() == 0;
}

void System::observe() {
    while (true) {
        wait(m_clk.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
        m_cycles = currentCycle();
        auto usage = m_queues.begin();
        (usage++)->sample(static_cast<unsigned>(m_parserToTransform.num_available()));
        (usage++)->sample(static_cast<unsigned>(m_compute0Results.num_available()));
        (usage++)->sample(static_cast<unsigned>(m_compute1Results.num_available()));
        usage->sample(static_cast<unsigned>(m_collectorToOutput.num_available()));
        m_pipeline.sample(m_transform.occupancy());
        if (drained()) {
            m_finished = true;
            sc_core::sc_stop();
            return;
        }
    }
}

std::uint64_t positive(const char* text, std::uint64_t limit) {
    const std::string value(text);
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("expected positive integer option");
    }
    const auto number = std::stoull(value);
    if (number == 0 || number > limit) {
        throw std::runtime_error("option out of range");
    }
    return number;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    if (argc > RESULT_DEPTH_ARGUMENT) {
        options.m_resultDepth = static_cast<int>(positive(argv[RESULT_DEPTH_ARGUMENT], MAX_DEPTH));
    }
    if (argc > DEPTH_ARGUMENT) {
        options.m_depth = static_cast<int>(positive(argv[DEPTH_ARGUMENT], MAX_DEPTH));
    }
    if (argc > TEST_OUTPUT_PERIOD_ARGUMENT) {
        options.m_testOutputPeriod = positive(argv[TEST_OUTPUT_PERIOD_ARGUMENT], TEST_MAX_OUTPUT_PERIOD);
    }
    if (argc > MAX_CYCLES_ARGUMENT) {
        options.m_maxCycles = positive(argv[MAX_CYCLES_ARGUMENT], MAX_CYCLE_LIMIT);
    }
    options.m_testTraceEnabled = argc > TEST_EVENTS_ARGUMENT && std::string(argv[TEST_EVENTS_ARGUMENT]) != "-";
    return options;
}

void checkDistinctPaths(char** argv, bool testTraceEnabled) {
    // Prevent accidentally truncating an input or using one file for two outputs.
    std::vector<std::filesystem::path> paths;
    std::vector<int> arguments{INPUT_ARGUMENT, OUTPUT_ARGUMENT, STATS_ARGUMENT};
    if (testTraceEnabled) {
        arguments.push_back(TEST_EVENTS_ARGUMENT);
    }
    for (const int argument : arguments) {
        const auto path = std::filesystem::weakly_canonical(argv[argument]);
        for (const auto& previous : paths) {
            if (path == previous || (std::filesystem::exists(path) && std::filesystem::exists(previous) &&
                                     std::filesystem::equivalent(path, previous))) {
                throw std::runtime_error("input, output, stats and events must be distinct files");
            }
        }
        paths.push_back(path);
    }
}

void writeQueueStats(std::ostream& stats, const System& system, const Options& options) {
    const std::array<const char*, QUEUE_COUNT> names{"parser_to_transform", "compute0_results", "compute1_results",
                                                     "collector_to_output"};
    const std::array<int, QUEUE_COUNT> capacities{options.m_depth, options.m_resultDepth, options.m_resultDepth,
                                                  options.m_depth};
    for (unsigned index = 0; index < QUEUE_COUNT; ++index) {
        stats << names[index] << "_capacity," << capacities[index] << '\n'
              << names[index] << "_average," << static_cast<double>(system.m_queues[index].m_sum) / system.m_cycles
              << '\n'
              << names[index] << "_peak," << system.m_queues[index].m_peak << '\n';
    }
    stats << "transform_registers_capacity," << TRANSFORM_CAPACITY << "\ntransform_registers_average,"
          << static_cast<double>(system.m_pipeline.m_sum) / system.m_cycles << "\ntransform_registers_peak,"
          << system.m_pipeline.m_peak << '\n';
}

void writeHandshakeStats(std::ostream& stats, const System& system, const Options& options) {
    const auto valid = system.m_transform.m_validCycles;
    stats << "handshake_valid_cycles," << valid << "\nhandshake_blocked_cycles," << system.m_transform.m_blockedCycles
          << "\nhandshake_blocked_fraction,"
          << (valid ? static_cast<double>(system.m_transform.m_blockedCycles) / valid : 0.0) << "\ncompute_units,"
          << UNIT_COUNT << "\ncompute_task_slots," << UNIT_COUNT << "\ntransform_slots," << TRANSFORM_CAPACITY
          << "\nfifo_slots," << 2 * options.m_depth + UNIT_COUNT * options.m_resultDepth << "\nbuffer_payload_bits,"
          << options.m_depth * (TASK_PAYLOAD_BITS + RESULT_PAYLOAD_BITS) +
                 UNIT_COUNT * options.m_resultDepth * RESULT_PAYLOAD_BITS + TRANSFORM_CAPACITY * TASK_PAYLOAD_BITS
          << "\ndispatch_idle_other_blocked_cycles," << system.m_dispatcher.m_idleOtherBlockedCycles
          << "\nreorder_wait_cycles," << system.m_collector.m_orderWaitCycles << "\ncollector_output_blocked_cycles,"
          << system.m_collector.m_outputBlockedCycles << '\n';
}

void writeComputeStats(std::ostream& stats, const System& system) {
    const std::array<const Compute*, UNIT_COUNT> computes{&system.m_compute0, &system.m_compute1};
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        stats << "compute" << unit << "_busy_cycles," << computes[unit]->m_busyCycles << '\n'
              << "compute" << unit << "_utilization,"
              << static_cast<double>(computes[unit]->m_busyCycles) / system.m_cycles << '\n'
              << "compute" << unit << "_tasks," << computes[unit]->m_accepted << '\n'
              << "compute" << unit << "_result_wait_cycles," << computes[unit]->m_resultWaitCycles << '\n';
    }
}

void writeStats(std::ostream& stats, const System& system, const Options& options) {
    const auto cycles = system.m_cycles;
    stats << std::setprecision(STATS_PRECISION) << "metric,value\n"
          << "cycles," << cycles << "\ntasks," << system.m_output.m_received << "\nsimulated_time_ns," << cycles
          << "\nthroughput_tasks_per_cycle," << static_cast<double>(system.m_output.m_received) / cycles
          << "\ncompute_busy_cycles," << (system.m_compute0.m_busyCycles + system.m_compute1.m_busyCycles)
          << "\ncompute_utilization,"
          << static_cast<double>((system.m_compute0.m_busyCycles + system.m_compute1.m_busyCycles)) /
                 (UNIT_COUNT * cycles)
          << "\ncompute_result_wait_cycles,"
          << (system.m_compute0.m_resultWaitCycles + system.m_compute1.m_resultWaitCycles) << "\noutput_period,"
          << options.m_testOutputPeriod << '\n';
    writeQueueStats(stats, system, options);
    writeHandshakeStats(stats, system, options);
    writeComputeStats(stats, system);
}

void flushFiles(std::ofstream& output, std::ofstream& stats, std::ofstream& testEvents, bool testTraceEnabled) {
    output.flush();
    stats.flush();
    if (testTraceEnabled) {
        testEvents.flush();
    }
    if (!output || !stats || (testTraceEnabled && !testEvents)) {
        throw std::runtime_error("file flush failed");
    }
}

void openTestEvents(std::ofstream& testEvents, TestEventLog& testEventLog, char** argv, bool testTraceEnabled) {
    if (!testTraceEnabled) {
        return;
    }
    testEvents.open(argv[TEST_EVENTS_ARGUMENT]);
    if (!testEvents) {
        throw std::runtime_error("cannot open events");
    }
    testEvents << "id,event,cycle,a,b,value,latency\n";
    testEventLog.m_testStream = &testEvents;
}

int run(int argc, char** argv) {
    const auto options = parseOptions(argc, argv);
    checkDistinctPaths(argv, options.m_testTraceEnabled);
    std::ifstream input(argv[INPUT_ARGUMENT]);
    if (!input) {
        throw std::runtime_error("cannot open input");
    }
    std::ofstream output(argv[OUTPUT_ARGUMENT]);
    std::ofstream stats(argv[STATS_ARGUMENT]);
    std::ofstream testEvents;
    if (!output || !stats) {
        throw std::runtime_error("cannot open output or stats");
    }
    TestEventLog testEventLog;
    openTestEvents(testEvents, testEventLog, argv, options.m_testTraceEnabled);
    System system("system", input, output, testEventLog, options);
    // Include the last allowed rising edge, but no extra rising edge.
    sc_core::sc_start(
        sc_core::sc_time(static_cast<double>(options.m_maxCycles) + FINAL_EDGE_MARGIN_NS, sc_core::SC_NS));
    if (!system.m_finished) {
        throw std::runtime_error("simulation cycle limit exceeded");
    }
    writeStats(stats, system, options);
    flushFiles(output, stats, testEvents, options.m_testTraceEnabled);
    // Diagnostics belong to the console; OUTPUT contains only final GCD values.
    std::cerr << "PASS: " << system.m_output.m_received << " tasks, " << system.m_cycles << " cycles\n";
    return 0;
}
} // namespace

int sc_main(int argc, char** argv) {
    if (argc < MIN_ARGUMENTS || argc > MAX_ARGUMENTS) {
        std::cerr << "Usage: stage3_gcd INPUT OUTPUT STATS [DEPTH=2] [EVENTS.csv|-]"
                     " [OUTPUT_PERIOD=1] [MAX_CYCLES=1000000] [RESULT_DEPTH=2]\n";
        constexpr int USAGE_ERROR = 2;
        return USAGE_ERROR;
    }
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
