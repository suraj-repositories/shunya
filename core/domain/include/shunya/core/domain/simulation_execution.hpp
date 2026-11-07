#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/domain/simulation_runtime.hpp"
#include "shunya/core/domain/simulation_clock.hpp"

#include <chrono>
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
// RUN-01: Interactive Visual Preview Mode
// =============================================================================

enum class ExecutionMode {
    InteractivePreview,
    RealtimeSynced,
    ScientificAuthoritative,
    Accelerated,
    HeadlessBatch
};

[[nodiscard]] constexpr std::string_view execution_mode_to_string(ExecutionMode mode) noexcept {
    switch (mode) {
        case ExecutionMode::InteractivePreview: return "InteractivePreview";
        case ExecutionMode::RealtimeSynced: return "RealtimeSynced";
        case ExecutionMode::ScientificAuthoritative: return "ScientificAuthoritative";
        case ExecutionMode::Accelerated: return "Accelerated";
        case ExecutionMode::HeadlessBatch: return "HeadlessBatch";
    }
    return "InteractivePreview";
}

[[nodiscard]] std::optional<ExecutionMode> execution_mode_from_string(std::string_view str) noexcept;

struct ExecutionModeDescriptor {
    ExecutionMode mode;
    bool is_authoritative;
    bool is_approximate;
    bool has_visual_only_warning;
    std::string display_label;
    std::string description;

    [[nodiscard]] static ExecutionModeDescriptor describe(ExecutionMode mode) noexcept;
};

// =============================================================================
// RUN-02: Realtime Wall-Clock Synced Mode
// =============================================================================

enum class TimingStatus {
    OnTime,
    Lagging,
    CatchingUp,
    Ahead
};

[[nodiscard]] constexpr std::string_view timing_status_to_string(TimingStatus status) noexcept {
    switch (status) {
        case TimingStatus::OnTime: return "OnTime";
        case TimingStatus::Lagging: return "Lagging";
        case TimingStatus::CatchingUp: return "CatchingUp";
        case TimingStatus::Ahead: return "Ahead";
    }
    return "OnTime";
}

struct RealtimePacingConfig {
    double target_rtf = 1.0; // 1.0 = 1 sec sim time per 1 sec wall time
    std::chrono::microseconds target_frame_interval{16667}; // ~60 Hz
    bool catch_up_allowed = true;
    uint32_t max_catch_up_steps_per_frame = 10;
};

struct RealtimeDiagnostics {
    double current_rtf = 0.0;
    double sync_jitter_seconds = 0.0;
    TimingStatus status = TimingStatus::OnTime;
    uint64_t frames_rendered = 0;
    uint64_t steps_executed = 0;
    double total_wall_seconds = 0.0;
    double total_sim_seconds = 0.0;
};

class RealtimePacer {
public:
    explicit RealtimePacer(RealtimePacingConfig config = {});

    void start();
    void update(double current_sim_time, double wall_elapsed_seconds);
    void pause();
    void resume();
    void record_step(double dt);

    [[nodiscard]] const RealtimePacingConfig& config() const noexcept { return config_; }
    [[nodiscard]] const RealtimeDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] bool is_paused() const noexcept { return is_paused_; }

private:
    RealtimePacingConfig config_;
    RealtimeDiagnostics diagnostics_;
    bool is_paused_{false};
    double prev_sim_time_{0.0};
    double prev_wall_time_{0.0};
};

// =============================================================================
// RUN-03: Scientific Authoritative Mode
// =============================================================================

struct ScientificRunConfig {
    SimulationConfig base_config;
    uint64_t deterministic_seed{42};
    double relative_tolerance{1e-6};
    double absolute_tolerance{1e-9};
    bool require_strict_conservation{true};
    double max_energy_drift_fraction{1e-4};
};

struct ScientificVerificationResult {
    bool passed{false};
    double max_state_difference{0.0};
    double max_energy_drift{0.0};
    std::string diagnostic_summary;
};

class ScientificRunVerifier {
public:
    [[nodiscard]] static ScientificVerificationResult verify_reproducibility(
        const std::vector<double>& run1_final_state,
        const std::vector<double>& run2_final_state,
        double tolerance = 1e-12);

    [[nodiscard]] static ScientificVerificationResult verify_energy_conservation(
        const std::vector<double>& energy_history,
        double max_drift_fraction = 1e-4);
};

// =============================================================================
// RUN-04: Accelerated Mode
// =============================================================================

struct AcceleratedRunMetrics {
    uint64_t steps_executed{0};
    double elapsed_sim_time{0.0};
    double elapsed_compute_time{0.0};
    double steps_per_second{0.0};
    double achieved_rtf{0.0};
};

