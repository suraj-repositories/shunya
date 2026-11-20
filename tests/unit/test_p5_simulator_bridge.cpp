#include "tests/test_framework.hpp"
#include "shunya/eng/simulator_bridge.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/cavity_physics.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_assignment.hpp"

#include <cmath>

using namespace shunya;

TEST_CASE("PH5-27.01: Pre-Flight Verification Engine & Failure Detection") {
    eng::SimulatorBridgeCoordinator bridge;
    core::domain::MaterialRegistry mat_reg;

    // Register a valid conductive material (Tungsten) and an insulator (Borosilicate Glass)
    core::domain::Material mat_tungsten(core::base::Uuid::generate_random(), "Tungsten_Pure",
                                        core::domain::MaterialPhysicalState::Solid, 19250.0);
    core::domain::ElectricalProperties ep_w;
    ep_w.electrical_conductivity = 1.78e7;
    ep_w.electrical_resistivity = 5.6e-8;
    mat_tungsten.set_electrical(ep_w);

    core::domain::ThermalProperties tp_w;
    tp_w.thermal_conductivity = 173.0;
    tp_w.specific_heat_capacity = 134.0;
    mat_tungsten.set_thermal(tp_w);

    core::domain::MechanicalProperties mp_w;
    mp_w.youngs_modulus_pa = 411e9;
    mp_w.yield_strength_pa = 750e6;
    mat_tungsten.set_mechanical(mp_w);

    const auto tungsten_id = mat_tungsten.id();
    REQUIRE(mat_reg.register_material(std::move(mat_tungsten)).is_ok());

    core::domain::Material mat_glass(core::base::Uuid::generate_random(), "Borosilicate_Glass",
                                     core::domain::MaterialPhysicalState::Solid, 2230.0);
    core::domain::ElectricalProperties ep_g;
    ep_g.electrical_conductivity = 1e-14;
    mat_glass.set_electrical(ep_g);

    core::domain::ThermalProperties tp_g;
    tp_g.thermal_conductivity = 1.14;
    tp_g.specific_heat_capacity = 830.0;
    mat_glass.set_thermal(tp_g);

    const auto glass_id = mat_glass.id();
    REQUIRE(mat_reg.register_material(std::move(mat_glass)).is_ok());

    // -------------------------------------------------------------------------
    // Test Case A: Valid sealed object with 2 conductive terminals passes pre-flight
    // -------------------------------------------------------------------------
    eng::PhysicalObject valid_bulb;
    valid_bulb.name = "A19_Incandescent_Bulb";
    valid_bulb.geometry = eng::ObjectGeometrySpec::make_cylinder(0.030, 0.110);
    valid_bulb.computed.volume_m3 = 1.45e-4;
    valid_bulb.computed.surface_area_m2 = 0.015;
    valid_bulb.computed.mass_kg = 0.035;
    valid_bulb.computed.axial_electrical_resistance_ohm = 240.0;
    valid_bulb.computed.has_conductive_path = true;
    valid_bulb.computed.is_solid_at_operating_temp = true;

    // Region 1: Tungsten Filament
    eng::ObjectRegion r_fil("filament", "Tungsten Filament", tungsten_id, 0.005, 0.000045, true, "",
                           eng::RegionFunctionalRole::ActiveEmitter);
    valid_bulb.regions.push_back(r_fil);

    // Region 2: Glass Envelope
    eng::ObjectRegion r_env("envelope", "Glass Envelope", glass_id, 0.995, 0.0008, false, "",
                           eng::RegionFunctionalRole::Structural);
    valid_bulb.regions.push_back(r_env);

    // Cavity: Sealed Argon/Nitrogen fill
    valid_bulb.cavity.kind = eng::CavityKind::FillGas;
    valid_bulb.cavity.cavity_volume_m3 = 1.25e-4;
    valid_bulb.cavity.fill_pressure_pa = 80000.0;
    valid_bulb.cavity.wall_thickness_m = 0.0008;
    valid_bulb.cavity.envelope_radius_m = 0.030;

    // Terminals: 2 pins connected to conductive filament region ("filament")
    eng::ObjectTerminal t1;
    t1.name = "Pin_Base";
    t1.connected_region = "filament";
    t1.position = {0, -0.055, 0};

    eng::ObjectTerminal t2;
    t2.name = "Pin_Shell";
    t2.connected_region = "filament";
    t2.position = {0, -0.040, 0};

    valid_bulb.terminals.push_back(t1);
    valid_bulb.terminals.push_back(t2);

    auto report_valid = eng::SimulatorBridgeCoordinator::validate_object(valid_bulb, mat_reg);
    CHECK(report_valid.passed);
    CHECK_FALSE(report_valid.has_errors());

    // -------------------------------------------------------------------------
    // Test Case B: Unassigned material in region triggers PF-01-MAT failure
    // -------------------------------------------------------------------------
    eng::PhysicalObject broken_mat = valid_bulb;
    broken_mat.regions[0].material_id = core::base::Uuid{}; // nil material ID
    auto report_mat = eng::SimulatorBridgeCoordinator::validate_object(broken_mat, mat_reg);
    CHECK_FALSE(report_mat.passed);
    CHECK(report_mat.has_errors());
    bool found_mat_err = false;
    for (const auto& issue : report_mat.issues) {
        if (issue.rule_id == "PF-01-MAT") found_mat_err = true;
    }
    CHECK(found_mat_err);

    // -------------------------------------------------------------------------
    // Test Case C: Zero cavity volume triggers PF-02-SEAL failure
    // -------------------------------------------------------------------------
    eng::PhysicalObject broken_cav = valid_bulb;
    broken_cav.cavity.cavity_volume_m3 = 0.0; // zero enclosed volume
    auto report_cav = eng::SimulatorBridgeCoordinator::validate_object(broken_cav, mat_reg);
    CHECK_FALSE(report_cav.passed);
    CHECK(report_cav.has_errors());
    bool found_seal_err = false;
    for (const auto& issue : report_cav.issues) {
        if (issue.rule_id == "PF-02-SEAL") found_seal_err = true;
    }
    CHECK(found_seal_err);

    // -------------------------------------------------------------------------
    // Test Case D: Terminal connected to Dielectric region triggers PF-03-TERM failure
    // -------------------------------------------------------------------------
    eng::PhysicalObject broken_term = valid_bulb;
    broken_term.regions[1].role = eng::RegionFunctionalRole::DielectricSpacer;
    broken_term.terminals[0].connected_region = "envelope"; // connected to dielectric glass
    auto report_term = eng::SimulatorBridgeCoordinator::validate_object(broken_term, mat_reg);
    CHECK_FALSE(report_term.passed);
    CHECK(report_term.has_errors());
    bool found_term_err = false;
    for (const auto& issue : report_term.issues) {
        if (issue.rule_id == "PF-03-TERM") found_term_err = true;
    }
    CHECK(found_term_err);

    // -------------------------------------------------------------------------
    // Test Case E: Model generation produces lumped nodes & electrical branches
    // -------------------------------------------------------------------------
    auto model_res = eng::SimulatorBridgeCoordinator::generate_simulation_model(valid_bulb, mat_reg);
    CHECK(model_res.is_ok());
    const auto& sim_model = model_res.value();
    CHECK(sim_model.thermal_nodes.size() == 2u);
    CHECK(sim_model.electrical_branches.size() == 1u);
    CHECK(std::abs(sim_model.electrical_branches[0].resistance_ohm - 240.0) < 1.0);
    CHECK(sim_model.total_mass_kg > 0.0);
}

