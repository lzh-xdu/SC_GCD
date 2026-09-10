/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file main.cpp
 * @brief Connect and observe the stage 2 handshake simulation.
 *
 * The CLI skeleton (parsePositiveInteger, parseOptions, checkDistinctPaths, flushFiles, openTestEvents)
 * is shared verbatim across all five stage mains; only the accepted positional arguments differ.
 */
#include "../common/contract.hpp"
#include "compute.hpp"
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

using namespace stage2;
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
constexpr unsigned OBSERVED_FIFO_COUNT = 2;
constexpr unsigned TRANSFORM_PIPELINE_CAPACITY_TASKS = 2;
constexpr unsigned TASK_PAYLOAD_BITS = 128;  // 64-bit ID + two 32-bit magnitudes.
constexpr unsigned RESULT_PAYLOAD_BITS = 96; // 64-bit ID + 32-bit unsigned GCD.
constexpr int STATISTICS_SIGNIFICANT_DIGITS = 12;
constexpr double CLOCK_HIGH_TIME_FRACTION = 0.5;
constexpr double SIMULATION_STOP_MARGIN_AFTER_LAST_EDGE_NS = 0.5;

struct Options {
    int m_depth = DEFAULT_FIFO_CAPACITY_TASKS;
    std::uint64_t m_testOutputPeriod = TEST_DEFAULT_OUTPUT_PERIOD_CYCLES;
    std::uint64_t m_maxCycles = DEFAULT_SIMULATION_CYCLE_LIMIT;
    bool m_testTraceEnabled = false;
};

using model::instrumentation::Usage;

// Simulation harness: observes after channel updates, never moves task data.
SC_MODULE(System) {
    sc_core::sc_clock m_clk;
    sc_core::sc_fifo<RawTask> m_parserToTransform;
    sc_core::sc_signal<Payload> m_transformToComputeData{"transform_to_compute_data"};
    sc_core::sc_signal<bool> m_transformToComputeValid{"transform_to_compute_valid"};
    sc_core::sc_signal<bool> m_computeToTransformReady{"compute_to_transform_ready"};
    sc_core::sc_fifo<Result> m_computeToOutput;
    Parser m_parser;
    Transform m_transform;
    Compute m_compute;
    Output m_output;
    std::array<Usage, OBSERVED_FIFO_COUNT> m_queues;
    Usage m_pipeline;
    std::uint64_t m_cycles = 0;
    bool m_finished = false;
    System(sc_core::sc_module_name name, std::istream & input, std::ostream & output, EventRecorder & recorder,
           const Options& options);
    void connectModules();
    void observe();
    [[nodiscard]] bool drained() const;
};

System::System(sc_core::sc_module_name name, std::istream& input, std::ostream& output, EventRecorder& recorder,
               const Options& options)
    : sc_module(name)
    , m_clk("clk", sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS), CLOCK_HIGH_TIME_FRACTION,
            sc_core::sc_time(CLOCK_PERIOD_NS, sc_core::SC_NS), true)
    , m_parserToTransform("parser_to_transform", options.m_depth)
    , m_computeToOutput("compute_to_output", options.m_depth)
    , m_parser("parser", input, recorder)
    , m_transform("transform", recorder)
    , m_compute("compute", recorder)
    , m_output("output", output, recorder, options.m_testOutputPeriod) {
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
    m_transform.m_dataOut(m_transformToComputeData);
    m_transform.m_validOut(m_transformToComputeValid);
    m_transform.m_readyIn(m_computeToTransformReady);
    m_compute.m_dataIn(m_transformToComputeData);
    m_compute.m_validIn(m_transformToComputeValid);
    m_compute.m_readyOut(m_computeToTransformReady);
    m_compute.m_resultsOut(m_computeToOutput);
    m_output.m_resultsIn(m_computeToOutput);
}

bool System::drained() const {
    return m_parser.m_eof && m_output.m_received == m_parser.m_sent && m_transform.empty() && m_compute.idle() &&
           m_parserToTransform.num_available() == 0 && m_computeToOutput.num_available() == 0;
}

