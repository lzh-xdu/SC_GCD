/*
 * Copyright (c) 2026 SC_GCD contributors. All rights reserved.
 */

/**
 * @file main.cpp
 * @brief Connect the window model and schedule only actionable event timestamps.
 *
 * The CLI skeleton (parsePositiveInteger, parseOptions, checkDistinctPaths, flushFiles, openTestEvents)
 * is shared verbatim across all five stage mains; only the accepted positional arguments differ.
 */
#include "collector.hpp"
#include "common/contract.hpp"
#include "compute.hpp"
#include "dispatcher.hpp"
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
#include <sstream>
#include <string>
#include <vector>

using namespace stage4;
namespace {
constexpr int MIN_COMMAND_LINE_ARGUMENT_COUNT = 4;
constexpr int MAX_COMMAND_LINE_ARGUMENT_COUNT = 11;
constexpr int REORDER_WINDOW_ARGUMENT_INDEX = 9;
constexpr int ARBITRATION_SEED_ARGUMENT_INDEX = 10;
constexpr unsigned DEFAULT_REORDER_WINDOW_CAPACITY_TASKS = 8;
constexpr std::uint32_t DEFAULT_ARBITRATION_RANDOM_SEED = 1;
constexpr std::uint64_t MAX_ARBITRATION_RANDOM_SEED = 4294967295ULL;
constexpr int COMPUTE_RESULT_FIFO_DEPTH_ARGUMENT_INDEX = 8;
constexpr unsigned DEFAULT_COMPUTE_RESULT_FIFO_CAPACITY_TASKS = 2;
constexpr int INPUT_FILE_ARGUMENT_INDEX = 1;
constexpr int OUTPUT_FILE_ARGUMENT_INDEX = 2;
constexpr int STATISTICS_FILE_ARGUMENT_INDEX = 3;
constexpr int FIFO_DEPTH_ARGUMENT_INDEX = 4;
constexpr int TEST_EVENT_LOG_ARGUMENT_INDEX = 5;
constexpr int TEST_OUTPUT_PERIOD_ARGUMENT_INDEX = 6;
constexpr int SIMULATION_CYCLE_LIMIT_ARGUMENT_INDEX = 7;
constexpr unsigned DEFAULT_FIFO_CAPACITY_TASKS = 2;
constexpr unsigned MAX_BUFFER_CAPACITY_TASKS = 1000000;
constexpr std::uint64_t DEFAULT_SIMULATION_CYCLE_LIMIT = 1000000;
constexpr std::uint64_t MAX_ALLOWED_SIMULATION_CYCLES = 1000000000;
constexpr std::uint64_t TEST_DEFAULT_OUTPUT_PERIOD_CYCLES = 1;
constexpr std::uint64_t TEST_MAX_OUTPUT_PERIOD_CYCLES = 100000000;
constexpr unsigned OBSERVED_FIFO_COUNT = 4;
constexpr unsigned TRANSFORM_PIPELINE_CAPACITY_TASKS = 2;
constexpr unsigned TASK_PAYLOAD_BITS = 128;  // 64-bit ID + two 32-bit magnitudes.
constexpr unsigned RESULT_PAYLOAD_BITS = 96; // 64-bit ID + 32-bit unsigned GCD.
constexpr int STATISTICS_SIGNIFICANT_DIGITS = 12;
constexpr double SIMULATION_STOP_MARGIN_AFTER_LAST_EDGE_NS = 0.5;

struct Options {
    unsigned m_window = DEFAULT_REORDER_WINDOW_CAPACITY_TASKS;
    std::uint32_t m_seed = DEFAULT_ARBITRATION_RANDOM_SEED;
    unsigned m_resultDepth = DEFAULT_COMPUTE_RESULT_FIFO_CAPACITY_TASKS;
    unsigned m_depth = DEFAULT_FIFO_CAPACITY_TASKS;
    std::uint64_t m_testOutputPeriod = TEST_DEFAULT_OUTPUT_PERIOD_CYCLES;
    std::uint64_t m_maxCycles = DEFAULT_SIMULATION_CYCLE_LIMIT;
    bool m_testTraceEnabled = false;
};

using model::instrumentation::Usage;

// Sparse event scheduler: modules own transitions; observations integrate unchanged intervals.
SC_MODULE(System) {
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
    EventRecorder& m_recorder;
    std::array<Usage, OBSERVED_FIFO_COUNT> m_queues;
    Usage m_pipeline;
    Usage m_windowResults;
    Usage m_windowReserved;
    sc_core::sc_signal<std::uint64_t> m_windowBase{"window_base"};
    std::uint64_t m_cycles = 0;
    bool m_finished = false;
    System(sc_core::sc_module_name name, std::istream & input, std::ostream & output, EventRecorder & recorder,
           const Options& options);
    void connectModules();
    void connectComputes();
    void runEvents();
    void sampleUsage();
    void accountSkipped(std::uint64_t count);
    [[nodiscard]] std::uint64_t nextDelay() const;
    std::uint64_t m_activations = 0;
    std::uint64_t m_maxJump = 0;
    [[nodiscard]] bool drained() const;
};

System::System(sc_core::sc_module_name name, std::istream& input, std::ostream& output, EventRecorder& recorder,
               const Options& options)
    : sc_module(name)
    , m_parserToTransform("parser_to_transform", options.m_depth)
    , m_compute0Results("compute0_results", options.m_resultDepth)
    , m_compute1Results("compute1_results", options.m_resultDepth)
    , m_collectorToOutput("collector_to_output", options.m_depth)
    , m_parser("parser", input, recorder, true)
    , m_transform("transform", recorder)
    , m_dispatcher("dispatcher", recorder, options.m_window, options.m_seed)
    , m_compute0("compute0", recorder, 0)
    , m_compute1("compute1", recorder, 1)
    , m_collector("collector", recorder, options.m_window)
    , m_output("output", output, recorder, options.m_testOutputPeriod)
    , m_recorder(recorder) {
    connectModules();
    SC_THREAD(runEvents);
}

void System::connectModules() {
    m_parser.m_tasksOut(m_parserToTransform);
    m_transform.m_tasksIn(m_parserToTransform);
    m_transform.m_dataOut(m_transformToComputeData);
    m_transform.m_validOut(m_transformToComputeValid);
    m_transform.m_readyIn(m_computeToTransformReady);
    m_dispatcher.m_dataIn(m_transformToComputeData);
    m_dispatcher.m_validIn(m_transformToComputeValid);
    m_dispatcher.m_readyOut(m_computeToTransformReady);
    m_dispatcher.m_baseIn(m_windowBase);
    m_collector.m_baseOut(m_windowBase);
    m_collector.m_resultsOut(m_collectorToOutput);
    m_output.m_resultsIn(m_collectorToOutput);
    connectComputes();
}

void System::connectComputes() {
    const std::array<Compute*, UNIT_COUNT> computeUnits{&m_compute0, &m_compute1};
    const std::array<sc_core::sc_fifo<Result>*, UNIT_COUNT> computeResultFifos{&m_compute0Results, &m_compute1Results};
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        computeUnits[unit]->m_dataIn(m_dispatchToComputeData[unit]);
        computeUnits[unit]->m_validIn(m_dispatchToComputeValid[unit]);
        computeUnits[unit]->m_readyOut(m_computeToDispatchReady[unit]);
        computeUnits[unit]->m_resultsOut(*computeResultFifos[unit]);
        m_dispatcher.m_dataOut[unit](m_dispatchToComputeData[unit]);
        m_dispatcher.m_validOut[unit](m_dispatchToComputeValid[unit]);
        m_dispatcher.m_readyIn[unit](m_computeToDispatchReady[unit]);
        m_collector.m_resultsIn[unit](*computeResultFifos[unit]);
    }
}

