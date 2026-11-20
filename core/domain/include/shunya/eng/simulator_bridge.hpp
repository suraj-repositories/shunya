#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/physics_runtime.hpp"

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace shunya::eng {

enum class PreFlightSeverity {
    Info,
    Warning,
    Error
};

struct PreFlightIssue {
    std::string rule_id;       // e.g. "PF-01-MAT", "PF-02-SEAL", "PF-03-TERM", "PF-04-DEGEN", "PF-05-MANIFOLD"
    PreFlightSeverity severity{PreFlightSeverity::Error};
    std::string message;
    std::string target_type;   // "region", "cavity", "terminal", "mesh", "object"
    std::string target_id;     // id or name to jump to
    std::string target_name;
    bool is_blocking{true};
};

struct PreFlightReport {
    bool passed{true};
    std::string object_id;
    std::string object_name;
    std::vector<PreFlightIssue> issues;

    [[nodiscard]] bool has_errors() const noexcept {
        for (const auto& issue : issues) {
            if (issue.severity == PreFlightSeverity::Error) return true;
        }
        return false;
    }

    [[nodiscard]] bool has_warnings() const noexcept {
        for (const auto& issue : issues) {
            if (issue.severity == PreFlightSeverity::Warning) return true;
        }
        return false;
    }

    [[nodiscard]] std::string to_markdown() const;
    [[nodiscard]] std::string to_json() const;
};

struct LumpedThermalNode {
    std::string node_id;
    std::string name;
    double heat_capacity_j_k{1.0};
    double initial_temp_k{298.15};
    double ambient_conductance_w_k{0.5};
};

struct ElectricalBranchModel {
    std::string branch_id;
    std::string name;
    std::string terminal_a;
    std::string terminal_b;
    double resistance_ohm{1e-3};
    double inductance_h{1e-6};
    double max_current_a{10.0};
};

struct SimulationReadyModel {
    core::base::Uuid object_id;
    std::string object_name;
    double total_mass_kg{0.0};
    double volume_m3{0.0};
    core::base::math::Vector3<double> center_of_mass_m{0.0, 0.0, 0.0};
    core::base::math::Matrix4x4<double> inertia_tensor{};
    std::vector<LumpedThermalNode> thermal_nodes;
    std::vector<ElectricalBranchModel> electrical_branches;
    SimulatedObjectBody physics_body;
    std::string serialized_scene_json;
};

enum class SimulationQuickRunPreset {
    Step_Voltage_120V,
    Drop_Test_1m,
    Thermal_Shock_1200K,
    Tensile_Pull_1kN
};

enum class SyncStatus {
    In_Sync_Clean,
    Modified_Out_Of_Sync,
    Simulation_Running
};

struct TelemetrySnapshot {
    double time_s{0.0};
    double current_a{0.0};
    double voltage_v{0.0};
    double temperature_k{298.15};
    double deformation_m{0.0};
    double kinetic_energy_j{0.0};
    double potential_energy_j{0.0};
    core::base::math::Vector3<double> position{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> velocity{0.0, 0.0, 0.0};
};

/**
 * @brief Thread-safe ring buffer for 60 Hz live telemetry streaming
 */
template <typename T, size_t Capacity = 2048>
class TelemetryRingBuffer {
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.size() >= Capacity) {
            buffer_.pop_front();
        }
        buffer_.push_back(item);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.clear();
    }

    [[nodiscard]] std::vector<T> snapshot(size_t max_items = Capacity) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> result;
        size_t start = (buffer_.size() > max_items) ? (buffer_.size() - max_items) : 0;
        result.reserve(buffer_.size() - start);
        for (size_t i = start; i < buffer_.size(); ++i) {
            result.push_back(buffer_[i]);
        }
        return result;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::deque<T> buffer_;
};

/**
 * @brief Coordinator translating PhysicalObject instances into calibrated simulation models,
 *        enforcing pre-flight checks, streaming live telemetry at 60Hz, and managing round-trip re-sync.
 */
class SimulatorBridgeCoordinator {
public:
    SimulatorBridgeCoordinator() = default;

    // PH5-27.01.002: Pre-Flight Validation Engine
    [[nodiscard]] static PreFlightReport validate_object(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& mat_reg);

    // PH5-27.01.004: Lumped Model & Mass/Inertia Generation
    [[nodiscard]] static core::base::Result<SimulationReadyModel> generate_simulation_model(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& mat_reg);

    // PH5-27.02.001 & 002: Quick-Run In-Situ Simulation
    void start_quick_run(
        SimulationQuickRunPreset preset,
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& mat_reg,
        double duration_s = 2.0,
        double dt_s = 0.001);

    void step_simulation(double dt_s);
    void run_to_completion(double dt_s = 0.001);
    void stop_simulation();

    // PH5-27.02.003: 60Hz Telemetry Streaming
    [[nodiscard]] std::vector<TelemetrySnapshot> sample_telemetry(size_t max_count = 100) const;
    [[nodiscard]] TelemetrySnapshot latest_telemetry() const;

    // PH5-27.02.004 & 005 & 006: Round-Trip Synchronization
    void mark_object_modified(const core::base::Uuid& obj_id);
    bool resync_active_simulation(const PhysicalObject& obj, const core::domain::MaterialRegistry& mat_reg);
    [[nodiscard]] SyncStatus sync_status() const noexcept { return sync_status_; }
    void mark_in_sync() noexcept { sync_status_ = SyncStatus::In_Sync_Clean; }

    // Active state inspection
    [[nodiscard]] bool is_running() const noexcept { return is_running_; }
    [[nodiscard]] double current_time_s() const noexcept { return current_time_s_; }
    [[nodiscard]] const SimulationReadyModel* active_model() const noexcept {
        return active_model_ ? active_model_.get() : nullptr;
    }
    [[nodiscard]] double active_resistance_ohm() const noexcept { return active_resistance_ohm_; }

private:
    SyncStatus sync_status_{SyncStatus::In_Sync_Clean};
    bool is_running_{false};
    double current_time_s_{0.0};
    double duration_s_{2.0};
    SimulationQuickRunPreset current_preset_{SimulationQuickRunPreset::Step_Voltage_120V};
    std::unique_ptr<SimulationReadyModel> active_model_;
    SimulatedObjectBody active_sim_body_;
    TelemetryRingBuffer<TelemetrySnapshot, 2048> telemetry_buffer_;
    TelemetrySnapshot latest_snapshot_{};
    double active_resistance_ohm_{240.0};
    double target_voltage_v_{120.0};
    double applied_force_n_{0.0};
    double current_temp_k_{298.15};
    double drop_y_{1.0};
    double drop_vy_{0.0};
    double tensile_def_m_{0.0};
};

} // namespace shunya::eng
