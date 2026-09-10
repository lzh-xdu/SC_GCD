/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file entry.cpp
 * @brief Wrap an unchanged model entry point with host and kernel measurements.
 */
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

// Each profiling target supplies the model source; normal binaries are unaffected.
#define sc_main profileModelMain
#include PROFILE_MODEL_SOURCE
#undef sc_main

#include <chrono>

namespace {
constexpr double FILETIME_TICKS_PER_MS = 10000.0;

double fileTimeMs(const FILETIME& value) {
    ULARGE_INTEGER ticks;
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    return static_cast<double>(ticks.QuadPart) / FILETIME_TICKS_PER_MS;
}
void writeHostProfile(double elapsedMs) {
    PROCESS_MEMORY_COUNTERS memory{};
    memory.cb = sizeof(memory);
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    assertCondition(GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)) != 0,
                     "cannot measure process memory");
    assertCondition(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) != 0,
                     "cannot measure process CPU time");
    std::cerr << std::setprecision(12) << "HOST_PROFILE model_ms=" << elapsedMs
              << " cpu_ms=" << fileTimeMs(kernel) + fileTimeMs(user)
              << " peak_working_set_bytes=" << memory.PeakWorkingSetSize
              << " peak_commit_bytes=" << memory.PeakPagefileUsage << " delta_cycles=" << sc_core::sc_delta_count()
              << '\n';
}
} // namespace

extern "C" int sc_main(int argc, char** argv) {
    const auto started = std::chrono::steady_clock::now();
    const auto result = profileModelMain(argc, argv);
    const auto elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    writeHostProfile(elapsedMs);
    return result;
}
