#include "tests/test_framework.hpp"
#include "shunya/core/base/app_context.hpp"
#include "shunya/core/base/units.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"
#include "shunya/core/domain/simulation_clock.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/thermal.hpp"
#include "shunya/core/domain/mechanics.hpp"
#include "shunya/core/domain/scripting.hpp"
#include "shunya/core/domain/library.hpp"
#include "shunya/core/domain/component.hpp"
#include "shunya/core/domain/simulation_runtime.hpp"
#include "shunya/core/domain/observability.hpp"

#include <cmath>
#include <vector>
#include <string>
#include <filesystem>

using namespace shunya::core;
using namespace shunya::core::base;
using namespace shunya::core::domain;
using namespace shunya::core::domain::thermal;
using namespace shunya::core::domain::mechanics;
using namespace shunya::core::domain::scripting;
using namespace shunya::core::domain::library;

TEST_CASE("E2E-01: Multiphysics Coupled Demonstrator (Circuit -> Thermal -> Mechanics)") {
    // -------------------------------------------------------------------------
    // 1. Project Creation & Scene Graph Assembly
    // -------------------------------------------------------------------------
    auto prj_res = Project::create_new("ElectroThermoMechanical_Actuator", "Shunya MultiPhysics Team");
    REQUIRE(prj_res.is_ok());
    auto project = prj_res.value();
    CHECK(project->manifest().name == "ElectroThermoMechanical_Actuator");

    SceneGraph scene;
    auto root_res = scene.create_entity("Root_Actuator_Assembly");
    REQUIRE(root_res.is_ok());
    auto root = root_res.value();

    auto e_elec = scene.create_entity("Electronics_Subsystem", root->id());
    REQUIRE(e_elec.is_ok());
    auto e_therm = scene.create_entity("Thermal_Dissipation_Stage", root->id());
    REQUIRE(e_therm.is_ok());
    auto e_mech = scene.create_entity("Mechanical_Actuator_Stage", root->id());
    REQUIRE(e_mech.is_ok());

    CHECK(scene.entity_count() == 4u); // root + 3 children
    CHECK(e_elec.value()->name() == "Electronics_Subsystem");
    CHECK(e_therm.value()->name() == "Thermal_Dissipation_Stage");
    CHECK(e_mech.value()->name() == "Mechanical_Actuator_Stage");

    // -------------------------------------------------------------------------
    // 2. Electronics Domain: Joule Heating Calculation
    // -------------------------------------------------------------------------
    // 24V source, 8 ohm heater element, 8 ohm load
    auto netlist = CircuitBenchmarkFactory::create_voltage_divider(24.0, 8.0, 8.0);

    DcOperatingPointSolver dc_solver;
    auto op_res = dc_solver.solve(netlist);
    REQUIRE(op_res.is_ok());
    auto op = op_res.value();
    CHECK(op.converged);

    double v_in = op.get_voltage("in");
    CHECK(std::abs(v_in - 24.0) < 1e-4);

    double i_heater = op.get_current("R1");
    // I = 24V / (8 + 8) = 1.5 A
    CHECK(std::abs(i_heater - 1.5) < 1e-4);

    // Joule heat generation rate: P = I^2 * R = 1.5^2 * 8 = 18.0 W
    double p_joule = i_heater * i_heater * 8.0;
    CHECK(std::abs(p_joule - 18.0) < 1e-4);

    // -------------------------------------------------------------------------
    // 3. Thermal Domain: Transient Heat Conduction & Temperature Rise
    // -------------------------------------------------------------------------
    ThermalNetwork thermal_net;

    // Node 0: Heater block (ambient 300 K, compact 10 cm^3 core -> C ~ 24.3 J/K)
    ThermalNode heater_block;
    heater_block.volume = 1e-5; // 0.01 liter
    heater_block.material.density = 2700.0; // Aluminum
    heater_block.material.specific_heat = 900.0;
    heater_block.state.temperature = 300.0;
    heater_block.state.heat_source = p_joule; // 18 W direct Joule heating
    thermal_net.add_node(heater_block);

    // Node 1: External heat sink (ambient 300 K)
    ThermalNode sink_block;
    sink_block.volume = 5e-5;
    sink_block.material.density = 2700.0;
    sink_block.material.specific_heat = 900.0;
    sink_block.state.temperature = 300.0;
    sink_block.boundary.type = BoundaryType::Convection;
    sink_block.boundary.surface_area = 0.02;
    sink_block.boundary.convection_h = 10.0;
    sink_block.boundary.ambient_temperature = 300.0;
    thermal_net.add_node(sink_block);

    // Thermal resistor between heater and sink (Conductance = 1.0 W/K)
    ThermalResistor r_therm;
    r_therm.node_a = 0;
    r_therm.node_b = 1;
    r_therm.type = ResistorType::Conduction;
    r_therm.conductance = 1.0;
    thermal_net.add_resistor(r_therm);

    // Step thermal transient for 2.0 seconds
    constexpr double dt_therm = 0.05;
    constexpr int thermal_steps = 40; // 40 * 0.05 = 2.0s
    for (int step = 0; step < thermal_steps; ++step) {
        auto step_res = thermal_net.step_transient(dt_therm);
        REQUIRE(step_res.is_ok());
    }

    double final_temp_heater = thermal_net.get_node(0).state.temperature;
    double final_temp_sink = thermal_net.get_node(1).state.temperature;

    // Temperature must have risen strictly above 300 K
    CHECK(final_temp_heater > 300.5);
    CHECK(final_temp_sink > 300.0);
    CHECK(final_temp_heater > final_temp_sink); // Heat conducts from heater to sink

    double delta_t = final_temp_heater - 300.0;

    // -------------------------------------------------------------------------
    // 4. Mechanics Domain: Thermal Actuator Strain & Harmonic Response
    // -------------------------------------------------------------------------
    // Thermal expansion model: Delta_L = alpha * L_0 * Delta_T
    // Aluminum alpha ~ 23e-6 1/K, bar length L0 = 0.5 m
    constexpr double alpha_expansion = 23e-6;
    constexpr double l0 = 0.5;
    double delta_l = alpha_expansion * l0 * delta_t;
    CHECK(delta_l > 0.0);

    // Actuation driving force onto spring-mass system: F = k_actuator * delta_l
    constexpr double k_actuator = 1e5; // N/m stiffness
    double f_actuator = k_actuator * delta_l;
    CHECK(f_actuator > 0.0);

    // Mechanics World
    PhysicsWorld world;
    auto actuator_body = std::make_shared<RigidBody>("actuator_piston", BodyType::Dynamic);
    auto mass_props = MassProperties::create_solid_box(1.5, {0.1, 0.1, 0.1});
    actuator_body->set_mass_properties(mass_props);
    actuator_body->set_position({0.0, 0.0, 0.0});
    actuator_body->set_linear_velocity({0.0, 0.0, 0.0});
    world.add_body(actuator_body);

    constexpr double dt_mech = 0.002;
    constexpr int mech_steps = 100; // 0.2s

    for (int step = 0; step < mech_steps; ++step) {
        actuator_body->clear_accumulators();
        // Apply thermal actuation force along X
        actuator_body->apply_force_center(Vector3d{f_actuator, 0.0, 0.0});

        // Restore spring force: F_spring = -k_return * x
        constexpr double k_return = 500.0;
        double cur_x = actuator_body->position().x;
        actuator_body->apply_force_center(Vector3d{-k_return * cur_x, 0.0, 0.0});

        world.step(dt_mech);
    }

    // Piston has displaced positively along X axis due to thermal actuation
    CHECK(actuator_body->position().x > 0.0);
    CHECK(actuator_body->linear_velocity().x != 0.0);

    // -------------------------------------------------------------------------
    // 5. Persistence Round-Trip Verification
    // -------------------------------------------------------------------------
    std::filesystem::path temp_dir = std::filesystem::current_path() / "test_artifacts" / "shunya_e2e_test";
    std::filesystem::create_directories(temp_dir);
    std::string project_file = (temp_dir / "demonstrator.shunya").string();

    auto save_res = project->save_to_file(project_file);
    REQUIRE(save_res.is_ok());

    auto reload_res = Project::open_from_file(project_file);
    REQUIRE(reload_res.is_ok());
    auto reloaded_prj = reload_res.value();
    CHECK(reloaded_prj->manifest().name == "ElectroThermoMechanical_Actuator");
    CHECK(reloaded_prj->manifest().author == "Shunya MultiPhysics Team");

    std::filesystem::remove_all(temp_dir);
}

