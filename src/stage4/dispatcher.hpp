/* Copyright (c) 2026 SC_GCD contributors. All rights reserved. */
/** @file dispatcher.hpp
 * @brief Ready-first arbitration with reproducible random ties and window credits.
 */
#pragma once
#include "payload.hpp"

#include "types.hpp"

namespace stage4 {
inline constexpr unsigned UNIT_COUNT = 2;
/**
 * @brief Selects ready compute engines while reserving finite result-window capacity.
 *
 * Interface:
 *
 * m_dataIn / m_validIn --> +----------------------+ --> m_dataOut[0/1] / m_validOut[0/1]
 * m_readyOut          <--  |      Dispatcher      | <-- m_readyIn[0/1]
 * m_baseIn           -->  | window + arbitration |
 *                          +----------------------+
 *
 * Protocol:
 * - Dispatch only if id >= base and id-base < W, reserving a result slot before computation.
 * - Select a ready engine; when both are ready, use a seeded reproducible random choice.
 * - Assert downstream valid only for a ready selected receiver, avoiding an unaccepted transfer changing destination.
 * - If no engine is ready or the window is full, upstream must retain its valid/data.
 * - No internal task buffer; window must match Collector and window/seed must be nonzero.
 * - Invalid window or seed throws invalid_argument; bind ports before sc_start and keep the observer alive.
 *
 * Timing:
 * - route is combinational; advance() samples transfers at scheduled timestamps, after prior updates settle.
 * - Random state advances only on an accepted transfer where both engines are ready.
 * - At most one task transfers per scheduled boundary; no added routing cycle.
 * - Collector base changes take effect through signal delta updates, so released capacity is usable next boundary.
 *
 * Reset:
 * - No reset port or runtime reset protocol.
 * - Construction initializes the random state from seed and zeroes counters.
 */
SC_MODULE(Dispatcher) {
    sc_core::sc_in<Payload> m_dataIn{"data_in"};
    sc_core::sc_in<bool> m_validIn{"valid_in"};
    sc_core::sc_out<bool> m_readyOut{"ready_out"};
    sc_core::sc_in<std::uint64_t> m_baseIn{"base_in"};
    sc_core::sc_vector<sc_core::sc_out<Payload>> m_dataOut{"data_out", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_out<bool>> m_validOut{"valid_out", UNIT_COUNT};
    sc_core::sc_vector<sc_core::sc_in<bool>> m_readyIn{"ready_in", UNIT_COUNT};
    model::instrumentation::DispatchStatistics m_statistics;
    /**
     * @brief window 为未退休任务上限，须与 Collector 一致；seed 是可复现仲裁种子。
     * @throws std::invalid_argument window 或 seed 为零。端口须在 sc_start 前绑定。
     */
    Dispatcher(sc_core::sc_module_name name, EventRecorder & recorder, unsigned window, std::uint32_t seed);

    void advance();
    std::uint64_t nextDelay() const;
    void accountSkipped(std::uint64_t first, std::uint64_t count);

private:
    sc_core::sc_signal<std::uint32_t> m_randomState{"random_state"};
    EventRecorder& m_recorder;
    unsigned m_window;
    bool hasCredit() const;
    unsigned selectedUnit() const;
    void route();
};
} // namespace stage4
