#include "tests/test_framework.hpp"
#include "shunya/core/domain/simulation_execution.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// RUN-01: Interactive Preview Mode
// =============================================================================

TEST_CASE("RUN-01: Execution Modes and Interactive Preview Warnings") {
    // String conversion round-trips
    CHECK(execution_mode_to_string(ExecutionMode::InteractivePreview) == "InteractivePreview");
    CHECK(execution_mode_to_string(ExecutionMode::RealtimeSynced) == "RealtimeSynced");
    CHECK(execution_mode_to_string(ExecutionMode::ScientificAuthoritative) == "ScientificAuthoritative");
    CHECK(execution_mode_to_string(ExecutionMode::Accelerated) == "Accelerated");
    CHECK(execution_mode_to_string(ExecutionMode::HeadlessBatch) == "HeadlessBatch");

    CHECK(execution_mode_from_string("InteractivePreview").value() == ExecutionMode::InteractivePreview);
    CHECK(execution_mode_from_string("RealtimeSynced").value() == ExecutionMode::RealtimeSynced);
    CHECK(execution_mode_from_string("ScientificAuthoritative").value() == ExecutionMode::ScientificAuthoritative);
    CHECK(execution_mode_from_string("Accelerated").value() == ExecutionMode::Accelerated);
    CHECK(execution_mode_from_string("HeadlessBatch").value() == ExecutionMode::HeadlessBatch);
    CHECK_FALSE(execution_mode_from_string("NonExistentMode").has_value());

    // Interactive Preview must be approximate, non-authoritative, and flag visual warning
    const auto preview_desc = ExecutionModeDescriptor::describe(ExecutionMode::InteractivePreview);
    CHECK_FALSE(preview_desc.is_authoritative);
    CHECK(preview_desc.is_approximate);
    CHECK(preview_desc.has_visual_only_warning);
    CHECK(preview_desc.display_label.find("APPROXIMATE") != std::string::npos);

    // Scientific Authoritative must be authoritative and not approximate
    const auto sci_desc = ExecutionModeDescriptor::describe(ExecutionMode::ScientificAuthoritative);
    CHECK(sci_desc.is_authoritative);
    CHECK_FALSE(sci_desc.is_approximate);
    CHECK_FALSE(sci_desc.has_visual_only_warning);
    CHECK(sci_desc.display_label.find("SCIENTIFIC") != std::string::npos);
}

// =============================================================================
// RUN-02: Realtime Wall-Clock Synced Mode
// =============================================================================

TEST_CASE("RUN-02: Realtime Mode Pacer, Throttling and Diagnostics") {
    RealtimePacingConfig cfg{
        .target_rtf = 1.0,
        .target_frame_interval = std::chrono::microseconds(16667),
        .catch_up_allowed = true,
        .max_catch_up_steps_per_frame = 5
    };

    RealtimePacer pacer(cfg);
    pacer.start();
    CHECK_FALSE(pacer.is_paused());
    CHECK(pacer.diagnostics().steps_executed == 0u);

    // Step recording
    pacer.record_step(0.001);
    pacer.record_step(0.001);
    CHECK(pacer.diagnostics().steps_executed == 2u);

    // Initial update: zero wall time
    pacer.update(0.0, 0.0);

    // Nominal frame: 0.016s sim time in 0.016s wall time -> OnTime
    pacer.update(0.016, 0.016);
    CHECK(pacer.diagnostics().status == TimingStatus::OnTime);
    CHECK(std::abs(pacer.diagnostics().current_rtf - 1.0) < 0.05);

    // Lagging frame: sim time advances only 0.005s in 0.020s wall time -> Lagging
    pacer.update(0.021, 0.036);
    CHECK(pacer.diagnostics().status == TimingStatus::Lagging);

    // Pause / Resume
    pacer.pause();
    CHECK(pacer.is_paused());
    pacer.update(0.1, 0.1); // Ignored when paused
    CHECK(pacer.diagnostics().frames_rendered == 3u);

    pacer.resume();
    CHECK_FALSE(pacer.is_paused());
}

