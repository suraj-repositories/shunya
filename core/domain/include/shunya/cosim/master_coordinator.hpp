#pragma once

#include "shunya/cirsim/transient_simulation_engine.hpp"
#include "shunya/physim/simulation_controller.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace shunya::cosim {

// =============================================================================
// Lock-Free Triple-Buffer Telemetry Exchange (PH4-10.01.004)
// =============================================================================
template <typename T, size_t BufferSlots = 4>
class LockFreeTelemetryBuffer {
public:
    LockFreeTelemetryBuffer() : sequence_(0), last_read_seq_(0) {}

    void write(const T& data) noexcept {
        const uint64_t seq = sequence_.load(std::memory_order_relaxed);
        sequence_.store(seq + 1, std::memory_order_release); // odd: write in progress
        buffers_[(seq >> 1) % BufferSlots] = data;
        sequence_.store(seq + 2, std::memory_order_release); // even: write committed
    }

    [[nodiscard]] bool read(T& out_data) noexcept {
        for (;;) {
            const uint64_t seq_before = sequence_.load(std::memory_order_acquire);
            if (seq_before & 1) {
                std::this_thread::yield();
                continue;
            }
            if (seq_before == last_read_seq_ || seq_before == 0) {
                out_data = last_data_;
                return false;
            }
            const size_t slot = static_cast<size_t>(((seq_before - 2) >> 1) % BufferSlots);
            const T candidate = buffers_[slot];
            if (sequence_.load(std::memory_order_acquire) == seq_before) {
                last_read_seq_ = seq_before;
                last_data_ = candidate;
                out_data = candidate;
                return true;
            }
            std::this_thread::yield();
        }
    }

    [[nodiscard]] T get_latest() noexcept {
        T data;
        static_cast<void>(read(data));
        return last_data_;
    }

    [[nodiscard]] const T& peek_latest() const noexcept {
        return last_data_;
    }

private:
    std::array<T, BufferSlots> buffers_{};
    std::atomic<uint64_t> sequence_{0};
    uint64_t last_read_seq_{0};
    T last_data_{};
};

struct CoSimTelemetryPacket {
    double timestamp_s{0.0};
    double motor_armature_current_a{0.0};
    double motor_back_emf_v{0.0};
    double motor_mechanical_torque_nm{0.0};
    double shaft_angular_velocity_rad_s{0.0};
    double component_temperature_k{298.15};
    bool limit_switch_closed{false};
};

// =============================================================================
// Co-Simulation Checkpoint & State Rollback (PH4-10.01.003)
// =============================================================================
struct CircuitCheckpoint {
    double time_s{0.0};
    double motor_current_a{0.0};
    double motor_voltage_v{0.0};
    double battery_soc{1.0};
    double temperature_k{298.15};
};

struct CoSimCheckpoint {
    double time_s{0.0};
    double macro_dt_s{1e-3};
    CircuitCheckpoint circuit_state;
    physim::SimulationSnapshot physics_state;
    CoSimTelemetryPacket telemetry;
};

struct CoSimConfig {
    double macro_time_step_s{1e-3};      ///< Physics macro step (e.g. 1 ms)
    double micro_time_step_s{1e-5};      ///< Circuit micro step (e.g. 10 us)
    double min_macro_step_s{1e-5};       ///< Lower bound on negotiated macro step
    double max_macro_step_s{1e-2};       ///< Upper bound on macro step
    double collision_impulse_rollback_threshold{100.0}; ///< Impulse threshold triggering rollback
    int max_sub_cycle_ratio{1000};       ///< Safety limit on micro/macro steps per tick
};

struct CoSimStepResult {
    bool success{true};
    double advanced_time_s{0.0};
    size_t circuit_micro_steps{0};
    bool rollback_triggered{false};
    std::string diagnostics;
};

/**
 * @brief Multi-Rate Master Co-Simulation Orchestrator (PH4-10.01.001 - PH4-10.01.004).
 *        Synchronizes high-frequency circuit transient solvers with multi-body 3D physics.
 */
class MasterCoSimulationCoordinator {
public:
    explicit MasterCoSimulationCoordinator(CoSimConfig config = {})
        : config_(config), current_macro_dt_(config.macro_time_step_s) {}

    // Solver Access
    [[nodiscard]] cirsim::TransientSimulationEngine& circuit_engine() noexcept { return circuit_engine_; }
    [[nodiscard]] const cirsim::TransientSimulationEngine& circuit_engine() const noexcept { return circuit_engine_; }

    [[nodiscard]] physim::SimulationController& physics_controller() noexcept { return physics_controller_; }
    [[nodiscard]] const physim::SimulationController& physics_controller() const noexcept { return physics_controller_; }

    // Multi-Rate Time Orchestration (PH4-10.01.001, PH4-10.01.002)
    [[nodiscard]] CoSimStepResult step(double duration_s = 0.0);

    // Rollback & Negotiation (PH4-10.01.003)
    void capture_checkpoint();
    bool rollback();
    void negotiate_step(bool reduce_step);

    // Telemetry Buffer (PH4-10.01.004)
    [[nodiscard]] LockFreeTelemetryBuffer<CoSimTelemetryPacket>& telemetry_pipe() noexcept { return telemetry_pipe_; }
    [[nodiscard]] const LockFreeTelemetryBuffer<CoSimTelemetryPacket>& telemetry_pipe() const noexcept { return telemetry_pipe_; }

    [[nodiscard]] double current_time_s() const noexcept { return current_time_s_; }
    [[nodiscard]] double current_macro_dt_s() const noexcept { return current_macro_dt_; }
    [[nodiscard]] size_t total_rollbacks() const noexcept { return rollback_count_; }
    [[nodiscard]] size_t sub_cycle_ratio() const noexcept;

    void set_config(const CoSimConfig& cfg) noexcept { config_ = cfg; }
    [[nodiscard]] const CoSimConfig& config() const noexcept { return config_; }

private:
    CoSimConfig config_;
    double current_time_s_{0.0};
    double current_macro_dt_{1e-3};
    size_t rollback_count_{0};

    cirsim::TransientSimulationEngine circuit_engine_;
    physim::SimulationController physics_controller_;
    LockFreeTelemetryBuffer<CoSimTelemetryPacket> telemetry_pipe_;
    CoSimCheckpoint last_checkpoint_;
    bool has_checkpoint_{false};
};

} // namespace shunya::cosim
