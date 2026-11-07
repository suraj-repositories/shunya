#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"
#include "shunya/core/domain/component_runtime.hpp"
#include "shunya/core/domain/simulation_clock.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// SIM-01: Simulation Lifecycle States
// =============================================================================

enum class SimulationState {
    Stopped,
    Configuring,
    Running,
    Paused,
    Stepping,
    Completed,
    Failed,
    Cancelled
};

[[nodiscard]] constexpr std::string_view simulation_state_to_string(SimulationState state) noexcept {
    switch (state) {
        case SimulationState::Stopped: return "Stopped";
        case SimulationState::Configuring: return "Configuring";
        case SimulationState::Running: return "Running";
        case SimulationState::Paused: return "Paused";
        case SimulationState::Stepping: return "Stepping";
        case SimulationState::Completed: return "Completed";
        case SimulationState::Failed: return "Failed";
        case SimulationState::Cancelled: return "Cancelled";
    }
    return "Stopped";
}

[[nodiscard]] std::optional<SimulationState> simulation_state_from_string(std::string_view str) noexcept;

[[nodiscard]] constexpr bool is_simulation_active(SimulationState state) noexcept {
    return state == SimulationState::Running ||
           state == SimulationState::Paused ||
           state == SimulationState::Stepping;
}

[[nodiscard]] constexpr bool is_simulation_terminal(SimulationState state) noexcept {
    return state == SimulationState::Completed ||
           state == SimulationState::Failed ||
           state == SimulationState::Cancelled ||
           state == SimulationState::Stopped;
}

[[nodiscard]] bool can_transition(SimulationState from, SimulationState to) noexcept;

// =============================================================================
// SIM-02: Simulation Configuration
// =============================================================================

enum class TimeStepPolicy {
    Fixed,
    Adaptive
};

[[nodiscard]] constexpr std::string_view time_step_policy_to_string(TimeStepPolicy policy) noexcept {
    switch (policy) {
        case TimeStepPolicy::Fixed: return "Fixed";
        case TimeStepPolicy::Adaptive: return "Adaptive";
    }
    return "Fixed";
}

[[nodiscard]] std::optional<TimeStepPolicy> time_step_policy_from_string(std::string_view str) noexcept;

enum class SolverType {
    EulerExplicit,
    RungeKutta2,
    RungeKutta4,
    AdaptiveRK45,
    Verlet,
    ImplicitEuler,
    Custom
};

[[nodiscard]] constexpr std::string_view solver_type_to_string(SolverType solver) noexcept {
    switch (solver) {
        case SolverType::EulerExplicit: return "EulerExplicit";
        case SolverType::RungeKutta2: return "RungeKutta2";
        case SolverType::RungeKutta4: return "RungeKutta4";
        case SolverType::AdaptiveRK45: return "AdaptiveRK45";
        case SolverType::Verlet: return "Verlet";
        case SolverType::ImplicitEuler: return "ImplicitEuler";
        case SolverType::Custom: return "Custom";
    }
    return "RungeKutta4";
}

[[nodiscard]] std::optional<SolverType> solver_type_from_string(std::string_view str) noexcept;

struct SimulationConfig {
    double start_time{0.0};
    double end_time{10.0};
    TimeStepPolicy time_step_policy{TimeStepPolicy::Fixed};
    double initial_time_step{0.01};
    double min_time_step{1e-6};
    double max_time_step{0.1};
    SolverType solver_type{SolverType::RungeKutta4};
    double relative_tolerance{1e-4};
    double absolute_tolerance{1e-6};
    uint64_t random_seed{42};
    double output_sampling_interval{0.01};
    uint64_t max_steps{1'000'000};

    [[nodiscard]] base::ValidationReport validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<SimulationConfig> from_json(std::string_view json_str);
};

// =============================================================================
// SIM-04: Domain Engine Coordination Protocol
// =============================================================================

class ISimulationParticipant {
public:
    virtual ~ISimulationParticipant() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual int execution_order() const { return 0; }

    struct StepResult {
        bool success{true};
        bool step_accepted{true};
        double suggested_dt{0.0};
        double estimated_error{0.0};
        std::string message;
    };

    virtual base::Result<void> initialize(const SimulationConfig& config) {
        (void)config;
        return {};
    }

    virtual base::Result<void> pre_step(double t, double dt) {
        (void)t;
        (void)dt;
        return {};
    }

    [[nodiscard]] virtual base::Result<StepResult> step(double t, double dt) = 0;

    virtual base::Result<void> post_step(double t, double dt) {
        (void)t;
        (void)dt;
        return {};
    }

    virtual base::Result<void> reset() {
        return {};
    }

    virtual base::Result<void> terminate() {
        return {};
    }

    [[nodiscard]] virtual std::string serialize_state() const {
        return "{}";
    }

