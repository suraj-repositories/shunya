#include "tests/test_framework.hpp"

#include "shunya/core/domain/simulation_clock.hpp"
#include "shunya/core/domain/simulation_runtime.hpp"
#include "shunya/core/domain/project.hpp"

#include <cmath>
#include <memory>
#include <regex>
#include <thread>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

namespace {

base::Error make_mock_error(std::string msg, int32_t code = 9999) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Simulation,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

// Mock simulation participant for testing deterministic coordination and stepping
class MockOscillatorParticipant : public ISimulationParticipant {
public:
    explicit MockOscillatorParticipant(std::string name, int order = 0, double freq = 1.0)
        : name_(std::move(name)), order_(order), freq_(freq) {}

    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] int execution_order() const override { return order_; }

    base::Result<void> initialize(const SimulationConfig& config) override {
        (void)config;
        init_called = true;
        y = 1.0;
        v = 0.0;
        return {};
    }

    base::Result<void> pre_step(double t, double dt) override {
        (void)t;
        (void)dt;
        pre_step_count++;
        return {};
    }

    base::Result<StepResult> step(double t, double dt) override {
        (void)t;
        step_count++;

        if (fail_on_next_step) {
            fail_on_next_step = false;
            return make_mock_error("Synthetic oscillator failure");
        }

        if (reject_on_next_step) {
            reject_on_next_step = false;
            StepResult rej;
            rej.step_accepted = false;
            rej.message = "Oscillator local truncation error exceeded threshold";
            return rej;
        }

        // Simple harmonic oscillator integration: y'' = -omega^2 * y
        const double omega = 2.0 * 3.14159265358979323846 * freq_;
        const double a = -omega * omega * y;
        v += a * dt;
        y += v * dt;

        StepResult result;
        result.success = true;
        result.step_accepted = true;
        result.suggested_dt = (adaptive_suggest_dt > 0.0) ? adaptive_suggest_dt : dt;
        result.estimated_error = std::abs(a) * dt * dt * 0.5;
        return result;
    }

    base::Result<void> post_step(double t, double dt) override {
        (void)t;
        (void)dt;
        post_step_count++;
        return {};
    }

    base::Result<void> reset() override {
        y = 1.0;
        v = 0.0;
        pre_step_count = 0;
        step_count = 0;
        post_step_count = 0;
        return {};
    }

    [[nodiscard]] std::string serialize_state() const override {
        return "{\"y\":" + std::to_string(y) + ",\"v\":" + std::to_string(v) + "}";
    }

    base::Result<void> restore_state(std::string_view state_json) override {
        std::string s(state_json);
        std::regex y_re(R"re("y"\s*:\s*([-+]?[0-9]*\.?[0-9]+))re");
        std::regex v_re(R"re("v"\s*:\s*([-+]?[0-9]*\.?[0-9]+))re");
        std::smatch m;
        if (std::regex_search(s, m, y_re)) y = std::stod(m[1].str());
        if (std::regex_search(s, m, v_re)) v = std::stod(m[1].str());
        return {};
    }

    std::string name_;
    int order_{0};
    double freq_{1.0};
    double y{1.0};
    double v{0.0};
    bool init_called{false};
    uint64_t pre_step_count{0};
    uint64_t step_count{0};
    uint64_t post_step_count{0};
    bool fail_on_next_step{false};
    bool reject_on_next_step{false};
    double adaptive_suggest_dt{0.0};
};