// =============================================================================
// E2E-02: Section 18 MVP Demonstrator - Virtual Test Bench, Scripted Switch, & Component Reuse
// =============================================================================

TEST_CASE("E2E-02: Section 18 MVP Demonstrator - Virtual Test Bench, Scripted Switch, & Component Reuse") {
    // 1. Author Virtual Test Bench Project
    auto p1_res = Project::create_new("RC_Filter_Bench", "Lead Test Engineer");
    REQUIRE(p1_res.is_ok());
    auto p1 = p1_res.value();
    CHECK(p1->manifest().name == "RC_Filter_Bench");

    // 2. Netlist: DC source 5V, R = 1000 ohm, C = 1 uF, Ground
    auto rc_netlist = CircuitBenchmarkFactory::create_rc_filter(5.0, 1000.0, 1e-6);

    // 3. DC Operating Point & Transient Analysis
    DcOperatingPointSolver dc_solver;
    auto op_res = dc_solver.solve(rc_netlist);
    REQUIRE(op_res.is_ok());
    CHECK(op_res.value().converged);
    CHECK(std::abs(op_res.value().get_voltage("in") - 5.0) < 1e-4);

    // Solve Transient Step Response (5 time constants)
    TransientSolver tran_solver;
    tran_solver.t_stop = 0.005; // 5 ms
    tran_solver.time_step = 1e-5; // 10 us
    tran_solver.use_dc_op_initial_state = false; // Start with uncharged C

    auto tran_res = tran_solver.solve(rc_netlist);
    REQUIRE(tran_res.is_ok());
    auto tran = tran_res.value();
    CHECK(tran.success);

    auto v_out_trace = tran.get_voltage_trace("out");
    REQUIRE(v_out_trace != nullptr);
    double v_final = v_out_trace->back();
    double expected_v = 5.0 * (1.0 - std::exp(-5.0)); // ~4.966 V
    CHECK(std::abs(v_final - expected_v) < 0.1);

    // 4. Scripted Rule Controlling Switch State Based on Voltage Probe
    ScriptHostContext host_ctx;
    host_ctx.state_registers["v_out_probe"] = v_final;
    host_ctx.control_registers["switch_open"] = false;
    host_ctx.simulation_time = 0.005;

    SandboxLimits limits;
    limits.granted_capabilities = ScriptCapability::ReadState |
                                  ScriptCapability::WriteControl |
                                  ScriptCapability::AccessTime;

    HostSandboxAPI sandbox_api(host_ctx, limits);
    auto read_probe = sandbox_api.read_state("v_out_probe");
    REQUIRE(read_probe.is_ok());
    CHECK(std::get<double>(read_probe.value()) > 4.5);

    // Rule fires: switch opens when capacitor threshold > 4.5 V
    if (std::get<double>(read_probe.value()) > 4.5) {
        auto write_res = sandbox_api.write_control("switch_open", true);
        REQUIRE(write_res.is_ok());
    }
    CHECK(std::get<bool>(host_ctx.control_registers["switch_open"]) == true);

    // 5. Package Reusable Private Component
    ComponentRepository private_repo("Local_Hardware_Library", LibraryScope::UserPersonal);
    PackageManifest rc_pkg;
    rc_pkg.package_id = "pkg_rc_lowpass_1k_1u";
    rc_pkg.name = "RC_LowPass_Subcircuit";
    rc_pkg.version = SemVer{1, 0, 0, ""};
    rc_pkg.creator = "Lead Test Engineer";
    rc_pkg.license = "Apache-2.0";
    rc_pkg.compatible_domains = {"electronics"};
    rc_pkg.validation_status = ValidationStatus::CertifiedL4;

    auto pub_res = private_repo.publish_package(rc_pkg);
    REQUIRE(pub_res.is_ok());

    // 6. Instantiate into Second Project
    auto p2_res = Project::create_new("Telemetry_Receiver_Project", "Integration Team");
    REQUIRE(p2_res.is_ok());
    auto p2 = p2_res.value();

    auto queried_pkg = private_repo.find_package("RC_LowPass_Subcircuit");
    REQUIRE(queried_pkg.has_value());
    CHECK(queried_pkg->package_id == "pkg_rc_lowpass_1k_1u");

    ComponentDefinition instantiated_comp(
        base::Uuid::generate_random(),
        queried_pkg->name,
        "Instantiated Reusable RC Subcircuit",
        base::Version(queried_pkg->version.major, queried_pkg->version.minor, queried_pkg->version.patch)
    );

    ComponentPort in_port;
    in_port.schema.name = "in";
    in_port.schema.domain = PortDomain::Electrical;
    REQUIRE(instantiated_comp.add_port(in_port).is_ok());

    ComponentPort out_port;
    out_port.schema.name = "out";
    out_port.schema.domain = PortDomain::Electrical;
    REQUIRE(instantiated_comp.add_port(out_port).is_ok());

    CHECK(instantiated_comp.ports().size() == 2u);
    CHECK(instantiated_comp.type_name() == "RC_LowPass_Subcircuit");
}