void System::observe() {
    while (true) {
        wait(m_clk.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
        m_cycles = currentCycle();
        auto usage = m_queues.begin();
        (usage++)->sample(static_cast<unsigned>(m_parserToTransform.num_available()));
        usage->sample(static_cast<unsigned>(m_computeToOutput.num_available()));
        m_pipeline.sample(m_transform.occupancy());
        if (drained()) {
            m_finished = true;
            sc_core::sc_stop();
            return;
        }
    }
}

[[nodiscard]] std::uint64_t parsePositiveInteger(const char* text, std::uint64_t limit) {
    assertCondition<std::invalid_argument>(text != nullptr && limit > 0, "invalid option parser arguments");
    const std::string optionText(text);
    assertCondition(!optionText.empty() && optionText.find_first_not_of("0123456789") == std::string::npos,
                    "expected a decimal integer option");
    constexpr std::uint64_t DECIMAL_RADIX = 10;
    std::uint64_t parsedOptionValue = 0;
    for (const char character : optionText) {
        const auto digit = static_cast<std::uint64_t>(character - '0');
        // Check before multiplication/addition, including when limit is UINT64_MAX.
        assertCondition(parsedOptionValue < limit / DECIMAL_RADIX ||
                            (parsedOptionValue == limit / DECIMAL_RADIX && digit <= limit % DECIMAL_RADIX),
                        "option out of range");
        parsedOptionValue = parsedOptionValue * DECIMAL_RADIX + digit;
    }
    assertCondition(parsedOptionValue > 0, "option out of range");
    return parsedOptionValue;
}

Options parseOptions(int argc, char** argv) {
    assertCondition<std::invalid_argument>(argc >= MIN_COMMAND_LINE_ARGUMENT_COUNT &&
                                               argc <= MAX_COMMAND_LINE_ARGUMENT_COUNT && argv != nullptr,
                                           "invalid command line arguments");
    for (int argument = 0; argument < argc; ++argument) {
        assertCondition<std::invalid_argument>(argv[argument] != nullptr, "null command line argument");
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
    assertCondition<std::invalid_argument>(argv != nullptr, "null command line arguments");
    // Prevent accidentally truncating an input or using one file for two outputs.
    std::vector<std::filesystem::path> paths;
    std::vector<int> arguments{INPUT_FILE_ARGUMENT_INDEX, OUTPUT_FILE_ARGUMENT_INDEX, STATISTICS_FILE_ARGUMENT_INDEX};
    if (testTraceEnabled) {
        arguments.push_back(TEST_EVENT_LOG_ARGUMENT_INDEX);
    }
    for (const int argument : arguments) {
        const auto path = std::filesystem::weakly_canonical(argv[argument]);
        for (const auto& previous : paths) {
            assertCondition(!(path == previous || (std::filesystem::exists(path) && std::filesystem::exists(previous) &&
                                                   std::filesystem::equivalent(path, previous))),
                            "input, output, stats and events must be distinct files");
        }
        paths.push_back(path);
    }
}

void writeQueueStats(std::ostream& stats, const System& system, int depth) {
    const std::array<const char*, OBSERVED_FIFO_COUNT> fifoMetricNames{"parser_to_transform", "compute_to_output"};
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

void writeHandshakeStats(std::ostream& stats, const System& system, const Options& options) {
    const auto handshakeValidCycles = system.m_transform.m_statistics.m_validCycles;
    stats << "handshake_valid_cycles," << handshakeValidCycles << "\nhandshake_blocked_cycles,"
          << system.m_transform.m_statistics.m_blockedCycles << "\nhandshake_blocked_fraction,"
          << (handshakeValidCycles
                  ? static_cast<double>(system.m_transform.m_statistics.m_blockedCycles) / handshakeValidCycles
                  : 0.0)
          << "\ncompute_units,1\ncompute_task_slots,1\ntransform_slots,2\nfifo_slots,"
          << OBSERVED_FIFO_COUNT * options.m_depth << "\nbuffer_payload_bits,"
          << options.m_depth * (TASK_PAYLOAD_BITS + RESULT_PAYLOAD_BITS) +
                 TRANSFORM_PIPELINE_CAPACITY_TASKS * TASK_PAYLOAD_BITS
          << '\n';
}

void writeStats(std::ostream& stats, const System& system, const Options& options) {
    assertCondition(static_cast<bool>(stats), "statistics stream is not writable");
    assertCondition<std::logic_error>(system.m_cycles > 0, "statistics require observed cycles");
    const auto elapsedSimulationCycles = system.m_cycles;
    stats << std::setprecision(STATISTICS_SIGNIFICANT_DIGITS) << "metric,value\n"
          << "cycles," << elapsedSimulationCycles << "\ntasks," << system.m_output.m_received << "\nsimulated_time_ns,"
          << static_cast<double>(elapsedSimulationCycles) * CLOCK_PERIOD_NS << "\nthroughput_tasks_per_cycle,"
          << static_cast<double>(system.m_output.m_received) / elapsedSimulationCycles << "\ncompute_busy_cycles,"
          << system.m_compute.m_statistics.m_busyCycles << "\ncompute_utilization,"
          << static_cast<double>(system.m_compute.m_statistics.m_busyCycles) / elapsedSimulationCycles
          << "\ncompute_result_wait_cycles," << system.m_compute.m_statistics.m_resultWaitCycles << "\noutput_period,"
          << options.m_testOutputPeriod << '\n';
    writeQueueStats(stats, system, options.m_depth);
    writeHandshakeStats(stats, system, options);
}

void flushFiles(std::ofstream& output, std::ofstream& stats, std::ofstream& testEvents, bool testTraceEnabled) {
    output.flush();
    stats.flush();
    if (testTraceEnabled) {
        testEvents.flush();
    }
    assertCondition(output && stats && (!testTraceEnabled || testEvents), "file flush failed");
}

void openTestEvents(std::ofstream& testEvents, EventRecorder& recorder, char** argv, bool testTraceEnabled) {
    if (!testTraceEnabled) {
        return;
    }
    testEvents.open(argv[TEST_EVENT_LOG_ARGUMENT_INDEX]);
    assertCondition(static_cast<bool>(testEvents), "cannot open events");
    testEvents << "id,event,cycle,a,b,value,latency\n";
    recorder.m_testStream = &testEvents;
}

int run(int argc, char** argv) {
    const auto options = parseOptions(argc, argv);
    checkDistinctPaths(argv, options.m_testTraceEnabled);
    std::ifstream input(argv[INPUT_FILE_ARGUMENT_INDEX]);
    assertCondition(static_cast<bool>(input), "cannot open input");
    std::ofstream output(argv[OUTPUT_FILE_ARGUMENT_INDEX]);
    std::ofstream stats(argv[STATISTICS_FILE_ARGUMENT_INDEX]);
    std::ofstream testEvents;
    assertCondition(output && stats, "cannot open output or stats");
    EventRecorder recorder;
    openTestEvents(testEvents, recorder, argv, options.m_testTraceEnabled);
    System system("system", input, output, recorder, options);
    // Include the last allowed rising edge, but no extra rising edge.
    sc_core::sc_start(sc_core::sc_time(
        static_cast<double>(options.m_maxCycles) + SIMULATION_STOP_MARGIN_AFTER_LAST_EDGE_NS, sc_core::SC_NS));
    assertCondition(system.m_finished, "simulation cycle limit exceeded");
    writeStats(stats, system, options);
    recorder.m_statistics.write(stats);
    stats << "compute0_idle_no_input_cycles," << system.m_compute.m_statistics.m_idleNoInputCycles << '\n';
    flushFiles(output, stats, testEvents, options.m_testTraceEnabled);
    // Diagnostics belong to the console; OUTPUT contains only final GCD values.
    std::cerr << "PASS: " << system.m_output.m_received << " tasks, " << system.m_cycles << " cycles\n";
    return 0;
}
} // namespace

int sc_main(int argc, char** argv) {
    if (argc < MIN_COMMAND_LINE_ARGUMENT_COUNT || argc > MAX_COMMAND_LINE_ARGUMENT_COUNT) {
        std::cerr << "Usage: stage2_gcd INPUT OUTPUT STATS [DEPTH=2] [EVENTS.csv|-]"
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