TEST_CASE("SIM-01: Simulation Lifecycle States and Transitions") {
    // State string conversions
    CHECK(simulation_state_to_string(SimulationState::Stopped) == "Stopped");
    CHECK(simulation_state_to_string(SimulationState::Configuring) == "Configuring");
    CHECK(simulation_state_to_string(SimulationState::Running) == "Running");
    CHECK(simulation_state_to_string(SimulationState::Paused) == "Paused");
    CHECK(simulation_state_to_string(SimulationState::Stepping) == "Stepping");
    CHECK(simulation_state_to_string(SimulationState::Completed) == "Completed");
    CHECK(simulation_state_to_string(SimulationState::Failed) == "Failed");
    CHECK(simulation_state_to_string(SimulationState::Cancelled) == "Cancelled");

    CHECK(simulation_state_from_string("Running").value() == SimulationState::Running);
    CHECK(simulation_state_from_string("Cancelled").value() == SimulationState::Cancelled);
    CHECK_FALSE(simulation_state_from_string("NonExistent").has_value());

    // Active and terminal classifications
    CHECK(is_simulation_active(SimulationState::Running));
    CHECK(is_simulation_active(SimulationState::Paused));
    CHECK(is_simulation_active(SimulationState::Stepping));
    CHECK_FALSE(is_simulation_active(SimulationState::Stopped));
    CHECK_FALSE(is_simulation_active(SimulationState::Completed));

    CHECK(is_simulation_terminal(SimulationState::Completed));
    CHECK(is_simulation_terminal(SimulationState::Failed));
    CHECK(is_simulation_terminal(SimulationState::Cancelled));
    CHECK(is_simulation_terminal(SimulationState::Stopped));
    CHECK_FALSE(is_simulation_terminal(SimulationState::Running));

    // State transition rules
    CHECK(can_transition(SimulationState::Stopped, SimulationState::Configuring));
    CHECK(can_transition(SimulationState::Stopped, SimulationState::Running));
    CHECK_FALSE(can_transition(SimulationState::Stopped, SimulationState::Paused));

    CHECK(can_transition(SimulationState::Running, SimulationState::Paused));
    CHECK(can_transition(SimulationState::Running, SimulationState::Completed));
    CHECK(can_transition(SimulationState::Running, SimulationState::Failed));
    CHECK(can_transition(SimulationState::Running, SimulationState::Cancelled));

    CHECK(can_transition(SimulationState::Paused, SimulationState::Running));
    CHECK(can_transition(SimulationState::Paused, SimulationState::Stopped));

    CHECK(can_transition(SimulationState::Completed, SimulationState::Stopped));
    CHECK_FALSE(can_transition(SimulationState::Completed, SimulationState::Running));
}

TEST_CASE("SIM-02: SimulationConfig Validation and JSON Serialization") {
    // Nominal configuration passes validation
    {
        SimulationConfig cfg;
        cfg.start_time = 0.0;
        cfg.end_time = 5.0;
        cfg.initial_time_step = 0.01;
        cfg.min_time_step = 1e-6;
        cfg.max_time_step = 0.1;
        cfg.relative_tolerance = 1e-4;
        cfg.absolute_tolerance = 1e-6;

        auto report = cfg.validate();
        CHECK(report.is_valid());
    }

    // Invalid configuration triggers diagnostics
    {
        SimulationConfig cfg;
        cfg.start_time = 10.0;
        cfg.end_time = 5.0; // Error: end <= start
        cfg.initial_time_step = -0.1; // Error: non-positive dt
        cfg.min_time_step = 0.05;
        cfg.max_time_step = 0.01; // Error: max < min
        cfg.relative_tolerance = -1.0;

        auto report = cfg.validate();
        CHECK_FALSE(report.is_valid());
        CHECK(report.error_count() >= 4);
    }

    // JSON serialization and deserialization roundtrip
    {
        SimulationConfig original;
        original.start_time = 1.0;
        original.end_time = 25.0;
        original.time_step_policy = TimeStepPolicy::Adaptive;
        original.initial_time_step = 0.005;
        original.min_time_step = 1e-5;
        original.max_time_step = 0.05;
        original.solver_type = SolverType::AdaptiveRK45;
        original.relative_tolerance = 1e-5;
        original.absolute_tolerance = 1e-7;
        original.random_seed = 999;
        original.output_sampling_interval = 0.02;
        original.max_steps = 50000;

        std::string json = original.to_json();
        auto restored_res = SimulationConfig::from_json(json);
        REQUIRE(restored_res.is_ok());

        const auto& restored = restored_res.value();
        CHECK(restored.start_time == 1.0);
        CHECK(restored.end_time == 25.0);
        CHECK(restored.time_step_policy == TimeStepPolicy::Adaptive);
        CHECK(std::abs(restored.initial_time_step - 0.005) < 1e-9);
        CHECK(restored.solver_type == SolverType::AdaptiveRK45);
        CHECK(restored.random_seed == 999);
        CHECK(restored.max_steps == 50000);
    }
}

TEST_CASE("SIM-03: SimulationClock Decoupling and Metrics") {
    SimulationClock clock(0.0);
    CHECK(clock.physical_time() == 0.0);
    CHECK(clock.step_count() == 0);
    CHECK_FALSE(clock.is_wall_clock_running());

    clock.start_wall_clock();
    CHECK(clock.is_wall_clock_running());

    clock.advance(0.1);
    clock.advance(0.1);
    clock.advance(0.05);

    CHECK(std::abs(clock.physical_time() - 0.25) < 1e-9);
    CHECK(clock.step_count() == 3);

    // Pause and resume
    clock.pause_wall_clock();
    CHECK_FALSE(clock.is_wall_clock_running());
    const double paused_time = clock.elapsed_wall_time_seconds();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(std::abs(clock.elapsed_wall_time_seconds() - paused_time) < 0.005);

    clock.resume_wall_clock();
    CHECK(clock.is_wall_clock_running());

    // Reset clock
    clock.reset(5.0);
    CHECK(clock.physical_time() == 5.0);
    CHECK(clock.step_count() == 0);
}

