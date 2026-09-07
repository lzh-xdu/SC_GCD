/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file main.cpp
 * @brief Connect and observe the stage 1 simulation.
 */
#include "compute.hpp"
#include "../common/contract.hpp"
#include "output.hpp"
#include "parser.hpp"
#include "transform.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stage1;
namespace {
constexpr int MIN_COMMAND_LINE_ARGUMENT_COUNT = 4;
constexpr int MAX_COMMAND_LINE_ARGUMENT_COUNT = 8;
constexpr int INPUT_FILE_ARGUMENT_INDEX = 1;
constexpr int OUTPUT_FILE_ARGUMENT_INDEX = 2;
constexpr int STATISTICS_FILE_ARGUMENT_INDEX = 3;
constexpr int FIFO_DEPTH_ARGUMENT_INDEX = 4;
constexpr int TEST_EVENT_LOG_ARGUMENT_INDEX = 5;
constexpr int TEST_OUTPUT_PERIOD_ARGUMENT_INDEX = 6;
constexpr int SIMULATION_CYCLE_LIMIT_ARGUMENT_INDEX = 7;
constexpr int DEFAULT_FIFO_CAPACITY_TASKS = 2;
constexpr int MAX_BUFFER_CAPACITY_TASKS = 1000000;
constexpr std::uint64_t DEFAULT_SIMULATION_CYCLE_LIMIT = 1000000;
constexpr std::uint64_t MAX_ALLOWED_SIMULATION_CYCLES = 1000000000;
constexpr std::uint64_t TEST_DEFAULT_OUTPUT_PERIOD_CYCLES = 1;
constexpr std::uint64_t TEST_MAX_OUTPUT_PERIOD_CYCLES = 100000000;
constexpr unsigned OBSERVED_FIFO_COUNT = 3;
constexpr unsigned TRANSFORM_PIPELINE_CAPACITY_TASKS = 2;
constexpr int STATISTICS_SIGNIFICANT_DIGITS = 12;
constexpr double CLOCK_HIGH_TIME_FRACTION = 0.5;
constexpr double SIMULATION_STOP_MARGIN_AFTER_LAST_EDGE_NS = 0.5;

struct Options {
    int m_depth = DEFAULT_FIFO_CAPACITY_TASKS;
    std::uint64_t m_testOutputPeriod = TEST_DEFAULT_OUTPUT_PERIOD_CYCLES;
    std::uint64_t m_maxCycles = DEFAULT_SIMULATION_CYCLE_LIMIT;
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
    sc_core::sc_fifo<OrderedTask> m_transformToCompute;
    sc_core::sc_fifo<Result> m_computeToOutput;
    Parser m_parser;
    Transform m_transform;
    Compute m_compute;
    Output m_output;
    std::array<Usage, OBSERVED_FIFO_COUNT> m_queues;
    Usage m_pipeline;
    std::uint64_t m_cycles = 0;
    bool m_finished = false;
    System(sc_core::sc_module_name name, std::istream & input, std::ostream & output, TestEventLog & testEventLog,
           const Options& options);
    void connectModules();
    void observe();
    bool drained() const;
};

System::System(sc_core::sc_module_name name, std::istream& input, std::ostream& output, TestEventLog& testEventLog,
               const Options& options)
    : sc_module(name)
    , m_clk("clk", sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS), CLOCK_HIGH_TIME_FRACTION,
            sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS), true)
    , m_parserToTransform("parser_to_transform", options.m_depth)
    , m_transformToCompute("transform_to_compute", options.m_depth)
    , m_computeToOutput("compute_to_output", options.m_depth)
    , m_parser("parser", input, testEventLog)
    , m_transform("transform", testEventLog)
    , m_compute("compute", testEventLog)
    , m_output("output", output, testEventLog, options.m_testOutputPeriod) {
    connectModules();
    SC_THREAD(observe);
}

void System::connectModules() {
    m_parser.m_clk(m_clk);
    m_transform.m_clk(m_clk);
    m_compute.m_clk(m_clk);
    m_output.m_clk(m_clk);
    m_parser.m_tasksOut(m_parserToTransform);
    m_transform.m_tasksIn(m_parserToTransform);
    m_transform.m_tasksOut(m_transformToCompute);
    m_compute.m_tasksIn(m_transformToCompute);
    m_compute.m_resultsOut(m_computeToOutput);
    m_output.m_resultsIn(m_computeToOutput);
}

bool System::drained() const {
    return m_parser.m_eof && m_output.m_received == m_parser.m_sent && m_transform.empty() && m_compute.idle() &&
           m_parserToTransform.num_available() == 0 && m_transformToCompute.num_available() == 0 &&
           m_computeToOutput.num_available() == 0;
}