TEST_CASE("PH5-27.02: Quick-Run In-Situ Simulation, 60Hz Telemetry & Round-Trip Re-Sync") {
    eng::SimulatorBridgeCoordinator bridge;
    core::domain::MaterialRegistry mat_reg;

    core::domain::Material mat_tungsten(core::base::Uuid::generate_random(), "Tungsten",
                                        core::domain::MaterialPhysicalState::Solid, 19250.0);
    core::domain::ElectricalProperties ep_w;
    ep_w.electrical_conductivity = 1.78e7;
    ep_w.electrical_resistivity = 5.6e-8;
    mat_tungsten.set_electrical(ep_w);

    core::domain::ThermalProperties tp_w;
    tp_w.thermal_conductivity = 173.0;
    tp_w.specific_heat_capacity = 134.0;
    mat_tungsten.set_thermal(tp_w);

    const auto tungsten_id = mat_tungsten.id();
    REQUIRE(mat_reg.register_material(std::move(mat_tungsten)).is_ok());

    eng::PhysicalObject bulb;
    bulb.name = "Test_Bulb";
    bulb.geometry = eng::ObjectGeometrySpec::make_cylinder(0.030, 0.110);
    bulb.computed.volume_m3 = 1.45e-4;
    bulb.computed.surface_area_m2 = 0.015;
    bulb.computed.mass_kg = 0.035;
    bulb.computed.axial_electrical_resistance_ohm = 240.0; // R = 240 ohms at nominal thickness (0.045 mm)

    eng::ObjectRegion r_fil("filament", "Filament", tungsten_id, 1.0, 0.000045, true, "",
                           eng::RegionFunctionalRole::ActiveEmitter);
    bulb.regions.push_back(r_fil);

    eng::ObjectTerminal t1;
    t1.name = "Pin_1";
    t1.connected_region = "filament";
    t1.position = {0, -0.05, 0};

    eng::ObjectTerminal t2;
    t2.name = "Pin_2";
    t2.connected_region = "filament";
    t2.position = {0, -0.04, 0};

    bulb.terminals.push_back(t1);
    bulb.terminals.push_back(t2);

    // -------------------------------------------------------------------------
    // 1. Start Quick-Run with Step Voltage (120V)
    // -------------------------------------------------------------------------
    bridge.start_quick_run(eng::SimulationQuickRunPreset::Step_Voltage_120V, bulb, mat_reg, 0.10, 0.001);
    CHECK(bridge.is_running());
    CHECK(bridge.sync_status() == eng::SyncStatus::Simulation_Running);
    CHECK(std::abs(bridge.active_resistance_ohm() - 240.0) < 1e-3);

    // Step simulation for 60 iterations (60 ms)
    for (int i = 0; i < 60; ++i) {
        bridge.step_simulation(0.001);
    }

    auto snap = bridge.latest_telemetry();
    CHECK(std::abs(snap.voltage_v - 120.0) < 1e-3);
    CHECK(std::abs(snap.current_a - 0.50) < 0.05); // I = 120 / 240 = 0.5 A
    CHECK(snap.temperature_k > 298.15); // Temperature rising due to Joule heating
    CHECK(snap.deformation_m > 0.0); // Thermal expansion observed

    // Telemetry streaming: Ring buffer holds sampled telemetry
    auto stream = bridge.sample_telemetry(100);
    CHECK(stream.size() >= 60u);

    // -------------------------------------------------------------------------
    // 2. Modify object parameters (e.g. doubling wire thickness reduces resistance)
    // -------------------------------------------------------------------------
    bridge.stop_simulation();
    bridge.mark_object_modified(bulb.id);
    CHECK(bridge.sync_status() == eng::SyncStatus::Modified_Out_Of_Sync);

    // User updates filament wire thickness: resistance drops from 240 ohms to 60 ohms
    bulb.computed.axial_electrical_resistance_ohm = 60.0;

    // Resynchronize simulation
    bool resynced = bridge.resync_active_simulation(bulb, mat_reg);
    CHECK(resynced);
    CHECK(bridge.sync_status() == eng::SyncStatus::In_Sync_Clean);
    CHECK(std::abs(bridge.active_resistance_ohm() - 60.0) < 1e-3);

    // Restart simulation with re-synchronized resistance
    bridge.start_quick_run(eng::SimulationQuickRunPreset::Step_Voltage_120V, bulb, mat_reg, 0.05, 0.001);
    bridge.step_simulation(0.001);
    auto resync_snap = bridge.latest_telemetry();
    // At 60 ohms and 120V: I = 120 / 60 = 2.0 A
    CHECK(std::abs(resync_snap.current_a - 2.0) < 0.1);
    bridge.stop_simulation();
}