TEST_CASE("SIM-04: Deterministic Participant Coordination Protocol") {
    SimulationRuntime runtime;
    SimulationConfig cfg;
    cfg.start_time = 0.0;
    cfg.end_time = 1.0;
    cfg.initial_time_step = 0.1;
    REQUIRE(runtime.configure(cfg).is_ok());

    // Add participants out of order
    auto p2 = std::make_shared<MockOscillatorParticipant>("EngineB", 20, 1.0);
    auto p1 = std::make_shared<MockOscillatorParticipant>("EngineA", 10, 1.0);
    auto p3 = std::make_shared<MockOscillatorParticipant>("EngineC", 30, 1.0);

    REQUIRE(runtime.add_participant(p2).is_ok());
    REQUIRE(runtime.add_participant(p1).is_ok());
    REQUIRE(runtime.add_participant(p3).is_ok());

    // Duplicate key rejection
    auto p_dup = std::make_shared<MockOscillatorParticipant>("EngineA", 5, 1.0);
    CHECK_FALSE(runtime.add_participant(p_dup).is_ok());

    // Deterministic sorting check: execution order 10 < 20 < 30
    const auto& participants = runtime.participants();
    REQUIRE(participants.size() == 3);
    CHECK(participants[0]->name() == "EngineA");
    CHECK(participants[1]->name() == "EngineB");
    CHECK(participants[2]->name() == "EngineC");

    // Single step verification
    REQUIRE(runtime.start().is_ok());
    REQUIRE(runtime.step().is_ok());

    CHECK(p1->pre_step_count == 1);
    CHECK(p1->step_count == 1);
    CHECK(p1->post_step_count == 1);
}

TEST_CASE("SIM-05: Fixed & Adaptive Time Steps and Rejection Diagnostics") {
    // Fixed time stepping maintains uniform dt
    {
        SimulationRuntime runtime;
        SimulationConfig cfg;
        cfg.start_time = 0.0;
        cfg.end_time = 0.5;
        cfg.initial_time_step = 0.1;
        cfg.time_step_policy = TimeStepPolicy::Fixed;
        REQUIRE(runtime.configure(cfg).is_ok());

        auto p = std::make_shared<MockOscillatorParticipant>("UniformEngine", 0, 1.0);
        REQUIRE(runtime.add_participant(p).is_ok());

        REQUIRE(runtime.run_all().is_ok());
        CHECK(runtime.state() == SimulationState::Completed);
        CHECK(runtime.clock().step_count() == 5);
        CHECK(runtime.diagnostics().steps_accepted == 5);
        CHECK(runtime.diagnostics().steps_rejected == 0);
        CHECK(std::abs(runtime.diagnostics().acceptance_ratio() - 1.0) < 1e-9);
    }

    // Adaptive step rejection recovers cleanly
    {
        SimulationRuntime runtime;
        SimulationConfig cfg;
        cfg.start_time = 0.0;
        cfg.end_time = 1.0;
        cfg.initial_time_step = 0.1;
        cfg.min_time_step = 0.01;
        cfg.time_step_policy = TimeStepPolicy::Adaptive;
        REQUIRE(runtime.configure(cfg).is_ok());

        auto p = std::make_shared<MockOscillatorParticipant>("RejectingEngine", 0, 1.0);
        p->reject_on_next_step = true;
        REQUIRE(runtime.add_participant(p).is_ok());

        REQUIRE(runtime.start().is_ok());
        REQUIRE(runtime.step().is_ok());

        CHECK(runtime.diagnostics().steps_rejected == 1);
        CHECK(runtime.diagnostics().steps_accepted == 1);
        CHECK(runtime.diagnostics().last_rejection_reason.find("Oscillator local truncation") != std::string::npos);
    }
}

