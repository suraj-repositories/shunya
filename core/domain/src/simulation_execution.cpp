#include "shunya/core/domain/simulation_execution.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace shunya::core::domain {

// =============================================================================
// RUN-01: Interactive Visual Preview Mode
// =============================================================================

std::optional<ExecutionMode> execution_mode_from_string(std::string_view str) noexcept {
    if (str == "InteractivePreview") return ExecutionMode::InteractivePreview;
    if (str == "RealtimeSynced") return ExecutionMode::RealtimeSynced;
    if (str == "ScientificAuthoritative") return ExecutionMode::ScientificAuthoritative;
    if (str == "Accelerated") return ExecutionMode::Accelerated;
    if (str == "HeadlessBatch") return ExecutionMode::HeadlessBatch;
    return std::nullopt;
}

ExecutionModeDescriptor ExecutionModeDescriptor::describe(ExecutionMode mode) noexcept {
    switch (mode) {
        case ExecutionMode::InteractivePreview:
            return {
                .mode = ExecutionMode::InteractivePreview,
                .is_authoritative = false,
                .is_approximate = true,
                .has_visual_only_warning = true,
                .display_label = "[APPROXIMATE PREVIEW - NON-AUTHORITATIVE]",
                .description = "Interactive preview with relaxed numerical tolerances. Not certified for scientific verification."
            };
        case ExecutionMode::RealtimeSynced:
            return {
                .mode = ExecutionMode::RealtimeSynced,
                .is_authoritative = false,
                .is_approximate = false,
                .has_visual_only_warning = false,
                .display_label = "[REALTIME SYNCED]",
                .description = "Wall-clock paced simulation synchronized with display refresh intervals."
            };
        case ExecutionMode::ScientificAuthoritative:
            return {
                .mode = ExecutionMode::ScientificAuthoritative,
                .is_authoritative = true,
                .is_approximate = false,
                .has_visual_only_warning = false,
                .display_label = "[SCIENTIFIC AUTHORITATIVE]",
                .description = "Authoritative scientific execution enforcing strict error bounds, conservation, and deterministic reproducibility."
            };
        case ExecutionMode::Accelerated:
            return {
                .mode = ExecutionMode::Accelerated,
                .is_authoritative = true,
                .is_approximate = false,
                .has_visual_only_warning = false,
                .display_label = "[ACCELERATED COMPUTE]",
                .description = "Unthrottled maximum-throughput execution running as fast as computational hardware permits."
            };
        case ExecutionMode::HeadlessBatch:
            return {
                .mode = ExecutionMode::HeadlessBatch,
                .is_authoritative = true,
                .is_approximate = false,
                .has_visual_only_warning = false,
                .display_label = "[HEADLESS BATCH]",
                .description = "Non-interactive batch mode optimized for parameter sweeps, CI pipelines, and cloud cluster compute."
            };
    }
    return describe(ExecutionMode::InteractivePreview);
}

// =============================================================================
// RUN-02: Realtime Wall-Clock Synced Mode
// =============================================================================

RealtimePacer::RealtimePacer(RealtimePacingConfig config)
    : config_(config) {}

void RealtimePacer::start() {
    diagnostics_ = RealtimeDiagnostics{};
    is_paused_ = false;
    prev_sim_time_ = 0.0;
    prev_wall_time_ = 0.0;
}

void RealtimePacer::update(double current_sim_time, double wall_elapsed_seconds) {
    if (is_paused_) return;

    diagnostics_.frames_rendered++;
    diagnostics_.total_sim_seconds = current_sim_time;
    diagnostics_.total_wall_seconds = wall_elapsed_seconds;

    const double d_wall = wall_elapsed_seconds - prev_wall_time_;
    const double d_sim = current_sim_time - prev_sim_time_;

    if (d_wall > 1e-6) {
        diagnostics_.current_rtf = d_sim / d_wall;
        const double expected_sim = wall_elapsed_seconds * config_.target_rtf;
        diagnostics_.sync_jitter_seconds = current_sim_time - expected_sim;

        if (diagnostics_.current_rtf < config_.target_rtf * 0.90) {
            diagnostics_.status = TimingStatus::Lagging;
        } else if (diagnostics_.current_rtf > config_.target_rtf * 1.10) {
            diagnostics_.status = (diagnostics_.sync_jitter_seconds < 0.0) ? TimingStatus::CatchingUp : TimingStatus::Ahead;
        } else {
            diagnostics_.status = TimingStatus::OnTime;
        }
    }

    prev_sim_time_ = current_sim_time;
    prev_wall_time_ = wall_elapsed_seconds;
}