// =============================================================================
// RUN-03: Scientific Mode & Reproducibility
// =============================================================================

TEST_CASE("RUN-03: Scientific Mode Reproducibility and Conservation") {
    // 1. Bitwise / tight reproducibility verification
    std::vector<double> state_a = {1.000000000001, -2.500000000002, 3.141592653589};
    std::vector<double> state_b = {1.000000000002, -2.500000000001, 3.141592653588};

    auto res = ScientificRunVerifier::verify_reproducibility(state_a, state_b, 1e-9);
    CHECK(res.passed);
    CHECK(res.max_state_difference < 1e-9);

    // Exceeding tolerance
    std::vector<double> state_divergent = {1.001, -2.5, 3.14};
    auto res_div = ScientificRunVerifier::verify_reproducibility(state_a, state_divergent, 1e-6);
    CHECK_FALSE(res_div.passed);
    CHECK(res_div.max_state_difference > 1e-4);

    // Dimension mismatch
    std::vector<double> state_short = {1.0, -2.5};
    auto res_dim = ScientificRunVerifier::verify_reproducibility(state_a, state_short);
    CHECK_FALSE(res_dim.passed);

    // 2. Energy conservation check
    std::vector<double> conserved_energy = {100.0, 100.001, 99.999, 100.002, 100.0};
    auto res_e = ScientificRunVerifier::verify_energy_conservation(conserved_energy, 1e-4);
    CHECK(res_e.passed);

    std::vector<double> leaking_energy = {100.0, 95.0, 90.0, 80.0};
    auto res_leak = ScientificRunVerifier::verify_energy_conservation(leaking_energy, 1e-4);
    CHECK_FALSE(res_leak.passed);
}

// =============================================================================
// RUN-04: Accelerated Mode
// =============================================================================

TEST_CASE("RUN-04: Accelerated Mode Throughput Tracking") {
    AcceleratedRunController controller;
    CHECK(controller.metrics().steps_executed == 0u);

    // Record batch: 10,000 steps representing 1.0 sim-second, executed in 0.02 wall-seconds
    controller.record_batch(10000, 1.0, 0.02);
    CHECK(controller.metrics().steps_executed == 10000u);
    CHECK(std::abs(controller.metrics().steps_per_second - 500000.0) < 1000.0);
    CHECK(std::abs(controller.metrics().achieved_rtf - 50.0) < 0.1);

    // Additional batch: 5,000 steps, 0.5 sim-second, 0.01 wall-seconds
    controller.record_batch(5000, 0.5, 0.01);
    CHECK(controller.metrics().steps_executed == 15000u);
    CHECK(std::abs(controller.metrics().achieved_rtf - 50.0) < 0.1);

    controller.reset();
    CHECK(controller.metrics().steps_executed == 0u);
}

// =============================================================================
// RUN-05: Time Dilation Display Controls
// =============================================================================

TEST_CASE("RUN-05: Time Dilation Display Controls and Model Invariance") {
    TimeDilationController dilation(1.0);
    CHECK(std::abs(dilation.dilation_factor() - 1.0) < 1e-9);

    const double physical_dt = 0.005; // 5 ms physical step
    CHECK(std::abs(dilation.physical_to_presentation_dt(physical_dt) - 0.005) < 1e-9);

    // Set 0.1x (10x slow-motion)
    auto ok = dilation.set_dilation_factor(0.1);
    CHECK(ok.is_ok());
    CHECK(std::abs(dilation.dilation_factor() - 0.1) < 1e-9);

    // Presentation dt is 10x longer (50 ms frame duration)
    CHECK(std::abs(dilation.physical_to_presentation_dt(physical_dt) - 0.05) < 1e-9);
    // Inversion restores physical dt
    CHECK(std::abs(dilation.presentation_to_physical_dt(0.05) - physical_dt) < 1e-9);

    // Validation: invalid non-positive factors rejected
    auto err_zero = dilation.set_dilation_factor(0.0);
    CHECK(err_zero.is_err());
    CHECK(err_zero.error().code == 1);

    auto err_neg = dilation.set_dilation_factor(-0.5);
    CHECK(err_neg.is_err());
}

