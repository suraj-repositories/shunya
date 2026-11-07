#include "tests/test_framework.hpp"
#include "shunya/core/domain/scripting.hpp"

#include <cmath>
#include <string>

using namespace shunya::core::domain::scripting;
using namespace shunya::core::base::math;

// =============================================================================
// CODE-01: Rule Types & Closed-Loop PID Controller
// =============================================================================

TEST_CASE("CODE-01: Closed-Loop PID Controller Tracking and Anti-Windup") {
    PIDConfig cfg;
    cfg.kp = 2.0;
    cfg.ki = 0.5;
    cfg.kd = 0.1;
    cfg.setpoint = 100.0;
    cfg.output_min = -50.0;
    cfg.output_max = 50.0;
    cfg.integral_min = -20.0;
    cfg.integral_max = 20.0;

    PIDController pid(cfg);
    CHECK(approx_equal(pid.setpoint(), 100.0));

    // Step with measured = 80 -> error = 20
    // P = 2.0 * 20 = 40
    // I = 0.5 * (20 * 0.1) = 1.0
    // D = 0 (first step)
    // output = 40 + 1 = 41
    double u1 = pid.update(80.0, 0.1);
    CHECK(approx_equal(u1, 41.0, 0.1));
    CHECK(approx_equal(pid.last_error(), 20.0));

    // Next step with measured = 90 -> error = 10
    // P = 2.0 * 10 = 20
    // I = 0.5 * (2.0 + 1.0) = 1.5
    // D = 0.1 * ((10 - 20) / 0.1) = -10.0
    // output = 20 + 1.5 - 10 = 11.5
    double u2 = pid.update(90.0, 0.1);
    CHECK(approx_equal(u2, 11.5, 0.2));

    pid.reset();
    CHECK(approx_equal(pid.integral_state(), 0.0));
}

// =============================================================================
// CODE-02: Typed Host API for Sandboxed Execution
// =============================================================================

TEST_CASE("CODE-02: Host Sandbox API State Reading, Control Writing and Events") {
    ScriptHostContext ctx;
    ctx.state_registers["temp_sensor"] = 298.15;
    ctx.state_registers["pump_active"] = true;
    ctx.simulation_time = 12.5;

    SandboxLimits limits;
    limits.granted_capabilities = ScriptCapability::ReadState |
                                  ScriptCapability::WriteControl |
                                  ScriptCapability::EmitEvents |
                                  ScriptCapability::AccessTime;

    HostSandboxAPI api(ctx, limits);

    // Read state
    auto r1 = api.read_state("temp_sensor");
    REQUIRE(r1.is_ok());
    CHECK(std::get<double>(r1.value()) == 298.15);

    // Write control
    auto w1 = api.write_control("heater_power", 500.0);
    REQUIRE(w1.is_ok());
    CHECK(std::get<double>(ctx.control_registers["heater_power"]) == 500.0);

    // Emit event
    auto e1 = api.emit_event("THRESHOLD_ALARM", 105.0);
    REQUIRE(e1.is_ok());
    CHECK(ctx.emitted_events.size() == 1u);
    CHECK(ctx.emitted_events[0].first == "THRESHOLD_ALARM");

    // Simulation time
    auto t1 = api.get_simulation_time();
    REQUIRE(t1.is_ok());
    CHECK(t1.value() == 12.5);
}

// =============================================================================
// CODE-03 & CODE-04: Capability Permissions & Sandboxing
// =============================================================================

TEST_CASE("CODE-04: Security Sandbox Capability Enforcement") {
    ScriptHostContext ctx;
    ctx.state_registers["sensor1"] = 1.0;

    // Restricted limits: ReadState only, NO WriteControl permission
    SandboxLimits limits;
    limits.granted_capabilities = ScriptCapability::ReadState;

    HostSandboxAPI api(ctx, limits);

    auto read_res = api.read_state("sensor1");
    CHECK(read_res.is_ok());

    // Attempt unauthorized control write: must fail with error code 403
    auto write_res = api.write_control("valve_pos", 1.0);
    CHECK(write_res.is_err());
    CHECK(write_res.error().code == 403);
}

// =============================================================================
// CODE-05: Deterministic PRNG
// =============================================================================

TEST_CASE("CODE-05: Deterministic Seeded PRNG Reproducibility") {
    DeterministicPRNG rng1(123456789ULL);
    DeterministicPRNG rng2(123456789ULL);

    for (int i = 0; i < 50; ++i) {
        const double d1 = rng1.next_double();
        const double d2 = rng2.next_double();
        CHECK(d1 == d2);
        CHECK(d1 >= 0.0);
        CHECK(d1 < 1.0);
    }

    // Gaussian test
    DeterministicPRNG rng3(42);
    const double g = rng3.next_gaussian(10.0, 2.0);
    CHECK(std::isfinite(g));
}

// =============================================================================
// CODE-06 & CODE-10: Script Lifecycle and Breakpoint Debugging
// =============================================================================