void RealtimePacer::pause() {
    is_paused_ = true;
}

void RealtimePacer::resume() {
    is_paused_ = false;
}

void RealtimePacer::record_step([[maybe_unused]] double dt) {
    diagnostics_.steps_executed++;
}

// =============================================================================
// RUN-03: Scientific Authoritative Mode
// =============================================================================

ScientificVerificationResult ScientificRunVerifier::verify_reproducibility(
    const std::vector<double>& run1_final_state,
    const std::vector<double>& run2_final_state,
    double tolerance)
{
    if (run1_final_state.size() != run2_final_state.size()) {
        return {
            .passed = false,
            .max_state_difference = std::numeric_limits<double>::infinity(),
            .max_energy_drift = 0.0,
            .diagnostic_summary = "State vector dimension mismatch (" +
                                  std::to_string(run1_final_state.size()) + " vs " +
                                  std::to_string(run2_final_state.size()) + ")"
        };
    }

    double max_diff = 0.0;
    for (size_t i = 0; i < run1_final_state.size(); ++i) {
        const double diff = std::abs(run1_final_state[i] - run2_final_state[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }

    const bool passed = (max_diff <= tolerance);
    std::string summary = passed
        ? "Reproducibility verified: max delta " + std::to_string(max_diff) + " <= tol " + std::to_string(tolerance)
        : "Reproducibility failure: max delta " + std::to_string(max_diff) + " > tol " + std::to_string(tolerance);

    return {
        .passed = passed,
        .max_state_difference = max_diff,
        .max_energy_drift = 0.0,
        .diagnostic_summary = std::move(summary)
    };
}

ScientificVerificationResult ScientificRunVerifier::verify_energy_conservation(
    const std::vector<double>& energy_history,
    double max_drift_fraction)
{
    if (energy_history.empty()) {
        return {
            .passed = false,
            .max_state_difference = 0.0,
            .max_energy_drift = 0.0,
            .diagnostic_summary = "Empty energy history provided"
        };
    }

    const double e0 = energy_history.front();
    if (std::abs(e0) < 1e-12) {
        // Absolute drift check
        double max_drift = 0.0;
        for (double e : energy_history) {
            max_drift = std::max(max_drift, std::abs(e - e0));
        }
        const bool passed = (max_drift <= max_drift_fraction);
        return {
            .passed = passed,
            .max_state_difference = 0.0,
            .max_energy_drift = max_drift,
            .diagnostic_summary = "Near-zero initial energy drift checked: " + std::to_string(max_drift)
        };
    }

    double max_drift = 0.0;
    for (double e : energy_history) {
        const double drift = std::abs(e - e0) / std::abs(e0);
        if (drift > max_drift) {
            max_drift = drift;
        }
    }

    const bool passed = (max_drift <= max_drift_fraction);
    std::string summary = passed
        ? "Energy conservation satisfied: drift " + std::to_string(max_drift) + " <= fraction " + std::to_string(max_drift_fraction)
        : "Energy conservation violated: drift " + std::to_string(max_drift) + " > fraction " + std::to_string(max_drift_fraction);

    return {
        .passed = passed,
        .max_state_difference = 0.0,
        .max_energy_drift = max_drift,
        .diagnostic_summary = std::move(summary)
    };
}

// =============================================================================
// RUN-04: Accelerated Mode
// =============================================================================

void AcceleratedRunController::record_batch(uint64_t step_count, double sim_dt, double compute_wall_seconds) {
    metrics_.steps_executed += step_count;
    metrics_.elapsed_sim_time += sim_dt;
    metrics_.elapsed_compute_time += compute_wall_seconds;

    if (metrics_.elapsed_compute_time > 1e-6) {
        metrics_.steps_per_second = static_cast<double>(metrics_.steps_executed) / metrics_.elapsed_compute_time;
        metrics_.achieved_rtf = metrics_.elapsed_sim_time / metrics_.elapsed_compute_time;
    }
}

void AcceleratedRunController::reset() {
    metrics_ = AcceleratedRunMetrics{};
}

// =============================================================================
// RUN-05: Slow-Motion and Time Dilation Display Controls
// =============================================================================

TimeDilationController::TimeDilationController(double initial_dilation)
    : dilation_factor_(initial_dilation > 0.0 ? initial_dilation : 1.0) {}

base::Result<void> TimeDilationController::set_dilation_factor(double factor) {
    if (factor <= 0.0 || std::isnan(factor) || std::isinf(factor)) {
        return base::Result<void>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::Simulation,
            .severity = base::Severity::Error,
            .message = "Time dilation factor must be strictly positive and finite",
            .details = ""
        });
    }
    dilation_factor_ = factor;
    return base::Result<void>::ok();
}

