#pragma once

#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/pcb/spice_simulation_engine.hpp"

#include <chrono>
#include <cstddef>
#include <string>

namespace shunya::perf {

struct HotLoopBenchmarkResult {
    std::string benchmark_name;
    size_t iterations{0};
    double total_time_ms{0.0};
    double mean_time_us{0.0};
    double min_time_us{0.0};
    double max_time_us{0.0};
    bool meets_sub_millisecond_target{false}; // mean_time_us < 1000.0 us
};

/**
 * @brief High-resolution profiling suite ensuring Phase 3 Material Mixing
 *        and Circuit solves execute strictly in sub-millisecond time.
 * (Roadmap task PH4-02.04.005)
 */
class HotLoopProfiler {
public:
    [[nodiscard]] static HotLoopBenchmarkResult profile_material_mixing(
        const mat::MixtureExperiment& experiment,
        size_t iterations = 500);

    [[nodiscard]] static HotLoopBenchmarkResult profile_circuit_dc_solve(
        const core::domain::CircuitNetlist& netlist,
        size_t iterations = 500);

    [[nodiscard]] static bool verify_sub_millisecond_guarantees(
        double* out_mixing_mean_us = nullptr,
        double* out_circuit_mean_us = nullptr);
};

} // namespace shunya::perf