    virtual base::Result<void> restore_state(std::string_view state_json) {
        (void)state_json;
        return {};
    }
};

// =============================================================================
// SIM-05: Time-Step Policy & Rejection Diagnostics
// =============================================================================

struct TimeStepDiagnostics {
    uint64_t total_steps_attempted{0};
    uint64_t steps_accepted{0};
    uint64_t steps_rejected{0};
    double current_dt{0.0};
    double min_dt_encountered{std::numeric_limits<double>::infinity()};
    double max_dt_encountered{0.0};
    std::string last_rejection_reason;

    [[nodiscard]] double acceptance_ratio() const noexcept {
        if (total_steps_attempted == 0) return 1.0;
        return static_cast<double>(steps_accepted) / static_cast<double>(total_steps_attempted);
    }

    void reset() noexcept {
        total_steps_attempted = 0;
        steps_accepted = 0;
        steps_rejected = 0;
        current_dt = 0.0;
        min_dt_encountered = std::numeric_limits<double>::infinity();
        max_dt_encountered = 0.0;
        last_rejection_reason.clear();
    }
};

class TimeStepController {
public:
    TimeStepController() = default;
    explicit TimeStepController(const SimulationConfig& config);

    void configure(const SimulationConfig& config);

    [[nodiscard]] double compute_next_dt(double current_dt, double suggested_dt, double local_error) noexcept;
    [[nodiscard]] base::Result<void> handle_rejected_step(double& dt, std::string_view reason);
    void record_accepted_step(double dt) noexcept;

    [[nodiscard]] const TimeStepDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    void reset() noexcept;

private:
    SimulationConfig config_{};
    TimeStepDiagnostics diagnostics_{};
};

// =============================================================================
// SIM-06: Checkpoint / Restart
// =============================================================================

struct SimulationCheckpoint {
    base::Uuid checkpoint_id{};
    double physical_time{0.0};
    uint64_t step_count{0};
    std::string wall_timestamp;
    std::unordered_map<std::string, std::string> participant_states;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<SimulationCheckpoint> from_json(std::string_view json_str);
};

// =============================================================================
// SIM-07: Discrete Event Scheduler
// =============================================================================

struct SimulationEvent {
    base::Uuid event_id{base::Uuid::generate_random()};
    std::string name;
    double scheduled_time{0.0};
    int priority{0}; // Higher priority dispatched first at identical time
    bool is_recurring{false};
    double recurrence_interval{0.0};
    std::function<base::Result<void>(double event_time)> handler;
};

class SimulationEventQueue {
public:
    SimulationEventQueue() = default;

    void schedule_event(SimulationEvent event);
    void post_user_event(std::string name, std::function<base::Result<void>(double)> handler, double current_time);

    [[nodiscard]] std::optional<double> next_event_time() const noexcept;
    [[nodiscard]] bool has_pending_events_up_to(double t) const noexcept;
    [[nodiscard]] base::Result<size_t> dispatch_events_up_to(double t);
    [[nodiscard]] double calculate_substep_for_events(double current_t, double target_dt) const noexcept;

    [[nodiscard]] size_t pending_event_count() const noexcept { return events_.size(); }
    void clear() noexcept { events_.clear(); }

private:
    std::vector<SimulationEvent> events_;
    void sort_events();
};

// =============================================================================
// SIM-08: Run Isolation
// =============================================================================

class IsolatedRunEnvironment {
public:
    IsolatedRunEnvironment() = default;

    [[nodiscard]] static base::Result<std::unique_ptr<IsolatedRunEnvironment>> create(
        const Project& project,
        const std::vector<std::shared_ptr<ComponentRuntimeInstance>>& components = {}
    );

    [[nodiscard]] const base::Uuid& project_id() const noexcept { return project_id_; }
    [[nodiscard]] const std::string& project_name() const noexcept { return project_name_; }
    [[nodiscard]] const base::Version& project_version() const noexcept { return project_version_; }
    [[nodiscard]] const std::string& project_json_snapshot() const noexcept { return project_json_snapshot_; }
    [[nodiscard]] const std::string& content_hash() const noexcept { return content_hash_; }
    [[nodiscard]] const std::vector<std::shared_ptr<ComponentRuntimeInstance>>& components() const noexcept {
        return isolated_components_;
    }

private:
    base::Uuid project_id_{};
    std::string project_name_;
    base::Version project_version_{1, 0, 0, "final"};
    std::string project_json_snapshot_;
    std::string content_hash_;
    std::vector<std::shared_ptr<ComponentRuntimeInstance>> isolated_components_;
};

// =============================================================================
// SIM-09: Run Manifest
// =============================================================================

