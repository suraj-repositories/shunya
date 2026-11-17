#include "shunya/perf/simulation_hardware_profiler.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

namespace shunya::perf {

uint64_t SimulationHardwareProfiler::read_cpu_cycles() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    return __rdtsc();
#else
    return static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
#endif
}

void SimulationHardwareProfiler::start_stage(SimulationStage stage) noexcept {
    const size_t idx = static_cast<size_t>(stage);
    if (idx >= STAGE_COUNT) return;

    active_probes_[idx].start_time = std::chrono::high_resolution_clock::now();
    active_probes_[idx].start_cycles = read_cpu_cycles();
    active_probes_[idx].active = true;
}

void SimulationHardwareProfiler::stop_stage(SimulationStage stage) noexcept {
    const auto end_time = std::chrono::high_resolution_clock::now();
    const uint64_t end_cycles = read_cpu_cycles();

    const size_t idx = static_cast<size_t>(stage);
    if (idx >= STAGE_COUNT || !active_probes_[idx].active) return;

    const auto start_time = active_probes_[idx].start_time;
    const uint64_t start_cycles = active_probes_[idx].start_cycles;
    active_probes_[idx].active = false;

    const double elapsed_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    const uint64_t cycles = (end_cycles >= start_cycles) ? (end_cycles - start_cycles) : 0;

    auto& m = metrics_[idx];
    ++m.call_count;
    m.total_time_us += elapsed_us;
    m.min_time_us = std::min(m.min_time_us, elapsed_us);
    m.max_time_us = std::max(m.max_time_us, elapsed_us);
    m.last_time_us = elapsed_us;
    m.total_cpu_cycles += cycles;

    if (m.call_count == 1) {
        m.ema_time_us = elapsed_us;
    } else {
        m.ema_time_us = 0.9 * m.ema_time_us + 0.1 * elapsed_us;
    }
}

const StageMetrics& SimulationHardwareProfiler::stage_metrics(SimulationStage stage) const noexcept {
    const size_t idx = static_cast<size_t>(stage);
    static const StageMetrics empty{};
    return (idx < STAGE_COUNT) ? metrics_[idx] : empty;
}

double SimulationHardwareProfiler::stage_last_us(SimulationStage stage) const noexcept {
    return stage_metrics(stage).last_time_us;
}

double SimulationHardwareProfiler::stage_avg_us(SimulationStage stage) const noexcept {
    return stage_metrics(stage).average_time_us();
}

double SimulationHardwareProfiler::total_step_time_us() const noexcept {
    return stage_metrics(SimulationStage::Total_Step).last_time_us;
}

std::string_view SimulationHardwareProfiler::stage_name(SimulationStage stage) noexcept {
    switch (stage) {
        case SimulationStage::Circuit_MatrixAssembly: return "Circuit: Matrix Assembly";
        case SimulationStage::Circuit_Factorization: return "Circuit: Factorization";
        case SimulationStage::Circuit_TriangularSolve: return "Circuit: Triangular Solve";
        case SimulationStage::Circuit_ConvergenceCheck: return "Circuit: Convergence Check";
        case SimulationStage::Physics_BroadPhase: return "Physics: Broad-Phase Collision";
        case SimulationStage::Physics_NarrowPhase: return "Physics: Narrow-Phase GJK/EPA";
        case SimulationStage::Physics_ConstraintSolver: return "Physics: Constraint PGS Solve";
        case SimulationStage::Physics_PositionIntegration: return "Physics: Symplectic Position Integration";
        case SimulationStage::Thermal_LaplacianDiffusion: return "Thermal: Laplacian Diffusion";
        case SimulationStage::Thermal_BoundaryConditions: return "Thermal: Boundary Conditions";
        case SimulationStage::Total_Step: return "Overall Simulation Step";
        default: return "Unknown Stage";
    }
}

std::string SimulationHardwareProfiler::format_profile_summary() const {
    std::ostringstream oss;
    oss << "=== Hardware Simulation Profile Summary ===\n";
    oss << std::left << std::setw(38) << "Stage"
        << std::right << std::setw(8) << "Calls"
        << std::setw(12) << "Avg (us)"
        << std::setw(12) << "Min (us)"
        << std::setw(12) << "Max (us)"
        << std::setw(14) << "Total (us)"
        << "\n";
    oss << std::string(96, '-') << "\n";

    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        const auto& m = metrics_[i];
        if (m.call_count == 0) continue;

        const auto stage = static_cast<SimulationStage>(i);
        oss << std::left << std::setw(38) << stage_name(stage)
            << std::right << std::setw(8) << m.call_count
            << std::fixed << std::setprecision(2)
            << std::setw(12) << m.average_time_us()
            << std::setw(12) << m.min_time_us
            << std::setw(12) << m.max_time_us
            << std::setw(14) << m.total_time_us
            << "\n";
    }
    return oss.str();
}

void SimulationHardwareProfiler::reset() noexcept {
    metrics_.fill(StageMetrics{});
    active_probes_.fill(ActiveProbe{});
}

} // namespace shunya::perf