class AcceleratedRunController {
public:
    void record_batch(uint64_t step_count, double sim_dt, double compute_wall_seconds);
    void reset();

    [[nodiscard]] const AcceleratedRunMetrics& metrics() const noexcept { return metrics_; }

private:
    AcceleratedRunMetrics metrics_;
};

// =============================================================================
// RUN-05: Slow-Motion and Time Dilation Display Controls
// =============================================================================

class TimeDilationController {
public:
    explicit TimeDilationController(double initial_dilation = 1.0);

    base::Result<void> set_dilation_factor(double factor);
    [[nodiscard]] double dilation_factor() const noexcept { return dilation_factor_; }

    /// Calculates visual presentation duration corresponding to physical time step
    /// Note: Does not change physical delta time or solver state.
    [[nodiscard]] double physical_to_presentation_dt(double physical_dt) const noexcept;
    [[nodiscard]] double presentation_to_physical_dt(double presentation_dt) const noexcept;

private:
    double dilation_factor_{1.0};
};

// =============================================================================
// RUN-06: Headless Batch Mode for Sweeps, CI, and Remote Compute
// =============================================================================

struct SweepParameter {
    std::string name;
    std::vector<double> values;
};

enum class BatchFailurePolicy {
    ContinueOnFailure,
    AbortBatch
};

struct BatchExecutionConfig {
    std::string batch_name{"SweepBatch"};
    std::vector<SweepParameter> parameters;
    BatchFailurePolicy failure_policy{BatchFailurePolicy::ContinueOnFailure};
    bool headless{true};
    std::string output_directory{"results/batch"};
};

struct SingleRunResult {
    std::string run_id;
    std::unordered_map<std::string, double> parameter_values;
    bool success{false};
    double sim_time_reached{0.0};
    std::string error_message;
};

struct BatchExecutionSummary {
    std::string batch_name;
    uint32_t total_runs{0};
    uint32_t successful_runs{0};
    uint32_t failed_runs{0};
    std::vector<SingleRunResult> run_results;

    [[nodiscard]] std::string to_json() const;
};

class BatchExecutionRunner {
public:
    explicit BatchExecutionRunner(BatchExecutionConfig config);

    [[nodiscard]] const BatchExecutionConfig& config() const noexcept { return config_; }

    /// Generates combinations of sweep parameter values (Cartesian product)
    [[nodiscard]] std::vector<std::unordered_map<std::string, double>> generate_parameter_combinations() const;

    /// Runs a batch given an executor function: (params) -> Result<SingleRunResult>
    [[nodiscard]] BatchExecutionSummary execute(
        const std::function<SingleRunResult(const std::unordered_map<std::string, double>&)>& run_func);

private:
    BatchExecutionConfig config_;
};

// =============================================================================
// RUN-07: Checkpoint/Restart and Deterministic Replay
// =============================================================================

struct SimulationInputEvent {
    double timestamp{0.0};
    std::string target_element;
    std::string port_name;
    double value{0.0};
};

struct RecordedCheckpointEntry {
    double sim_time{0.0};
    uint64_t step_index{0};
    std::vector<double> state_vector;
};

class ExecutionReplayManager {
public:
    void record_input(SimulationInputEvent event);
    void record_checkpoint(RecordedCheckpointEntry cp);

    [[nodiscard]] const std::vector<SimulationInputEvent>& input_events() const noexcept { return input_events_; }
    [[nodiscard]] const std::vector<RecordedCheckpointEntry>& checkpoints() const noexcept { return checkpoints_; }

    [[nodiscard]] std::optional<RecordedCheckpointEntry> find_nearest_checkpoint_before(double target_time) const;
    [[nodiscard]] std::vector<SimulationInputEvent> get_inputs_between(double start_time, double end_time) const;

    void clear();

private:
    std::vector<SimulationInputEvent> input_events_;
    std::vector<RecordedCheckpointEntry> checkpoints_;
};

// =============================================================================
// RUN-08: Run Comparison (Trace Overlay, Metrics Compare, Diff Configs)
// =============================================================================

struct RunComparisonMetrics {
    double max_absolute_error{0.0};
    double mean_absolute_error{0.0};
    double root_mean_square_error{0.0};
    double peak_relative_error{0.0};
    bool identical{false};
};

struct ParameterDifference {
    std::string name;
    std::string value_a;
    std::string value_b;
};

struct RunComparisonResult {
    std::string run_id_a;
    std::string run_id_b;
    std::vector<ParameterDifference> parameter_diffs;
    std::unordered_map<std::string, RunComparisonMetrics> signal_metrics;

    [[nodiscard]] bool matches_within_tolerance(double tolerance) const noexcept;
    [[nodiscard]] std::string to_json() const;
};