TEST_CASE("SIM-06: Control Operations and Checkpoint/Restart") {
    SimulationRuntime runtime;
    SimulationConfig cfg;
    cfg.start_time = 0.0;
    cfg.end_time = 10.0;
    cfg.initial_time_step = 0.1;
    REQUIRE(runtime.configure(cfg).is_ok());

    auto p = std::make_shared<MockOscillatorParticipant>("StatefulEngine", 0, 0.5);
    REQUIRE(runtime.add_participant(p).is_ok());

    REQUIRE(runtime.start().is_ok());
    CHECK(runtime.state() == SimulationState::Running);

    // Advance 5 steps
    for (int i = 0; i < 5; ++i) {
        REQUIRE(runtime.step().is_ok());
    }
    CHECK(runtime.clock().step_count() == 5);

    // Pause simulation
    REQUIRE(runtime.pause().is_ok());
    CHECK(runtime.state() == SimulationState::Paused);

    // Create Checkpoint
    auto cp_res = runtime.create_checkpoint();
    REQUIRE(cp_res.is_ok());
    const auto cp = cp_res.value();
    CHECK(cp.step_count == 5);
    CHECK(std::abs(cp.physical_time - 0.5) < 1e-9);
    CHECK(cp.participant_states.find("StatefulEngine") != cp.participant_states.end());

    const double saved_y = p->y;
    const double saved_v = p->v;

    // Advance 5 more steps
    REQUIRE(runtime.resume().is_ok());
    for (int i = 0; i < 5; ++i) {
        REQUIRE(runtime.step().is_ok());
    }
    CHECK(runtime.clock().step_count() == 10);
    CHECK(p->y != saved_y);

    // Restore checkpoint
    REQUIRE(runtime.restore_checkpoint(cp).is_ok());
    CHECK(runtime.clock().step_count() == 5);
    CHECK(std::abs(runtime.clock().physical_time() - 0.5) < 1e-9);
    CHECK(std::abs(p->y - saved_y) < 1e-6);
    CHECK(std::abs(p->v - saved_v) < 1e-6);

    // Checkpoint JSON roundtrip
    std::string cp_json = cp.to_json();
    auto cp_restored_res = SimulationCheckpoint::from_json(cp_json);
    REQUIRE(cp_restored_res.is_ok());
    CHECK(cp_restored_res.value().step_count == 5);
    CHECK(std::abs(cp_restored_res.value().physical_time - 0.5) < 1e-9);
}

TEST_CASE("SIM-07: Discrete Event Scheduler and Slicing") {
    SimulationRuntime runtime;
    SimulationConfig cfg;
    cfg.start_time = 0.0;
    cfg.end_time = 1.0;
    cfg.initial_time_step = 0.2; // normally steps: 0.0, 0.2, 0.4, 0.6...
    cfg.max_time_step = 0.5;
    REQUIRE(runtime.configure(cfg).is_ok());

    int event_call_count = 0;
    double event_received_time = 0.0;

    // Schedule an event at t = 0.15 (between step 0 and 0.2)
    SimulationEvent ev;
    ev.name = "MidStepSwitch";
    ev.scheduled_time = 0.15;
    ev.handler = [&](double t) -> base::Result<void> {
        event_call_count++;
        event_received_time = t;
        return {};
    };
    runtime.event_queue().schedule_event(ev);

    REQUIRE(runtime.start().is_ok());
    // Stepping should slice dt to land precisely on 0.15
    REQUIRE(runtime.step().is_ok());
    CHECK(std::abs(runtime.clock().physical_time() - 0.15) < 1e-9);
    CHECK(event_call_count == 1);
    CHECK(std::abs(event_received_time - 0.15) < 1e-9);

    // Recurring event test
    int recurring_hits = 0;
    SimulationEvent recur;
    recur.name = "RecurringPulse";
    recur.scheduled_time = 0.2;
    recur.is_recurring = true;
    recur.recurrence_interval = 0.2; // at 0.2, 0.4, 0.6, 0.8, 1.0
    recur.handler = [&](double) -> base::Result<void> {
        recurring_hits++;
        return {};
    };
    runtime.event_queue().schedule_event(recur);

    REQUIRE(runtime.run_all().is_ok());
    CHECK(runtime.state() == SimulationState::Completed);
    CHECK(recurring_hits >= 4);
}

TEST_CASE("SIM-08: Run Isolation Safeguards") {
    auto prj_res = Project::create_new("MasterDesignProject", "LeadEngineer");
    REQUIRE(prj_res.is_ok());
    auto prj = prj_res.value();

    SimulationRuntime runtime;
    REQUIRE(runtime.isolate_project(*prj).is_ok());

    const auto* iso_env = runtime.isolated_environment();
    REQUIRE(iso_env != nullptr);
    CHECK(iso_env->project_name() == "MasterDesignProject");
    CHECK_FALSE(iso_env->content_hash().empty());

    // Mutate the original project manifest
    prj->manifest().name = "ModifiedAfterRunLaunch";
    prj->manifest().author = "AnotherUser";

    // Verify isolated snapshot remains completely unaffected
    CHECK(iso_env->project_name() == "MasterDesignProject");
}

