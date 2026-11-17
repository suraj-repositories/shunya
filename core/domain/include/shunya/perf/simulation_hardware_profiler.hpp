#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::perf {

enum class SimulationStage : uint8_t {
    // Circuit Simulation Stages
    Circuit_MatrixAssembly = 0,
    Circuit_Factorization,
    Circuit_TriangularSolve,
    Circuit_ConvergenceCheck,

    // 3D Physics Simulation Stages
    Physics_BroadPhase,
    Physics_NarrowPhase,
    Physics_ConstraintSolver,
    Physics_PositionIntegration,

    // Coupled Multi-Physics Stages
    Thermal_LaplacianDiffusion,
    Thermal_BoundaryConditions,

    // Overall Simulation Step
    Total_Step,

    _StageCount
};

struct StageMetrics {
    uint64_t call_count{0};
    double total_time_us{0.0};
    double min_time_us{1e9};
    double max_time_us{0.0};
    double last_time_us{0.0};
    double ema_time_us{0.0};
    uint64_t total_cpu_cycles{0};

    [[nodiscard]] double average_time_us() const noexcept {
        return call_count > 0 ? (total_time_us / static_cast<double>(call_count)) : 0.0;
    }
};

/**
 * @brief High-precision hardware performance profiler with microsecond resolution
 *        and hardware TSC CPU cycle measurement for simulation loops.
 */
class SimulationHardwareProfiler {
public:
    SimulationHardwareProfiler() = default;

    static uint64_t read_cpu_cycles() noexcept;

    void start_stage(SimulationStage stage) noexcept;
    void stop_stage(SimulationStage stage) noexcept;

    [[nodiscard]] const StageMetrics& stage_metrics(SimulationStage stage) const noexcept;
    [[nodiscard]] double stage_last_us(SimulationStage stage) const noexcept;
    [[nodiscard]] double stage_avg_us(SimulationStage stage) const noexcept;

    [[nodiscard]] double total_step_time_us() const noexcept;

    static std::string_view stage_name(SimulationStage stage) noexcept;

    [[nodiscard]] std::string format_profile_summary() const;

    void reset() noexcept;

private:
    struct ActiveProbe {
        std::chrono::high_resolution_clock::time_point start_time;
        uint64_t start_cycles{0};
        bool active{false};
    };

    static constexpr size_t STAGE_COUNT = static_cast<size_t>(SimulationStage::_StageCount);
    std::array<StageMetrics, STAGE_COUNT> metrics_{};
    std::array<ActiveProbe, STAGE_COUNT> active_probes_{};
};

/**
 * @brief RAII Scoped hardware timer for microsecond simulation loop measurement.
 */
class ScopedHardwareStageTimer {
public:
    ScopedHardwareStageTimer(SimulationHardwareProfiler& profiler, SimulationStage stage) noexcept
        : profiler_(profiler), stage_(stage) {
        profiler_.start_stage(stage_);
    }

    ~ScopedHardwareStageTimer() noexcept {
        profiler_.stop_stage(stage_);
    }

    ScopedHardwareStageTimer(const ScopedHardwareStageTimer&) = delete;
    ScopedHardwareStageTimer& operator=(const ScopedHardwareStageTimer&) = delete;

private:
    SimulationHardwareProfiler& profiler_;
    SimulationStage stage_;
};

} // namespace shunya::perf