class RunComparisonEngine {
public:
    [[nodiscard]] static RunComparisonMetrics compare_traces(
        const std::vector<double>& trace_a,
        const std::vector<double>& trace_b);

    [[nodiscard]] static std::vector<ParameterDifference> diff_configurations(
        const std::unordered_map<std::string, std::string>& config_a,
        const std::unordered_map<std::string, std::string>& config_b);

    [[nodiscard]] static RunComparisonResult compare_runs(
        std::string_view run_id_a,
        std::string_view run_id_b,
        const std::unordered_map<std::string, std::string>& config_a,
        const std::unordered_map<std::string, std::string>& config_b,
        const std::unordered_map<std::string, std::vector<double>>& signals_a,
        const std::unordered_map<std::string, std::vector<double>>& signals_b);
};

// =============================================================================
// RUN-09: Run Failure Report
// =============================================================================

enum class RunFailureCause {
    SingularMatrix,
    NewtonDivergence,
    TimeStepUnderflow,
    InfinityOrNaN,
    StateConstraintViolation,
    SafetyThresholdExceeded,
    UserAborted,
    Unknown
};

[[nodiscard]] constexpr std::string_view run_failure_cause_to_string(RunFailureCause cause) noexcept {
    switch (cause) {
        case RunFailureCause::SingularMatrix: return "SingularMatrix";
        case RunFailureCause::NewtonDivergence: return "NewtonDivergence";
        case RunFailureCause::TimeStepUnderflow: return "TimeStepUnderflow";
        case RunFailureCause::InfinityOrNaN: return "InfinityOrNaN";
        case RunFailureCause::StateConstraintViolation: return "StateConstraintViolation";
        case RunFailureCause::SafetyThresholdExceeded: return "SafetyThresholdExceeded";
        case RunFailureCause::UserAborted: return "UserAborted";
        case RunFailureCause::Unknown: return "Unknown";
    }
    return "Unknown";
}

[[nodiscard]] std::optional<RunFailureCause> run_failure_cause_from_string(std::string_view str) noexcept;

struct RunFailureReport {
    base::Uuid run_id;
    double failure_time{0.0};
    uint64_t failure_step{0};
    RunFailureCause cause{RunFailureCause::Unknown};
    std::string failing_element_id;
    std::string failing_domain;
    std::string solver_name;
    std::string detailed_message;
    std::vector<std::string> suggested_diagnostic_actions;

    [[nodiscard]] std::string format_diagnostic_summary() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<RunFailureReport> from_json(std::string_view json_str);
};

// =============================================================================
// RUN-10: Run Safety Controls & Safe Modes
// =============================================================================

enum class SafetyLimitAction {
    Warn,
    ClampAndLog,
    GracefulStop,
    EmergencyAbort
};

[[nodiscard]] constexpr std::string_view safety_limit_action_to_string(SafetyLimitAction action) noexcept {
    switch (action) {
        case SafetyLimitAction::Warn: return "Warn";
        case SafetyLimitAction::ClampAndLog: return "ClampAndLog";
        case SafetyLimitAction::GracefulStop: return "GracefulStop";
        case SafetyLimitAction::EmergencyAbort: return "EmergencyAbort";
    }
    return "EmergencyAbort";
}

struct SafetyThreshold {
    std::string name;
    size_t state_index{0};
    double min_allowed{-std::numeric_limits<double>::infinity()};
    double max_allowed{std::numeric_limits<double>::infinity()};
    SafetyLimitAction action{SafetyLimitAction::EmergencyAbort};
};

struct RunSafetyPolicy {
    double max_simulation_time{1e6};
    uint64_t max_step_count{10'000'000};
    double divergence_magnitude_limit{1e12};
    bool abort_on_nan{true};
    bool abort_on_infinity{true};
    std::vector<SafetyThreshold> thresholds;
};

struct SafetyCheckResult {
    bool tripped{false};
    SafetyLimitAction action{SafetyLimitAction::Warn};
    std::string reason;
    std::string threshold_name;
};

class SafetyGuardManager {
public:
    explicit SafetyGuardManager(RunSafetyPolicy policy = {});

    [[nodiscard]] const RunSafetyPolicy& policy() const noexcept { return policy_; }
    void set_policy(RunSafetyPolicy policy) { policy_ = std::move(policy); }

    /// Evaluates current state vector, time, and step index.
    /// Modifies state vector if action is ClampAndLog.
    [[nodiscard]] SafetyCheckResult evaluate_state(
        double sim_time,
        uint64_t step_count,
        std::vector<double>& state_vector);

    [[nodiscard]] const std::vector<std::string>& violation_log() const noexcept { return violation_log_; }

private:
    RunSafetyPolicy policy_;
    std::vector<std::string> violation_log_;
};

} // namespace shunya::core::domain