TEST_CASE("CODE-06 & CODE-10: Script Lifecycle and Breakpoints") {
    const std::string script_code =
        "# Temperature regulation script\n"
        "SET heater = 250.0 + 50.0\n"
        "EMIT ALARM_TRIGGER 1.0\n";

    SandboxLimits limits;
    limits.granted_capabilities = ScriptCapability::ReadState |
                                  ScriptCapability::WriteControl |
                                  ScriptCapability::EmitEvents |
                                  ScriptCapability::AccessTime;

    ScriptEngine engine("heater_script", script_code, limits);
    CHECK(engine.state() == ScriptLifecycleState::Uncompiled);

    auto comp_res = engine.compile();
    REQUIRE(comp_res.is_ok());
    CHECK(engine.state() == ScriptLifecycleState::Ready);

    ScriptHostContext ctx;
    auto init_res = engine.init(ctx);
    REQUIRE(init_res.is_ok());
    CHECK(engine.state() == ScriptLifecycleState::Attached);

    // Execute step
    auto exec_res = engine.execute_step(ctx);
    REQUIRE(exec_res.is_ok());
    CHECK(std::get<double>(ctx.control_registers["heater"]) == 300.0);
    CHECK(ctx.emitted_events.size() == 1u);

    // Breakpoint test: add breakpoint on line 2
    engine.add_breakpoint(2);
    CHECK(engine.has_breakpoint(2));

    auto bp_step = engine.execute_step(ctx);
    CHECK(bp_step.is_ok());
    CHECK(engine.state() == ScriptLifecycleState::Paused);

    engine.remove_breakpoint(2);
    CHECK_FALSE(engine.has_breakpoint(2));
}

// =============================================================================
// CODE-07: Code Editor Model & Tokenization
// =============================================================================

TEST_CASE("CODE-07: Code Editor Syntax Highlighting and Autocomplete") {
    std::string sample = "SET valve = sin(1.57) # control valve";
    auto tokens = CodeEditorModel::tokenize(sample);

    REQUIRE(tokens.size() >= 5u);
    CHECK(tokens[0].type == TokenType::Keyword);
    CHECK(tokens[0].text == "SET");
    CHECK(tokens[1].type == TokenType::Identifier);
    CHECK(tokens[1].text == "valve");
    CHECK(tokens[2].type == TokenType::Operator);
    CHECK(tokens[2].text == "=");
    CHECK(tokens[3].type == TokenType::Identifier);
    CHECK(tokens[3].text == "sin");

    // Autocomplete
    auto suggestions = CodeEditorModel::get_suggestions("s");
    CHECK(!suggestions.empty());
    bool found_sin = false;
    for (const auto& s : suggestions) {
        if (s.label == "sin") found_sin = true;
    }
    CHECK(found_sin);
}

// =============================================================================
// CODE-08: Visual Rule Builder
// =============================================================================

TEST_CASE("CODE-08: Visual Rule Graph Block Compilation") {
    VisualRuleGraph graph;
    graph.add_block(VisualRuleBlock{"b1", VisualBlockType::ReadState, "temp_reading", {}});
    graph.add_block(VisualRuleBlock{"b2", VisualBlockType::ConstantNumber, "15.0", {}});

    std::string formula = graph.compile_to_formula();
    CHECK(formula == "temp_reading + 15.0");
}

// =============================================================================
// CODE-09: Safe Formula Parser & Expression Evaluator
// =============================================================================

TEST_CASE("CODE-09: Formula Parser and Function Evaluation") {
    std::unordered_map<std::string, double> vars = {
        {"x", 10.0},
        {"y", 2.0}
    };

    // 1. Basic arithmetic and precedence: 10 + 2 * 3 = 16
    auto r1 = FormulaParser::evaluate("x + y * 3", vars);
    REQUIRE(r1.is_ok());
    CHECK(approx_equal(r1.value(), 16.0));

    // 2. Parentheses: (10 + 2) * 3 = 36
    auto r2 = FormulaParser::evaluate("(x + y) * 3", vars);
    REQUIRE(r2.is_ok());
    CHECK(approx_equal(r2.value(), 36.0));

    // 3. Exponentiation: 10 ^ 2 = 100
    auto r3 = FormulaParser::evaluate("x ^ y", vars);
    REQUIRE(r3.is_ok());
    CHECK(approx_equal(r3.value(), 100.0));

    // 4. Mathematical function: sqrt(100) = 10
    auto r4 = FormulaParser::evaluate("sqrt(100.0)");
    REQUIRE(r4.is_ok());
    CHECK(approx_equal(r4.value(), 10.0));

    // 5. Unary minus: -x + 5 = -5
    auto r5 = FormulaParser::evaluate("-x + 5.0", vars);
    REQUIRE(r5.is_ok());
    CHECK(approx_equal(r5.value(), -5.0));
}

// =============================================================================
// CODE-11: Script Package Versioning & Provenance
// =============================================================================

TEST_CASE("CODE-11: Script Package Metadata and Checksum") {
    ScriptPackageMetadata pkg;
    pkg.package_name = "BatteryManager";
    pkg.version = "1.2.0";
    pkg.author = "Laboratory Safety Team";

    const std::string content = "SET charger = 1.0";
    pkg.checksum_fnv1a = ScriptPackageMetadata::compute_checksum(content);

    CHECK(pkg.checksum_fnv1a != 0u);
    CHECK(pkg.version == "1.2.0");

    // Identical content must produce identical checksum
    CHECK(pkg.checksum_fnv1a == ScriptPackageMetadata::compute_checksum(content));
}

// =============================================================================
// CODE-12: Robustness, Security, Timeouts & Error Recovery
// =============================================================================

TEST_CASE("CODE-12: Division by Zero and Instruction Limit Timeout") {
    // 1. Division by zero must be rejected with an error, not crashing
    auto div_zero = FormulaParser::evaluate("10.0 / 0.0");
    CHECK(div_zero.is_err());

    // 2. Infinite loop / instruction limit timeout
    auto timeout_res = FormulaParser::evaluate("1 + 2 + 3 + 4 + 5", {}, 2); // limit of 2 ops
    CHECK(timeout_res.is_err());
    CHECK(timeout_res.error().code == 408); // timeout code

    // 3. Sqrt of negative number domain error
    auto domain_err = FormulaParser::evaluate("sqrt(-25.0)");
    CHECK(domain_err.is_err());
}