TEST_CASE("SIM-09: Run Manifest Generation and Hash Integrity") {
    SimulationRuntime runtime;
    SimulationConfig cfg;
    cfg.start_time = 0.0;
    cfg.end_time = 0.3;
    cfg.initial_time_step = 0.1;
    REQUIRE(runtime.configure(cfg).is_ok());

    auto p = std::make_shared<MockOscillatorParticipant>("ManifestParticipant", 0, 1.0);
    REQUIRE(runtime.add_participant(p).is_ok());
    REQUIRE(runtime.run_all().is_ok());

    RunManifest manifest = runtime.generate_run_manifest();
    CHECK(manifest.final_state == SimulationState::Completed);
    CHECK(manifest.diagnostics.steps_accepted == 3);
    CHECK_FALSE(manifest.compute_manifest_hash().empty());
    CHECK_FALSE(manifest.host_os.empty());
    CHECK_FALSE(manifest.compiler_info.empty());

    std::string json = manifest.to_json();
    auto restored_res = RunManifest::from_json(json);
    REQUIRE(restored_res.is_ok());
    CHECK(restored_res.value().final_state == SimulationState::Completed);
    CHECK(restored_res.value().diagnostics.steps_accepted == 3);
}

TEST_CASE("SIM-10: Cancellation, Progress Estimation and Graceful Error Recovery") {
    // Cooperative cancellation terminates cleanly
    {
        SimulationRuntime runtime;
        SimulationConfig cfg;
        cfg.start_time = 0.0;
        cfg.end_time = 100.0;
        cfg.initial_time_step = 0.1;
        REQUIRE(runtime.configure(cfg).is_ok());

        auto p = std::make_shared<MockOscillatorParticipant>("LongRunningEngine", 0, 1.0);
        REQUIRE(runtime.add_participant(p).is_ok());

        REQUIRE(runtime.start().is_ok());
        for (int i = 0; i < 5; ++i) {
            REQUIRE(runtime.step().is_ok());
        }

        // Cancel simulation
        runtime.cancel();
        auto step_res = runtime.step();
        CHECK_FALSE(step_res.is_ok());
        CHECK(runtime.state() == SimulationState::Cancelled);
    }

    // Graceful error recovery transitions to Failed without memory leak
    {
        SimulationRuntime runtime;
        SimulationConfig cfg;
        cfg.start_time = 0.0;
        cfg.end_time = 10.0;
        cfg.initial_time_step = 0.1;
        REQUIRE(runtime.configure(cfg).is_ok());

        auto p = std::make_shared<MockOscillatorParticipant>("FaultyEngine", 0, 1.0);
        REQUIRE(runtime.add_participant(p).is_ok());

        REQUIRE(runtime.start().is_ok());
        REQUIRE(runtime.step().is_ok());

        // Trigger synthetic fault
        p->fail_on_next_step = true;
        auto fail_step_res = runtime.step();
        CHECK_FALSE(fail_step_res.is_ok());
        CHECK(runtime.state() == SimulationState::Failed);
        CHECK(runtime.last_error().find("Synthetic oscillator failure") != std::string::npos);

        // Reset runtime returns to Stopped
        REQUIRE(runtime.reset().is_ok());
        CHECK(runtime.state() == SimulationState::Stopped);
    }

    // Progress ratio and telemetry metrics
    {
        SimulationRuntime runtime;
        SimulationConfig cfg;
        cfg.start_time = 0.0;
        cfg.end_time = 10.0;
        cfg.initial_time_step = 1.0;
        cfg.max_time_step = 2.0;
        REQUIRE(runtime.configure(cfg).is_ok());

        auto p = std::make_shared<MockOscillatorParticipant>("TelemetryEngine", 0, 1.0);
        REQUIRE(runtime.add_participant(p).is_ok());

        REQUIRE(runtime.start().is_ok());
        CHECK(runtime.progress_ratio() == 0.0);

        REQUIRE(runtime.step().is_ok()); // t = 1.0
        CHECK(std::abs(runtime.progress_ratio() - 0.1) < 1e-6);

        REQUIRE(runtime.step().is_ok()); // t = 2.0
        CHECK(std::abs(runtime.progress_ratio() - 0.2) < 1e-6);
    }
}