// =============================================================================
// RUN-06: Headless Batch Mode for Sweeps, CI, and Remote Compute
// =============================================================================

TEST_CASE("RUN-06: Headless Batch Parameter Sweeps and Failure Policies") {
    BatchExecutionConfig cfg{
        .batch_name = "RCSweep",
        .parameters = {
            SweepParameter{.name = "R", .values = {100.0, 200.0}},
            SweepParameter{.name = "C", .values = {1e-6, 2e-6, 4e-6}}
        },
        .failure_policy = BatchFailurePolicy::ContinueOnFailure,
        .headless = true,
        .output_directory = "results/rc_sweep"
    };

    BatchExecutionRunner runner(cfg);
    auto combos = runner.generate_parameter_combinations();
    CHECK(combos.size() == 6u); // 2 * 3 = 6 combinations

    // Execute with ContinueOnFailure
    auto summary = runner.execute([](const auto& params) -> SingleRunResult {
        const double r = params.at("R");
        const double c = params.at("C");
        // Simulated failure on R == 200 and C == 4e-6
        if (r == 200.0 && c == 4e-6) {
            return {
                .run_id = "run_fail",
                .parameter_values = params,
                .success = false,
                .sim_time_reached = 0.001,
                .error_message = "Convergence failure"
            };
        }
        return {
            .run_id = "run_ok",
            .parameter_values = params,
            .success = true,
            .sim_time_reached = 0.01,
            .error_message = ""
        };
    });

    CHECK(summary.total_runs == 6u);
    CHECK(summary.successful_runs == 5u);
    CHECK(summary.failed_runs == 1u);
    CHECK(summary.run_results.size() == 6u);

    // Verify JSON report
    std::string json = summary.to_json();
    CHECK(json.find("\"batch_name\": \"RCSweep\"") != std::string::npos);
    CHECK(json.find("\"successful_runs\": 5") != std::string::npos);

    // Test AbortBatch policy
    cfg.failure_policy = BatchFailurePolicy::AbortBatch;
    BatchExecutionRunner abort_runner(cfg);
    auto abort_summary = abort_runner.execute([](const auto& params) -> SingleRunResult {
        return {
            .run_id = "run_early_fail",
            .parameter_values = params,
            .success = false,
            .sim_time_reached = 0.0,
            .error_message = "Immediate abort"
        };
    });
    // Aborted after first run
    CHECK(abort_summary.run_results.size() == 1u);
    CHECK(abort_summary.failed_runs == 1u);
}

// =============================================================================
// RUN-07: Checkpoint/Restart and Deterministic Replay
// =============================================================================

TEST_CASE("RUN-07: Checkpoint/Restart and Event Replay Management") {
    ExecutionReplayManager replay;

    // Record checkpoints at t = 0.0, 1.0, 2.0
    replay.record_checkpoint({.sim_time = 0.0, .step_index = 0, .state_vector = {0.0, 0.0}});
    replay.record_checkpoint({.sim_time = 1.0, .step_index = 100, .state_vector = {1.5, -0.2}});
    replay.record_checkpoint({.sim_time = 2.0, .step_index = 200, .state_vector = {2.8, -0.4}});

    // Record inputs at t = 0.5, 1.2, 1.8
    replay.record_input({.timestamp = 0.5, .target_element = "V1", .port_name = "in", .value = 5.0});
    replay.record_input({.timestamp = 1.2, .target_element = "V1", .port_name = "in", .value = 10.0});
    replay.record_input({.timestamp = 1.8, .target_element = "SW1", .port_name = "ctrl", .value = 1.0});

    // Query nearest checkpoint before t = 1.5 -> should be t = 1.0
    auto cp = replay.find_nearest_checkpoint_before(1.5);
    REQUIRE(cp.has_value());
    CHECK(std::abs(cp->sim_time - 1.0) < 1e-9);
    CHECK(cp->step_index == 100u);

    // Query inputs between t = 1.0 and t = 2.0 -> should return events at 1.2 and 1.8
    auto inputs = replay.get_inputs_between(1.0, 2.0);
    CHECK(inputs.size() == 2u);
    CHECK(std::abs(inputs[0].timestamp - 1.2) < 1e-9);
    CHECK(std::abs(inputs[1].timestamp - 1.8) < 1e-9);

    replay.clear();
    CHECK(replay.checkpoints().size() == 0u);
    CHECK(replay.input_events().size() == 0u);
}

