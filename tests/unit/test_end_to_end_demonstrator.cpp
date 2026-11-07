#include "tests/test_framework.hpp"
#include "shunya/core/base/app_context.hpp"
#include "shunya/core/base/units.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"
#include "shunya/core/domain/simulation_clock.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/thermal.hpp"
#include "shunya/core/domain/mechanics.hpp"

#include <cmath>
#include <vector>
#include <string>
#include <filesystem>

using namespace shunya::core;
using namespace shunya::core::base;
using namespace shunya::core::domain;
using namespace shunya::core::domain::thermal;
using namespace shunya::core::domain::mechanics;

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