struct RunManifest {
    base::Uuid run_id{base::Uuid::generate_random()};
    base::Uuid project_id{};
    std::string project_content_hash;
    std::string solver_name;
    uint64_t seed{0};
    SimulationConfig config{};
    std::string start_time_iso;
    std::string completion_time_iso;
    SimulationState final_state{SimulationState::Stopped};
    TimeStepDiagnostics diagnostics{};
    std::string host_os;
    std::string compiler_info;
    base::Version engine_version{1, 0, 0, "final"};

    [[nodiscard]] std::string compute_manifest_hash() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<RunManifest> from_json(std::string_view json_str);
};

// =============================================================================
// SIM-10: Cancellation Token
// =============================================================================

class CancellationToken {
public:
    CancellationToken() = default;

    void request_cancellation() noexcept {
        cancelled_.store(true, std::memory_order_relaxed);
    }

    [[nodiscard]] bool is_cancelled() const noexcept {
        return cancelled_.load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        cancelled_.store(false, std::memory_order_relaxed);
    }

private:
    std::atomic<bool> cancelled_{false};
};

// =============================================================================
// Orchestrator: SimulationRuntime
// =============================================================================

class SimulationRuntime {
public:
    SimulationRuntime();
    explicit SimulationRuntime(SimulationConfig config);
    ~SimulationRuntime();

    base::Result<void> configure(const SimulationConfig& config);

    base::Result<void> add_participant(std::shared_ptr<ISimulationParticipant> participant);
    void remove_participant(const std::string& name);
    [[nodiscard]] std::shared_ptr<ISimulationParticipant> find_participant(const std::string& name) const;
    [[nodiscard]] const std::vector<std::shared_ptr<ISimulationParticipant>>& participants() const noexcept {
        return participants_;
    }

    base::Result<void> isolate_project(
        const Project& project,
        const std::vector<std::shared_ptr<ComponentRuntimeInstance>>& components = {}
    );
    [[nodiscard]] const IsolatedRunEnvironment* isolated_environment() const noexcept {
        return isolated_env_.get();
    }

    // Lifecycle Controls (SIM-06)
    base::Result<void> start();
    base::Result<void> pause();
    base::Result<void> resume();
    base::Result<void> step(std::optional<double> override_dt = std::nullopt);
    base::Result<void> run_until(double target_time);
    base::Result<void> run_all();
    base::Result<void> stop();
    base::Result<void> reset();

    // Cancellation (SIM-10)
    void cancel() noexcept;

    // Checkpoints (SIM-06)
    [[nodiscard]] base::Result<SimulationCheckpoint> create_checkpoint();
    base::Result<void> restore_checkpoint(const SimulationCheckpoint& checkpoint);
    [[nodiscard]] const std::vector<SimulationCheckpoint>& checkpoints() const noexcept {
        return checkpoints_;
    }

    // Event Queue (SIM-07)
    [[nodiscard]] SimulationEventQueue& event_queue() noexcept { return event_queue_; }
    [[nodiscard]] const SimulationEventQueue& event_queue() const noexcept { return event_queue_; }

    // State Queries
    [[nodiscard]] SimulationState state() const noexcept { return state_; }
    [[nodiscard]] const SimulationConfig& config() const noexcept { return config_; }
    [[nodiscard]] const SimulationClock& clock() const noexcept { return clock_; }
    [[nodiscard]] SimulationClock& clock() noexcept { return clock_; }
    [[nodiscard]] const TimeStepDiagnostics& diagnostics() const noexcept {
        return step_controller_.diagnostics();
    }

    // Telemetry & Progress (SIM-10)
    [[nodiscard]] double progress_ratio() const noexcept;
    [[nodiscard]] double estimated_remaining_wall_time_seconds() const noexcept;
    [[nodiscard]] double stepping_throughput() const noexcept;
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

    // Manifest (SIM-09)
    [[nodiscard]] RunManifest generate_run_manifest() const;

    using StateChangeCallback = std::function<void(SimulationState old_state, SimulationState new_state)>;
    void set_state_change_callback(StateChangeCallback cb) { state_callback_ = std::move(cb); }

private:
    base::Result<void> transition_to(SimulationState target_state);
    void sort_participants();

    SimulationState state_{SimulationState::Stopped};
    SimulationConfig config_{};
    SimulationClock clock_{};
    TimeStepController step_controller_{};
    SimulationEventQueue event_queue_{};
    CancellationToken cancellation_token_{};
    std::unique_ptr<IsolatedRunEnvironment> isolated_env_{nullptr};
    std::vector<std::shared_ptr<ISimulationParticipant>> participants_{};
    std::vector<SimulationCheckpoint> checkpoints_{};
    std::string last_error_{};
    std::string run_start_iso_{};
    std::string run_completion_iso_{};
    StateChangeCallback state_callback_{nullptr};
};

} // namespace shunya::core::domain