// =============================================================================
// RUN-08: Run Comparison (Trace Overlay, Metrics Compare, Diff Configs)
// =============================================================================

TEST_CASE("RUN-08: Run Comparison Engine Metrics and Configuration Diff") {
    // 1. Trace comparison
    std::vector<double> trace1 = {0.0, 1.0, 2.0, 3.0, 4.0};
    std::vector<double> trace2 = {0.0, 1.01, 1.99, 3.02, 3.98};

    auto metrics = RunComparisonEngine::compare_traces(trace1, trace2);
    CHECK_FALSE(metrics.identical);
    CHECK(std::abs(metrics.max_absolute_error - 0.02) < 1e-6);
    CHECK(std::abs(metrics.mean_absolute_error - 0.012) < 1e-6);
    CHECK(metrics.root_mean_square_error > 0.0);

    // Identical traces
    auto id_metrics = RunComparisonEngine::compare_traces(trace1, trace1);
    CHECK(id_metrics.identical);
    CHECK(id_metrics.max_absolute_error == 0.0);

    // 2. Config diff
    std::unordered_map<std::string, std::string> cfg_a = {
        {"solver", "RungeKutta4"},
        {"max_dt", "0.001"},
        {"tolerance", "1e-6"}
    };
    std::unordered_map<std::string, std::string> cfg_b = {
        {"solver", "RungeKutta4"},
        {"max_dt", "0.0005"},
        {"damping", "0.05"}
    };

    auto diffs = RunComparisonEngine::diff_configurations(cfg_a, cfg_b);
    CHECK(diffs.size() == 3u); // damping (missing in A), max_dt (differ), tolerance (missing in B)

    // 3. Full run comparison
    std::unordered_map<std::string, std::vector<double>> signals_a = {{"voltage", trace1}};
    std::unordered_map<std::string, std::vector<double>> signals_b = {{"voltage", trace2}};

    auto comp = RunComparisonEngine::compare_runs(
        "run_001", "run_002",
        cfg_a, cfg_b,
        signals_a, signals_b);

    CHECK(comp.run_id_a == "run_001");
    CHECK(comp.run_id_b == "run_002");
    CHECK(comp.matches_within_tolerance(0.05));
    CHECK_FALSE(comp.matches_within_tolerance(0.01));

    std::string json = comp.to_json();
    CHECK(json.find("\"run_id_a\": \"run_001\"") != std::string::npos);
}

// =============================================================================
// RUN-09: Run Failure Report
// =============================================================================