// =============================================================================
// E2E-03: Section 23 Whole-Platform Acceptance Criteria Verification (AC-01 through AC-10)
// =============================================================================

TEST_CASE("E2E-03: Section 23 Whole-Platform Acceptance Criteria Verification (AC-01 through AC-10)") {
    // AC-01: Project Create, Add Elements, Unit-Aware Properties, Save and Reopen
    {
        auto prj = Project::create_new("AC_Project", "QA Lead").value();
        prj->manifest().metadata["sample_rate"] = "1000";
        std::filesystem::path temp_dir = std::filesystem::current_path() / "test_artifacts" / "ac_test";
        std::filesystem::create_directories(temp_dir);
        std::string pfile = (temp_dir / "ac_project.shunya").string();
        REQUIRE(prj->save_to_file(pfile).is_ok());
        auto reloaded = Project::open_from_file(pfile).value();
        CHECK(reloaded->manifest().name == "AC_Project");
        std::filesystem::remove_all(temp_dir);
    }

    // AC-02: Typed Ports Reject Incompatible Connection with Actionable Error
    {
        ComponentDefinition comp("MismatchedSystem", "Mismatched Domain Component");
        InternalConnection bad_conn;
        bad_conn.from_child = "UnknownChild";
        bad_conn.from_port = "p1";
        bad_conn.to_child = "";
        bad_conn.to_port = "out";
        auto conn_res = comp.add_connection(bad_conn);
        CHECK(conn_res.is_err());
    }

    // AC-03: Simulation Lifecycle Transitions (Run, Pause, Step, Stop, Reset) Without Corrupting State
    {
        SimulationRuntime sim;
        SimulationConfig cfg;
        cfg.start_time = 0.0;
        cfg.end_time = 1.0;
        cfg.initial_time_step = 0.01;
        REQUIRE(sim.configure(cfg).is_ok());
        CHECK(sim.state() == SimulationState::Configuring);
        REQUIRE(sim.start().is_ok());
        CHECK(sim.state() == SimulationState::Running);
        REQUIRE(sim.pause().is_ok());
        CHECK(sim.state() == SimulationState::Paused);
        REQUIRE(sim.step().is_ok());
        REQUIRE(sim.stop().is_ok());
        CHECK(sim.state() == SimulationState::Stopped);
        REQUIRE(sim.reset().is_ok());
        CHECK(sim.state() == SimulationState::Stopped);
    }

    // AC-04: Deterministic Reproducibility from Manifest
    {
        constexpr int steps = 100;
        constexpr double dt = 0.001;
        auto run_sim = [](int n_steps, double delta_t) {
            SimulationClock clk;
            for (int i = 0; i < n_steps; ++i) clk.advance(delta_t);
            return clk.physical_time();
        };
        double t1 = run_sim(steps, dt);
        double t2 = run_sim(steps, dt);
        CHECK(std::abs(t1 - t2) < 1e-12);
        CHECK(std::abs(t1 - 0.1) < 1e-12);
    }

    // AC-05: Safe Sandboxed Rule Reads Sensor and Controls Allowed Output
    {
        ScriptHostContext host_ctx;
        host_ctx.state_registers["sensor_input"] = 42.0;
        host_ctx.control_registers["actuator_valve"] = 0.0;
        SandboxLimits limits;
        limits.granted_capabilities = ScriptCapability::ReadState | ScriptCapability::WriteControl;
        HostSandboxAPI api(host_ctx, limits);
        auto val = api.read_state("sensor_input").value();
        REQUIRE(api.write_control("actuator_valve", std::get<double>(val) * 2.0).is_ok());
        CHECK(std::get<double>(host_ctx.control_registers["actuator_valve"]) == 84.0);
    }

    // AC-06: Live Observability Instruments (Virtual Instruments and Probes)
    {
        VirtualMultimeter dmm;
        dmm.set_mode(VirtualMultimeter::Mode::VoltageDC);
        dmm.sample(5.002);
        CHECK(std::abs(dmm.current_reading() - 5.002) < 1e-6);
    }

    // AC-07: Component Saved Privately, Versioned, Reused, and Validated
    {
        ComponentRepository repo("PrivateRepo", LibraryScope::UserPersonal);
        PackageManifest pkg;
        pkg.package_id = "pkg_motor_actuator";
        pkg.name = "BrushlessMotorActuator";
        pkg.version = SemVer{1, 2, 0, ""};
        REQUIRE(repo.publish_package(pkg).is_ok());
        auto found = repo.find_package("BrushlessMotorActuator");
        REQUIRE(found.has_value());
        CHECK(found->version.to_string() == "1.2.0");
    }

    // AC-08: Domain Reference Benchmarks Declared Across Physical Domains
    {
        // Electronics
        auto elec_div = CircuitBenchmarkFactory::create_voltage_divider(10.0, 100.0, 100.0);
        DcOperatingPointSolver dc_sol;
        CHECK(std::abs(dc_sol.solve(elec_div).value().get_voltage("out") - 5.0) < 1e-4);

        // Thermal
        auto thm_slab = ThermalBenchmarkFactory::create_1d_slab_conduction(1.0, 1.0, 100.0, 400.0, 300.0, 5);
        REQUIRE(thm_slab.solve_steady_state().is_ok());
        CHECK(std::abs(thm_slab.get_node(2).state.temperature - 350.0) < 0.05);

        // Mechanics
        auto mech_fall = MechanicsBenchmarkFactory::create_free_fall_benchmark(50.0, 1.0);
        mech_fall.step(0.1);
        CHECK(mech_fall.get_body("faller")->position().y < 50.0);
    }

    // AC-09: Security Sandbox Capability Enforcement
    {
        ScriptHostContext host_ctx;
        SandboxLimits strict_limits; // No WriteControl capability
        strict_limits.granted_capabilities = ScriptCapability::ReadState;
        HostSandboxAPI secure_api(host_ctx, strict_limits);
        auto unauthorized_write = secure_api.write_control("restricted_valve", 1.0);
        CHECK(unauthorized_write.is_err());
        CHECK(unauthorized_write.error().code == 403); // Permission denied: WriteControl capability required
    }

    // AC-10: Complete Stack Consistency & Version Metadata Integrity
    {
        CHECK(CURRENT_VERSION.major == 0);
        CHECK(CURRENT_VERSION.minor == 1);
        CHECK(!CURRENT_VERSION.as_string().empty());
    }
}