bool System::drained() const {
    return m_parser.m_eof && m_output.m_received == m_parser.m_sent && m_transform.empty() &&
           (m_compute0.idle() && m_compute1.idle()) && m_parserToTransform.num_available() == 0 &&
           m_compute0Results.num_available() == 0 && m_compute1Results.num_available() == 0 &&
           m_collectorToOutput.num_available() == 0 && m_collector.occupancy() == 0;
}

std::uint64_t System::nextDelay() const {
    return std::min({m_parser.canRead() ? 1 : model::timing::NO_DEADLINE, m_transform.nextDelay(),
                     m_dispatcher.nextDelay(), m_compute0.nextDelay(), m_compute1.nextDelay(), m_collector.nextDelay(),
                     m_output.nextDelay()});
}
void System::accountSkipped(std::uint64_t count) {
    if (count == 0) {
        return;
    }
    const auto first = m_cycles + 1;
    m_transform.accountSkipped(first, count);
    m_dispatcher.accountSkipped(first, count);
    m_compute0.accountSkipped(first, count);
    m_compute1.accountSkipped(first, count);
    m_collector.accountSkipped(first, count);
    for (auto& usage : m_queues) {
        usage.hold(count);
    }
    m_pipeline.hold(count);
    m_windowResults.hold(count);
    m_windowReserved.hold(count);
}
void System::sampleUsage() {
    auto usage = m_queues.begin();
    (usage++)->sample(static_cast<std::uint64_t>(m_parserToTransform.num_available()));
    (usage++)->sample(static_cast<std::uint64_t>(m_compute0Results.num_available()));
    (usage++)->sample(static_cast<std::uint64_t>(m_compute1Results.num_available()));
    usage->sample(static_cast<std::uint64_t>(m_collectorToOutput.num_available()));
    m_pipeline.sample(m_transform.occupancy());
    m_windowResults.sample(m_collector.occupancy());
    m_windowReserved.sample(m_dispatcher.m_statistics.m_dispatched - m_collector.m_nextId);
}
void System::runEvents() {
    // Channel update -> combinational route -> route output update. No simulated time passes.
    // The three zero-time deltas do exactly that, in order: 1) the sc_fifo/sc_signal writes
    // from the previous boundary's advance() calls commit, 2) Dispatcher::route re-evaluates
    // its combinational outputs on the new values, 3) route's own writes commit, so every
    // module reads a fully settled network at the next scheduled boundary.
    constexpr unsigned SETTLE_DELTA_COUNT = 3;
    while (true) {
        for (unsigned phase = 0; phase < SETTLE_DELTA_COUNT; ++phase) {
            wait(sc_core::SC_ZERO_TIME);
        }
        if (m_cycles != 0) {
            sampleUsage();
            if (drained()) {
                m_finished = true;
                sc_core::sc_stop();
                return;
            }
        }
        const auto delay = nextDelay();
        assertCondition<std::logic_error>(delay != model::timing::NO_DEADLINE,
                                          "event model deadlock: no pending state transition");
        accountSkipped(delay - 1);
        // All rows before the next boundary are now known; no simulated time is advanced by output.
        m_recorder.flushBatch();
        wait(sc_core::sc_time(static_cast<double>(delay) * CYCLE_DURATION_NS, sc_core::SC_NS));
        m_cycles = currentCycle();
        ++m_activations;
        m_maxJump = std::max(m_maxJump, delay);
        // All handlers see old channels; sc_fifo/sc_signal commit in the following delta.
        m_parser.readAndSend();
        m_transform.advance();
        m_dispatcher.advance();
        m_compute0.advance();
        m_compute1.advance();
        m_collector.advance();
        m_output.advance();
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
    if (argc > REORDER_WINDOW_ARGUMENT_INDEX) {
        options.m_window =
            static_cast<unsigned>(parsePositiveInteger(argv[REORDER_WINDOW_ARGUMENT_INDEX], MAX_BUFFER_CAPACITY_TASKS));
    }
    if (argc > ARBITRATION_SEED_ARGUMENT_INDEX) {
        options.m_seed = static_cast<std::uint32_t>(
            parsePositiveInteger(argv[ARBITRATION_SEED_ARGUMENT_INDEX], MAX_ARBITRATION_RANDOM_SEED));
    }
    if (argc > COMPUTE_RESULT_FIFO_DEPTH_ARGUMENT_INDEX) {
        options.m_resultDepth = static_cast<unsigned>(
            parsePositiveInteger(argv[COMPUTE_RESULT_FIFO_DEPTH_ARGUMENT_INDEX], MAX_BUFFER_CAPACITY_TASKS));
    }
    if (argc > FIFO_DEPTH_ARGUMENT_INDEX) {
        options.m_depth =
            static_cast<unsigned>(parsePositiveInteger(argv[FIFO_DEPTH_ARGUMENT_INDEX], MAX_BUFFER_CAPACITY_TASKS));
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

void writeQueueStats(std::ostream& stats, const System& system, const Options& options) {
    const std::array<const char*, OBSERVED_FIFO_COUNT> fifoMetricNames{"parser_to_transform", "compute0_results",
                                                                       "compute1_results", "collector_to_output"};
    const std::array<unsigned, OBSERVED_FIFO_COUNT> fifoCapacitiesTasks{options.m_depth, options.m_resultDepth,
                                                                        options.m_resultDepth, options.m_depth};
    for (unsigned index = 0; index < OBSERVED_FIFO_COUNT; ++index) {
        stats << fifoMetricNames[index] << "_capacity," << fifoCapacitiesTasks[index] << '\n'
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
          << "\ncompute_units," << UNIT_COUNT << "\ncompute_task_slots," << UNIT_COUNT << "\ntransform_slots,"
          << TRANSFORM_PIPELINE_CAPACITY_TASKS << "\nfifo_slots,"
          << 2 * options.m_depth + UNIT_COUNT * options.m_resultDepth << "\nbuffer_payload_bits,"
          << options.m_depth * (TASK_PAYLOAD_BITS + RESULT_PAYLOAD_BITS) +
                 (UNIT_COUNT * options.m_resultDepth + options.m_window) * RESULT_PAYLOAD_BITS +
                 TRANSFORM_PIPELINE_CAPACITY_TASKS * TASK_PAYLOAD_BITS
          << "\nwindow_blocked_cycles," << system.m_dispatcher.m_statistics.m_windowBlockedCycles
          << "\nengines_blocked_cycles," << system.m_dispatcher.m_statistics.m_engineBlockedCycles
          << "\nrandom_choices," << system.m_dispatcher.m_statistics.m_randomChoices << "\nrandom_seed,"
          << options.m_seed << "\nreorder_wait_cycles," << system.m_collector.m_statistics.m_orderWaitCycles
          << "\ncollector_output_blocked_cycles," << system.m_collector.m_statistics.m_outputBlockedCycles << '\n';
}

void writeComputeStats(std::ostream& stats, const System& system) {
    const std::array<const Compute*, UNIT_COUNT> computeUnits{&system.m_compute0, &system.m_compute1};
    for (unsigned unit = 0; unit < UNIT_COUNT; ++unit) {
        stats << "compute" << unit << "_busy_cycles," << computeUnits[unit]->m_statistics.m_busyCycles << '\n'
              << "compute" << unit << "_utilization,"
              << static_cast<double>(computeUnits[unit]->m_statistics.m_busyCycles) / system.m_cycles << '\n'
              << "compute" << unit << "_tasks," << computeUnits[unit]->m_statistics.m_accepted << '\n'
              << "compute" << unit << "_result_wait_cycles," << computeUnits[unit]->m_statistics.m_resultWaitCycles
              << '\n';
    }
}

void writeWindowStats(std::ostream& stats, const System& system, const Options& options) {
    stats << "window_capacity," << options.m_window << "\nwindow_results_peak," << system.m_windowResults.m_peak
          << "\nwindow_results_average," << static_cast<double>(system.m_windowResults.m_sum) / system.m_cycles
          << "\nwindow_reserved_peak," << system.m_windowReserved.m_peak << "\nwindow_reserved_average,"
          << static_cast<double>(system.m_windowReserved.m_sum) / system.m_cycles << "\nwindow_valid_bits,"
          << options.m_window << '\n';
}

void writeStats(std::ostream& stats, const System& system, const Options& options) {
    assertCondition(static_cast<bool>(stats), "statistics stream is not writable");
    assertCondition<std::logic_error>(system.m_cycles > 0, "statistics require observed cycles");
    const auto elapsedSimulationCycles = system.m_cycles;
    stats << std::setprecision(STATISTICS_SIGNIFICANT_DIGITS) << "metric,value\n"
          << "cycles," << elapsedSimulationCycles << "\ntasks," << system.m_output.m_received << "\nsimulated_time_ns,"
          << static_cast<double>(elapsedSimulationCycles) * CYCLE_DURATION_NS << "\nthroughput_tasks_per_cycle,"
          << static_cast<double>(system.m_output.m_received) / elapsedSimulationCycles << "\ncompute_busy_cycles,"
          << (system.m_compute0.m_statistics.m_busyCycles + system.m_compute1.m_statistics.m_busyCycles)
          << "\ncompute_utilization,"
          << static_cast<double>(
                 (system.m_compute0.m_statistics.m_busyCycles + system.m_compute1.m_statistics.m_busyCycles)) /
                 (UNIT_COUNT * elapsedSimulationCycles)
          << "\ncompute_result_wait_cycles,"
          << (system.m_compute0.m_statistics.m_resultWaitCycles + system.m_compute1.m_statistics.m_resultWaitCycles)
          << "\noutput_period," << options.m_testOutputPeriod << '\n';
    writeQueueStats(stats, system, options);
    writeHandshakeStats(stats, system, options);
    writeComputeStats(stats, system);
    writeWindowStats(stats, system, options);
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
    stats << "window_blocked_with_ready_cycles," << system.m_dispatcher.m_statistics.m_windowBlockedWithReadyCycles
          << '\n';
    stats << "compute0_idle_no_input_cycles," << system.m_compute0.m_statistics.m_idleNoInputCycles << '\n'
          << "compute1_idle_no_input_cycles," << system.m_compute1.m_statistics.m_idleNoInputCycles << '\n';
    recorder.flushTrace();
    flushFiles(output, stats, testEvents, options.m_testTraceEnabled);
    // Diagnostics belong to the console; OUTPUT contains only final GCD values.
    std::cerr << "PASS: " << system.m_output.m_received << " tasks, " << system.m_cycles << " cycles\n";
    std::cerr << "EVENT_SCHEDULER activations=" << system.m_activations << " max_jump_cycles=" << system.m_maxJump
              << '\n';
    return 0;
}
} // namespace

int sc_main(int argc, char** argv) {
    if (argc < MIN_COMMAND_LINE_ARGUMENT_COUNT || argc > MAX_COMMAND_LINE_ARGUMENT_COUNT) {
        // Keep the usage text and the parsed defaults in one place: the named constants below.
        std::ostringstream usage;
        usage << "Usage: stage4_gcd INPUT OUTPUT STATS [DEPTH=" << DEFAULT_FIFO_CAPACITY_TASKS << "] [EVENTS.csv|-]"
              << " [OUTPUT_PERIOD=" << TEST_DEFAULT_OUTPUT_PERIOD_CYCLES
              << "] [MAX_CYCLES=" << DEFAULT_SIMULATION_CYCLE_LIMIT
              << "] [RESULT_DEPTH=" << DEFAULT_COMPUTE_RESULT_FIFO_CAPACITY_TASKS
              << "] [WINDOW=" << DEFAULT_REORDER_WINDOW_CAPACITY_TASKS
              << "] [SEED=" << DEFAULT_ARBITRATION_RANDOM_SEED << "]\n";
        std::cerr << usage.str();
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