double TimeDilationController::physical_to_presentation_dt(double physical_dt) const noexcept {
    return physical_dt / dilation_factor_;
}

double TimeDilationController::presentation_to_physical_dt(double presentation_dt) const noexcept {
    return presentation_dt * dilation_factor_;
}

// =============================================================================
// RUN-06: Headless Batch Mode for Sweeps, CI, and Remote Compute
// =============================================================================

BatchExecutionRunner::BatchExecutionRunner(BatchExecutionConfig config)
    : config_(std::move(config)) {}

std::vector<std::unordered_map<std::string, double>> BatchExecutionRunner::generate_parameter_combinations() const {
    std::vector<std::unordered_map<std::string, double>> combinations;
    if (config_.parameters.empty()) {
        combinations.emplace_back();
        return combinations;
    }

    combinations.push_back({});

    for (const auto& param : config_.parameters) {
        if (param.values.empty()) continue;
        std::vector<std::unordered_map<std::string, double>> new_combos;
        new_combos.reserve(combinations.size() * param.values.size());

        for (const auto& existing : combinations) {
            for (double val : param.values) {
                auto extended = existing;
                extended[param.name] = val;
                new_combos.push_back(std::move(extended));
            }
        }
        combinations = std::move(new_combos);
    }

    return combinations;
}

BatchExecutionSummary BatchExecutionRunner::execute(
    const std::function<SingleRunResult(const std::unordered_map<std::string, double>&)>& run_func)
{
    BatchExecutionSummary summary;
    summary.batch_name = config_.batch_name;

    const auto combos = generate_parameter_combinations();
    summary.total_runs = static_cast<uint32_t>(combos.size());

    for (const auto& combo : combos) {
        auto run_result = run_func(combo);
        if (run_result.success) {
            summary.successful_runs++;
        } else {
            summary.failed_runs++;
        }
        summary.run_results.push_back(run_result);

        if (!run_result.success && config_.failure_policy == BatchFailurePolicy::AbortBatch) {
            break;
        }
    }

    return summary;
}