void System::observe() {
    while (true) {
        wait(m_clk.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
        m_cycles = currentCycle();
        auto usage = m_queues.begin();
        (usage++)->sample(static_cast<unsigned>(m_parserToTransform.num_available()));
        (usage++)->sample(static_cast<unsigned>(m_transformToCompute.num_available()));
        usage->sample(static_cast<unsigned>(m_computeToOutput.num_available()));
        m_pipeline.sample(m_transform.occupancy());
        if (drained()) {
            m_finished = true;
            sc_core::sc_stop();
            return;
        }
    }
}

std::uint64_t parsePositiveInteger(const char* text, std::uint64_t limit) {
    requireCondition<std::invalid_argument>(text != nullptr && limit > 0, "invalid option parser arguments");
    const std::string optionText(text);
    requireCondition(!optionText.empty() && optionText.find_first_not_of("0123456789") == std::string::npos,
                     "expected parsePositiveInteger integer option");
    const auto parsedOptionValue = std::stoull(optionText);
    requireCondition(parsedOptionValue > 0 && parsedOptionValue <= limit, "option out of range");
    return parsedOptionValue;
}

Options parseOptions(int argc, char** argv) {
    requireCondition<std::invalid_argument>(argc >= MIN_COMMAND_LINE_ARGUMENT_COUNT &&
                                                argc <= MAX_COMMAND_LINE_ARGUMENT_COUNT && argv != nullptr,
                                            "invalid command line arguments");
    for (int argument = 0; argument < argc; ++argument) {
        requireCondition<std::invalid_argument>(argv[argument] != nullptr, "null command line argument");
    }
    Options options;
    if (argc > FIFO_DEPTH_ARGUMENT_INDEX) {
        options.m_depth =
            static_cast<int>(parsePositiveInteger(argv[FIFO_DEPTH_ARGUMENT_INDEX], MAX_BUFFER_CAPACITY_TASKS));
    }
    if (argc > TEST_OUTPUT_PERIOD_ARGUMENT_INDEX) {
        options.m_testOutputPeriod =
            parsePositiveInteger(argv[TEST_OUTPUT_PERIOD_ARGUMENT_INDEX], TEST_MAX_OUTPUT_PERIOD_CYCLES);
    }
    if (argc > SIMULATION_CYCLE_LIMIT_ARGUMENT_INDEX) {
        options.m_maxCycles =
            parsePositiveInteger(argv[SIMULATION_CYCLE_LIMIT_ARGUMENT_INDEX], MAX_ALLOWED_SIMULATION_CYCLES);
    }
    options.m_testTraceEnabled =
        argc > TEST_EVENT_LOG_ARGUMENT_INDEX && std::string(argv[TEST_EVENT_LOG_ARGUMENT_INDEX]) != "-";
    return options;
}

void checkDistinctPaths(char** argv, bool testTraceEnabled) {
    requireCondition<std::invalid_argument>(argv != nullptr, "null command line arguments");
    // Prevent accidentally truncating an input or using one file for two outputs.
    std::vector<std::filesystem::path> paths;
    std::vector<int> arguments{INPUT_FILE_ARGUMENT_INDEX, OUTPUT_FILE_ARGUMENT_INDEX, STATISTICS_FILE_ARGUMENT_INDEX};
    if (testTraceEnabled) {
        arguments.push_back(TEST_EVENT_LOG_ARGUMENT_INDEX);
    }
    for (const int argument : arguments) {
        const auto path = std::filesystem::weakly_canonical(argv[argument]);
        for (const auto& previous : paths) {
            requireCondition(
                !(path == previous || (std::filesystem::exists(path) && std::filesystem::exists(previous) &&
                                       std::filesystem::equivalent(path, previous))),
                "input, output, stats and events must be distinct files");
        }
        paths.push_back(path);
    }
}

void writeQueueStats(std::ostream& stats, const System& system, int depth) {
    requireCondition(static_cast<bool>(stats), "statistics stream is not writable");
    requireCondition<std::logic_error>(system.m_cycles > 0, "statistics require observed cycles");
    const std::array<const char*, OBSERVED_FIFO_COUNT> fifoMetricNames{"parser_to_transform", "transform_to_compute",
                                                                       "compute_to_output"};
    for (unsigned index = 0; index < OBSERVED_FIFO_COUNT; ++index) {
        stats << fifoMetricNames[index] << "_capacity," << depth << '\n'
              << fifoMetricNames[index] << "_average,"
              << static_cast<double>(system.m_queues[index].m_sum) / system.m_cycles << '\n'
              << fifoMetricNames[index] << "_peak," << system.m_queues[index].m_peak << '\n';
    }
    stats << "transform_registers_capacity," << TRANSFORM_PIPELINE_CAPACITY_TASKS << "\ntransform_registers_average,"
          << static_cast<double>(system.m_pipeline.m_sum) / system.m_cycles << "\ntransform_registers_peak,"
          << system.m_pipeline.m_peak << '\n';
}

void writeStats(std::ostream& stats, const System& system, const Options& options) {
    requireCondition(static_cast<bool>(stats), "statistics stream is not writable");
    requireCondition<std::logic_error>(system.m_cycles > 0, "statistics require observed cycles");
    const auto elapsedSimulationCycles = system.m_cycles;
    stats << std::setprecision(STATISTICS_SIGNIFICANT_DIGITS) << "metric,value\n"
          << "cycles," << elapsedSimulationCycles << "\ntasks," << system.m_output.m_received << "\nsimulated_time_ns,"
          << elapsedSimulationCycles << "\nthroughput_tasks_per_cycle,"
          << static_cast<double>(system.m_output.m_received) / elapsedSimulationCycles << "\ncompute_busy_cycles,"
          << system.m_compute.m_busyCycles << "\ncompute_utilization,"
          << static_cast<double>(system.m_compute.m_busyCycles) / elapsedSimulationCycles
          << "\ncompute_result_wait_cycles," << system.m_compute.m_resultWaitCycles << "\noutput_period,"
          << options.m_testOutputPeriod << '\n';
    writeQueueStats(stats, system, options.m_depth);
}

void flushFiles(std::ofstream& output, std::ofstream& stats, std::ofstream& testEvents, bool testTraceEnabled) {
    output.flush();
    stats.flush();
    if (testTraceEnabled) {
        testEvents.flush();
    }
    requireCondition(output && stats && (!testTraceEnabled || testEvents), "file flush failed");
}

void openTestEvents(std::ofstream& testEvents, TestEventLog& testEventLog, char** argv, bool testTraceEnabled) {
    if (!testTraceEnabled) {
        return;
    }
    testEvents.open(argv[TEST_EVENT_LOG_ARGUMENT_INDEX]);
    requireCondition(static_cast<bool>(testEvents), "cannot open events");
    testEvents << "id,event,cycle,a,b,value,latency\n";
    testEventLog.m_testStream = &testEvents;
}

int run(int argc, char** argv) {
    const auto options = parseOptions(argc, argv);
    checkDistinctPaths(argv, options.m_testTraceEnabled);
    std::ifstream input(argv[INPUT_FILE_ARGUMENT_INDEX]);
    requireCondition(static_cast<bool>(input), "cannot open input");
    std::ofstream output(argv[OUTPUT_FILE_ARGUMENT_INDEX]);
    std::ofstream stats(argv[STATISTICS_FILE_ARGUMENT_INDEX]);
    std::ofstream testEvents;
    requireCondition(output && stats, "cannot open output or stats");
    TestEventLog testEventLog;
    openTestEvents(testEvents, testEventLog, argv, options.m_testTraceEnabled);
    System system("system", input, output, testEventLog, options);
    // Include the last allowed rising edge, but no extra rising edge.
    sc_core::sc_start(sc_core::sc_time(
        static_cast<double>(options.m_maxCycles) + SIMULATION_STOP_MARGIN_AFTER_LAST_EDGE_NS, sc_core::SC_NS));
    requireCondition(system.m_finished, "simulation cycle limit exceeded");
    writeStats(stats, system, options);
    testEventLog.m_statistics.write(stats);
    stats << "compute0_idle_no_input_cycles," << system.m_compute.m_idleNoInputCycles << '\n';
    flushFiles(output, stats, testEvents, options.m_testTraceEnabled);
    // Diagnostics belong to the console; OUTPUT contains only final GCD values.
    std::cerr << "PASS: " << system.m_output.m_received << " tasks, " << system.m_cycles << " cycles\n";
    return 0;
}
} // namespace

int sc_main(int argc, char** argv) {
    if (argc < MIN_COMMAND_LINE_ARGUMENT_COUNT || argc > MAX_COMMAND_LINE_ARGUMENT_COUNT) {
        std::cerr << "Usage: stage1_gcd INPUT OUTPUT STATS [DEPTH=2] [EVENTS.csv|-]"
                     " [OUTPUT_PERIOD=1] [MAX_CYCLES=1000000]\n";
        constexpr int INVALID_ARGUMENT_COUNT_EXIT_CODE = 2;
        return INVALID_ARGUMENT_COUNT_EXIT_CODE;
    }
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