TEST_CASE("RUN-09: Run Failure Report Generation and JSON Roundtrip") {
    RunFailureReport rpt{
        .run_id = base::Uuid::generate_random(),
        .failure_time = 0.0452,
        .failure_step = 4520,
        .cause = RunFailureCause::SingularMatrix,
        .failing_element_id = "Resistor_Bridge_R1",
        .failing_domain = "Electrical",
        .solver_name = "ModifiedNodalAnalysis",
        .detailed_message = "Conductance matrix is structurally singular at node 4 (floating node).",
        .suggested_diagnostic_actions = {
            "Check for floating nodes without ground reference.",
            "Add high-value bleed resistor to ground.",
            "Enable diagonal pivot regularization."
        }
    };

    std::string summary = rpt.format_diagnostic_summary();
    CHECK(summary.find("SIMULATION FAILURE REPORT") != std::string::npos);
    CHECK(summary.find("SingularMatrix") != std::string::npos);
    CHECK(summary.find("Resistor_Bridge_R1") != std::string::npos);
    CHECK(summary.find("Suggested Actions:") != std::string::npos);

    // JSON serialization & deserialization
    std::string json = rpt.to_json();
    auto parsed = RunFailureReport::from_json(json);
    REQUIRE(parsed.is_ok());
    CHECK(parsed.value().run_id.to_string() == rpt.run_id.to_string());
    CHECK(std::abs(parsed.value().failure_time - 0.0452) < 1e-6);
    CHECK(parsed.value().failure_step == 4520u);
    CHECK(parsed.value().cause == RunFailureCause::SingularMatrix);
    CHECK(parsed.value().failing_element_id == "Resistor_Bridge_R1");
    CHECK(parsed.value().failing_domain == "Electrical");
}

// =============================================================================
// RUN-10: Run Safety Controls & Safe Modes
// =============================================================================

TEST_CASE("RUN-10: Run Safety Policy, Interlocks, and Divergence Guards") {
    RunSafetyPolicy policy{
        .max_simulation_time = 10.0,
        .max_step_count = 1000,
        .divergence_magnitude_limit = 1000.0,
        .abort_on_nan = true,
        .abort_on_infinity = true,
        .thresholds = {
            SafetyThreshold{
                .name = "TemperatureLimit",
                .state_index = 0,
                .min_allowed = 200.0,
                .max_allowed = 450.0,
                .action = SafetyLimitAction::EmergencyAbort
            },
            SafetyThreshold{
                .name = "VoltageClamp",
                .state_index = 1,
                .min_allowed = -15.0,
                .max_allowed = 15.0,
                .action = SafetyLimitAction::ClampAndLog
            }
        }
    };

    SafetyGuardManager guard(policy);

    // 1. Nominal state: within all thresholds
    std::vector<double> state = {300.0, 10.0};
    auto res_nominal = guard.evaluate_state(1.0, 50, state);
    CHECK_FALSE(res_nominal.tripped);

    // 2. Max simulation time exceeded
    auto res_time = guard.evaluate_state(10.5, 50, state);
    CHECK(res_time.tripped);
    CHECK(res_time.action == SafetyLimitAction::GracefulStop);

    // 3. Max step count exceeded
    auto res_step = guard.evaluate_state(5.0, 1050, state);
    CHECK(res_step.tripped);
    CHECK(res_step.action == SafetyLimitAction::GracefulStop);

    // 4. NaN detection
    std::vector<double> nan_state = {std::numeric_limits<double>::quiet_NaN(), 0.0};
    auto res_nan = guard.evaluate_state(1.0, 50, nan_state);
    CHECK(res_nan.tripped);
    CHECK(res_nan.action == SafetyLimitAction::EmergencyAbort);

    // 5. User-defined EmergencyAbort threshold (Temperature > 450 K)
    std::vector<double> hot_state = {500.0, 5.0};
    auto res_hot = guard.evaluate_state(1.0, 50, hot_state);
    CHECK(res_hot.tripped);
    CHECK(res_hot.action == SafetyLimitAction::EmergencyAbort);
    CHECK(res_hot.threshold_name == "TemperatureLimit");

    // 6. User-defined ClampAndLog threshold (Voltage > 15.0 clamped to 15.0)
    std::vector<double> high_v_state = {300.0, 25.0};
    auto res_clamp = guard.evaluate_state(1.0, 50, high_v_state);
    CHECK(res_clamp.tripped);
    CHECK(res_clamp.action == SafetyLimitAction::ClampAndLog);
    CHECK(std::abs(high_v_state[1] - 15.0) < 1e-9); // Clamped in-place!
    CHECK(guard.violation_log().size() > 0);
}