std::string BatchExecutionSummary::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"batch_name\": \"" << batch_name << "\",\n";
    ss << "  \"total_runs\": " << total_runs << ",\n";
    ss << "  \"successful_runs\": " << successful_runs << ",\n";
    ss << "  \"failed_runs\": " << failed_runs << ",\n";
    ss << "  \"runs\": [\n";

    for (size_t i = 0; i < run_results.size(); ++i) {
        const auto& r = run_results[i];
        ss << "    {\n";
        ss << "      \"run_id\": \"" << r.run_id << "\",\n";
        ss << "      \"success\": " << (r.success ? "true" : "false") << ",\n";
        ss << "      \"sim_time_reached\": " << r.sim_time_reached << ",\n";
        ss << "      \"parameters\": {";
        bool first = true;
        for (const auto& [k, v] : r.parameter_values) {
            if (!first) ss << ", ";
            ss << "\"" << k << "\": " << v;
            first = false;
        }
        ss << "}\n";
        ss << "    }" << (i + 1 < run_results.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// RUN-07: Checkpoint/Restart and Deterministic Replay
// =============================================================================

void ExecutionReplayManager::record_input(SimulationInputEvent event) {
    input_events_.push_back(std::move(event));
}

void ExecutionReplayManager::record_checkpoint(RecordedCheckpointEntry cp) {
    checkpoints_.push_back(std::move(cp));
}

std::optional<RecordedCheckpointEntry> ExecutionReplayManager::find_nearest_checkpoint_before(double target_time) const {
    std::optional<RecordedCheckpointEntry> best;
    for (const auto& cp : checkpoints_) {
        if (cp.sim_time <= target_time) {
            if (!best || cp.sim_time > best->sim_time) {
                best = cp;
            }
        }
    }
    return best;
}

std::vector<SimulationInputEvent> ExecutionReplayManager::get_inputs_between(double start_time, double end_time) const {
    std::vector<SimulationInputEvent> result;
    for (const auto& evt : input_events_) {
        if (evt.timestamp >= start_time && evt.timestamp <= end_time) {
            result.push_back(evt);
        }
    }
    return result;
}

void ExecutionReplayManager::clear() {
    input_events_.clear();
    checkpoints_.clear();
}

// =============================================================================
// RUN-08: Run Comparison Engine
// =============================================================================

RunComparisonMetrics RunComparisonEngine::compare_traces(
    const std::vector<double>& trace_a,
    const std::vector<double>& trace_b)
{
    RunComparisonMetrics metrics;
    const size_t n = std::min(trace_a.size(), trace_b.size());
    if (n == 0) {
        metrics.identical = (trace_a.empty() && trace_b.empty());
        return metrics;
    }

    double sum_abs = 0.0;
    double sum_sq = 0.0;
    double max_err = 0.0;
    double peak_rel = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double err = std::abs(trace_a[i] - trace_b[i]);
        sum_abs += err;
        sum_sq += err * err;
        if (err > max_err) max_err = err;

        const double ref = std::max(std::abs(trace_a[i]), 1e-12);
        const double rel = err / ref;
        if (rel > peak_rel) peak_rel = rel;
    }

    metrics.max_absolute_error = max_err;
    metrics.mean_absolute_error = sum_abs / static_cast<double>(n);
    metrics.root_mean_square_error = std::sqrt(sum_sq / static_cast<double>(n));
    metrics.peak_relative_error = peak_rel;
    metrics.identical = (trace_a.size() == trace_b.size()) && (max_err == 0.0);

    return metrics;
}

std::vector<ParameterDifference> RunComparisonEngine::diff_configurations(
    const std::unordered_map<std::string, std::string>& config_a,
    const std::unordered_map<std::string, std::string>& config_b)
{
    std::vector<ParameterDifference> diffs;

    for (const auto& [key, val_a] : config_a) {
        auto it = config_b.find(key);
        if (it == config_b.end()) {
            diffs.push_back({key, val_a, "<missing>"});
        } else if (it->second != val_a) {
            diffs.push_back({key, val_a, it->second});
        }
    }

    for (const auto& [key, val_b] : config_b) {
        if (!config_a.contains(key)) {
            diffs.push_back({key, "<missing>", val_b});
        }
    }

    std::sort(diffs.begin(), diffs.end(), [](const auto& l, const auto& r) {
        return l.name < r.name;
    });

    return diffs;
}

RunComparisonResult RunComparisonEngine::compare_runs(
    std::string_view run_id_a,
    std::string_view run_id_b,
    const std::unordered_map<std::string, std::string>& config_a,
    const std::unordered_map<std::string, std::string>& config_b,
    const std::unordered_map<std::string, std::vector<double>>& signals_a,
    const std::unordered_map<std::string, std::vector<double>>& signals_b)
{
    RunComparisonResult result;
    result.run_id_a = std::string(run_id_a);
    result.run_id_b = std::string(run_id_b);
    result.parameter_diffs = diff_configurations(config_a, config_b);

    for (const auto& [name, trace_a] : signals_a) {
        auto it = signals_b.find(name);
        if (it != signals_b.end()) {
            result.signal_metrics[name] = compare_traces(trace_a, it->second);
        }
    }

    return result;
}

bool RunComparisonResult::matches_within_tolerance(double tolerance) const noexcept {
    for (const auto& [_, metrics] : signal_metrics) {
        if (metrics.max_absolute_error > tolerance) {
            return false;
        }
    }
    return true;
}

std::string RunComparisonResult::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"run_id_a\": \"" << run_id_a << "\",\n";
    ss << "  \"run_id_b\": \"" << run_id_b << "\",\n";
    ss << "  \"parameter_differences\": [\n";
    for (size_t i = 0; i < parameter_diffs.size(); ++i) {
        const auto& d = parameter_diffs[i];
        ss << "    {\"param\": \"" << d.name << "\", \"val_a\": \"" << d.value_a
           << "\", \"val_b\": \"" << d.value_b << "\"}"
           << (i + 1 < parameter_diffs.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";
    ss << "  \"signals\": {\n";
    size_t count = 0;
    for (const auto& [sig, m] : signal_metrics) {
        ss << "    \"" << sig << "\": {\n";
        ss << "      \"max_err\": " << m.max_absolute_error << ",\n";
        ss << "      \"mae\": " << m.mean_absolute_error << ",\n";
        ss << "      \"rmse\": " << m.root_mean_square_error << ",\n";
        ss << "      \"identical\": " << (m.identical ? "true" : "false") << "\n";
        ss << "    }" << (++count < signal_metrics.size() ? "," : "") << "\n";
    }
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// RUN-09: Run Failure Report
// =============================================================================

std::optional<RunFailureCause> run_failure_cause_from_string(std::string_view str) noexcept {
    if (str == "SingularMatrix") return RunFailureCause::SingularMatrix;
    if (str == "NewtonDivergence") return RunFailureCause::NewtonDivergence;
    if (str == "TimeStepUnderflow") return RunFailureCause::TimeStepUnderflow;
    if (str == "InfinityOrNaN") return RunFailureCause::InfinityOrNaN;
    if (str == "StateConstraintViolation") return RunFailureCause::StateConstraintViolation;
    if (str == "SafetyThresholdExceeded") return RunFailureCause::SafetyThresholdExceeded;
    if (str == "UserAborted") return RunFailureCause::UserAborted;
    if (str == "Unknown") return RunFailureCause::Unknown;
    return std::nullopt;
}

std::string RunFailureReport::format_diagnostic_summary() const {
    std::ostringstream ss;
    ss << "=== SIMULATION FAILURE REPORT ===\n";
    ss << "Run ID:       " << run_id.to_string() << "\n";
    ss << "Sim Time:     " << failure_time << " s (Step " << failure_step << ")\n";
    ss << "Cause:        " << run_failure_cause_to_string(cause) << "\n";
    if (!failing_domain.empty()) ss << "Domain:       " << failing_domain << "\n";
    if (!failing_element_id.empty()) ss << "Element:      " << failing_element_id << "\n";
    if (!solver_name.empty()) ss << "Solver:       " << solver_name << "\n";
    ss << "Detail:       " << detailed_message << "\n";
    if (!suggested_diagnostic_actions.empty()) {
        ss << "Suggested Actions:\n";
        for (const auto& act : suggested_diagnostic_actions) {
            ss << "  - " << act << "\n";
        }
    }
    return ss.str();
}

std::string RunFailureReport::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"run_id\": \"" << run_id.to_string() << "\",\n";
    ss << "  \"failure_time\": " << failure_time << ",\n";
    ss << "  \"failure_step\": " << failure_step << ",\n";
    ss << "  \"cause\": \"" << run_failure_cause_to_string(cause) << "\",\n";
    ss << "  \"failing_element_id\": \"" << failing_element_id << "\",\n";
    ss << "  \"failing_domain\": \"" << failing_domain << "\",\n";
    ss << "  \"solver_name\": \"" << solver_name << "\",\n";
    ss << "  \"detailed_message\": \"" << detailed_message << "\",\n";
    ss << "  \"suggested_actions\": [\n";
    for (size_t i = 0; i < suggested_diagnostic_actions.size(); ++i) {
        ss << "    \"" << suggested_diagnostic_actions[i] << "\""
           << (i + 1 < suggested_diagnostic_actions.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<RunFailureReport> RunFailureReport::from_json(std::string_view json_str) {
    RunFailureReport rpt;
    // Basic field extraction from json
    auto get_str_val = [](std::string_view json, std::string_view key) -> std::string {
        std::string pattern = "\"" + std::string(key) + "\": \"";
        auto pos = json.find(pattern);
        if (pos == std::string_view::npos) return "";
        pos += pattern.size();
        auto end = json.find('"', pos);
        if (end == std::string_view::npos) return "";
        return std::string(json.substr(pos, end - pos));
    };

    auto get_num_val = [](std::string_view json, std::string_view key) -> double {
        std::string pattern = "\"" + std::string(key) + "\": ";
        auto pos = json.find(pattern);
        if (pos == std::string_view::npos) return 0.0;
        pos += pattern.size();
        char* end_ptr = nullptr;
        return std::strtod(json.data() + pos, &end_ptr);
    };

    std::string id_str = get_str_val(json_str, "run_id");
    auto uuid_res = base::Uuid::from_string(id_str);
    if (uuid_res.has_value()) {
        rpt.run_id = uuid_res.value();
    }
    rpt.failure_time = get_num_val(json_str, "failure_time");
    rpt.failure_step = static_cast<uint64_t>(get_num_val(json_str, "failure_step"));
    std::string cause_str = get_str_val(json_str, "cause");
    rpt.cause = run_failure_cause_from_string(cause_str).value_or(RunFailureCause::Unknown);
    rpt.failing_element_id = get_str_val(json_str, "failing_element_id");
    rpt.failing_domain = get_str_val(json_str, "failing_domain");
    rpt.solver_name = get_str_val(json_str, "solver_name");
    rpt.detailed_message = get_str_val(json_str, "detailed_message");

    return base::Result<RunFailureReport>::ok(std::move(rpt));
}

// =============================================================================
// RUN-10: Run Safety Controls & Safe Modes
// =============================================================================

SafetyGuardManager::SafetyGuardManager(RunSafetyPolicy policy)
    : policy_(std::move(policy)) {}

SafetyCheckResult SafetyGuardManager::evaluate_state(
    double sim_time,
    uint64_t step_count,
    std::vector<double>& state_vector)
{
    // 1. Time / step bounds check
    if (sim_time > policy_.max_simulation_time) {
        violation_log_.push_back("Simulation time exceeded maximum limit (" +
                                 std::to_string(sim_time) + " > " +
                                 std::to_string(policy_.max_simulation_time) + ")");
        return {
            .tripped = true,
            .action = SafetyLimitAction::GracefulStop,
            .reason = "Max simulation time limit reached",
            .threshold_name = "max_simulation_time"
        };
    }

    if (step_count > policy_.max_step_count) {
        violation_log_.push_back("Step count exceeded limit (" +
                                 std::to_string(step_count) + " > " +
                                 std::to_string(policy_.max_step_count) + ")");
        return {
            .tripped = true,
            .action = SafetyLimitAction::GracefulStop,
            .reason = "Max step count reached",
            .threshold_name = "max_step_count"
        };
    }

    // 2. NaN and Inf check
    for (size_t i = 0; i < state_vector.size(); ++i) {
        const double val = state_vector[i];
        if (policy_.abort_on_nan && std::isnan(val)) {
            std::string msg = "NaN detected in state variable index " + std::to_string(i);
            violation_log_.push_back(msg);
            return {
                .tripped = true,
                .action = SafetyLimitAction::EmergencyAbort,
                .reason = std::move(msg),
                .threshold_name = "abort_on_nan"
            };
        }
        if (policy_.abort_on_infinity && std::isinf(val)) {
            std::string msg = "Infinity detected in state variable index " + std::to_string(i);
            violation_log_.push_back(msg);
            return {
                .tripped = true,
                .action = SafetyLimitAction::EmergencyAbort,
                .reason = std::move(msg),
                .threshold_name = "abort_on_infinity"
            };
        }
        if (std::abs(val) > policy_.divergence_magnitude_limit) {
            std::string msg = "State variable index " + std::to_string(i) + " diverged magnitude " +
                              std::to_string(std::abs(val)) + " > limit " +
                              std::to_string(policy_.divergence_magnitude_limit);
            violation_log_.push_back(msg);
            return {
                .tripped = true,
                .action = SafetyLimitAction::EmergencyAbort,
                .reason = std::move(msg),
                .threshold_name = "divergence_magnitude_limit"
            };
        }
    }

    // 3. User-defined thresholds
    for (const auto& th : policy_.thresholds) {
        if (th.state_index >= state_vector.size()) continue;

        double& val = state_vector[th.state_index];
        if (val < th.min_allowed || val > th.max_allowed) {
            std::string msg = "Threshold '" + th.name + "' tripped: value " + std::to_string(val) +
                              " outside [" + std::to_string(th.min_allowed) + ", " +
                              std::to_string(th.max_allowed) + "]";
            violation_log_.push_back(msg);

            if (th.action == SafetyLimitAction::ClampAndLog) {
                val = std::clamp(val, th.min_allowed, th.max_allowed);
            }

            return {
                .tripped = true,
                .action = th.action,
                .reason = std::move(msg),
                .threshold_name = th.name
            };
        }
    }

    return SafetyCheckResult{
        .tripped = false,
        .action = SafetyLimitAction::Warn,
        .reason = "",
        .threshold_name = ""
    };
}

} // namespace shunya::core::domain
