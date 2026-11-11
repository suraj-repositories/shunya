#include "tests/test_framework.hpp"

#include "shunya/matdb/element_database.hpp"
#include "shunya/mat/element_behavior.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/eng/engineering_pipeline.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/component_generator.hpp"
#include "shunya/eng/interactive_circuit.hpp"
#include "shunya/eng/physics_runtime.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>

using namespace shunya::eng;
using namespace shunya::mat;
using namespace shunya::matdb;
using namespace shunya::core::domain;
using namespace shunya::core::base;

TEST_CASE("STRESS-01: ElementDatabase CSV cold parse performance & completeness") {
    ElementDatabase db;
    const auto csv_path = std::filesystem::path(SHUNYA_SOURCE_DIR) / "data" / "elements.csv";
    const auto t0 = std::chrono::steady_clock::now();
    auto res = db.load_from_csv(csv_path);
    const auto t1 = std::chrono::steady_clock::now();
    REQUIRE(res.is_ok());
    CHECK(db.count() == 118);

    const double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    CHECK(elapsed_ms < 250.0);
}

TEST_CASE("STRESS-02: 1000-Object reactive recalculation scaling & propagation") {
    EngineeringWorkspaceState ws;
    Material cu(Uuid::generate_random(), "Cu_Stress", MaterialPhysicalState::Solid, 8960.0);
    cu.thermal().specific_heat_capacity = 385.0;
    cu.thermal().thermal_conductivity = 401.0;
    cu.electrical().electrical_conductivity = 5.96e7;
    cu.electrical().electrical_resistivity = 1.0 / 5.96e7;
    const Uuid cu_id = cu.id();
    REQUIRE(ws.material_registry().register_material(cu).is_ok());

    for (int i = 0; i < 1000; ++i) {
        PhysicalObject obj;
        obj.name = "Busbar_" + std::to_string(i);
        obj.geometry.kind = ObjectShapeKind::Box;
        obj.geometry.dim_x_m = 0.01;
        obj.geometry.dim_y_m = 0.1;
        obj.geometry.dim_z_m = 0.01;
        ObjectRegion reg;
        reg.material_id = cu_id;
        reg.volume_fraction = 1.0;
        reg.is_conductive_path = true;
        obj.regions.push_back(reg);
        REQUIRE(ws.object_manager().add_object(std::move(obj), ws.material_registry()).is_ok());
    }
    REQUIRE(ws.object_manager().count() == 1000);
    const double initial_mass = ws.object_manager().objects()[500].computed.mass_kg;
    CHECK(std::abs(initial_mass - (1e-5 * 8960.0)) < 1e-6);

    // Mutate material density and benchmark reactive propagation across all 1000 objects
    const Material* existing = ws.material_registry().get_material(cu_id);
    REQUIRE(existing != nullptr);
    Material updated = *existing;
    updated.set_density(4480.0);
    REQUIRE(ws.material_registry().unregister_material(cu_id).is_ok());
    REQUIRE(ws.material_registry().register_material(std::move(updated)).is_ok());

    const auto t0 = std::chrono::steady_clock::now();
    ws.notify_material_modified(cu_id, "Density halved for stress test");
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    CHECK(elapsed_ms < 250.0);

    const double updated_mass = ws.object_manager().objects()[500].computed.mass_kg;
    CHECK(std::abs(updated_mass - initial_mass * 0.5) < 1e-6);
}

TEST_CASE("EDGE-01: Extreme temperatures, pressures, NaN & invalid geometries") {
    REQUIRE(ElementDatabase::global().load_default().is_ok());

    const auto* cu = ElementDatabase::global().find_by_symbol("Cu");
    REQUIRE(cu != nullptr);

    // Extreme cryo T=1 K, ultra-high vacuum P=1e-3 Pa
    auto cryo = ElementBehaviorEngine::evaluate(*cu, 1.0, 1e-3, false);
    CHECK(cryo.phase == EvaluatedPhase::Solid);
    CHECK(cryo.density_kg_m3.available());
    CHECK(std::isfinite(*cryo.density_kg_m3.value));

    // Extreme plasma T=10000 K at 1 atm -> Gas, and high pressure P=1e9 Pa stays finite & non-Solid
    auto hot_atm = ElementBehaviorEngine::evaluate(*cu, 10000.0, 101325.0, true);
    CHECK(hot_atm.phase == EvaluatedPhase::Gas);
    CHECK_FALSE(hot_atm.warnings.empty());

    auto hot_hp = ElementBehaviorEngine::evaluate(*cu, 10000.0, 1e9, true);
    CHECK(hot_hp.phase != EvaluatedPhase::Solid);
    CHECK(hot_hp.density_kg_m3.available());
    CHECK(std::isfinite(*hot_hp.density_kg_m3.value));

    // Zero-dimension / negative geometry validation
    ObjectGeometrySpec bad_geo;
    bad_geo.kind = ObjectShapeKind::Cylinder;
    bad_geo.dim_y_m = 0.0;
    bad_geo.radius_m = -0.02;
    CHECK(bad_geo.validate().is_err());

    // NaN / negative T & P workspace validation
    AmbientEnvironmentConfig bad_env;
    bad_env.temperature_k = std::numeric_limits<double>::quiet_NaN();
    bad_env.pressure_pa = -5.0;
    EngineeringWorkspaceState ws;
    ws.set_environment(bad_env);
    CHECK_FALSE(ws.validate_workspace().is_valid());
}

TEST_CASE("PERSIST-EDGE: Legacy schema v0.0.1 migration & corrupt bundle rejection") {
    EngineeringWorkspaceState ws;

    // Legacy v0.0.1 project JSON (no bundle_version, has schema_version = "0.0.1" + id)
    const std::string legacy_json = R"({
      "schema_version": "0.0.1",
      "id": "11111111-2222-4333-8444-555555555555",
      "name": "Legacy_Phase1_Rotor",
      "author": "Lab Engineer",
      "description": "Migrated test"
    })";
    auto mig_res = ws.from_bundle_json(legacy_json);
    REQUIRE(mig_res.is_ok());
    REQUIRE(ws.project() != nullptr);
    CHECK(ws.project()->manifest().name == "Legacy_Phase1_Rotor");

    // Corrupt payload must be cleanly rejected
    auto bad_res = ws.from_bundle_json("{not a valid shunya bundle");
    CHECK(bad_res.is_err());
}

TEST_CASE("E2E-02-NEG: Negative engineering validation (Insulator Resistor & Open Circuit)") {
    EngineeringWorkspaceState ws;

    // 1. Insulator cannot become a conductive Resistor
    Material ptfe(Uuid::generate_random(), "PTFE_Insulator", MaterialPhysicalState::Solid, 2200.0);
    ptfe.electrical().electrical_conductivity = 1e-16;
    ptfe.electrical().electrical_resistivity = 1e16;
    ptfe.thermal().specific_heat_capacity = 1000.0;
    const Uuid ptfe_id = ptfe.id();
    REQUIRE(ws.material_registry().register_material(ptfe).is_ok());

    PhysicalObject block;
    block.name = "PTFE_Block";
    block.geometry.kind = ObjectShapeKind::Box;
    block.geometry.dim_x_m = 0.02;
    block.geometry.dim_y_m = 0.05;
    block.geometry.dim_z_m = 0.02;
    block.regions.push_back(ObjectRegion{"core", "Core", ptfe_id, 1.0, 0.02, false});
    auto bid_res = ws.object_manager().add_object(block, ws.material_registry());
    REQUIRE(bid_res.is_ok());

    const auto* obj_ptr = ws.object_manager().get_object(bid_res.value());
    REQUIRE(obj_ptr != nullptr);

    ComponentTypeConfig cfg;
    ComponentTerminal t1{"A", ElectricalPinPolarity::PassiveBiDirectional, {0.0, 0.0, 0.0}, "core"};
    ComponentTerminal t2{"B", ElectricalPinPolarity::PassiveBiDirectional, {0.02, 0.0, 0.0}, "core"};
    std::vector<ComponentTerminal> terms{t1, t2};
    auto comp_res = ComponentGenerator::generate("Bad_Res", *obj_ptr, ComponentKind::Resistor, cfg, terms, ws.material_registry());
    CHECK(comp_res.is_err()); // Qualification rejected!

    // 2. Circuit missing Ground must fail validation & DC solver cleanly
    InteractiveCircuitDocument doc("NoGroundCircuit");
    PlacedCircuitComponent src;
    src.id = "V1";
    src.label = "V1";
    src.kind = CircuitSymbolKind::VoltageSourceDC;
    src.primary_value = 5.0;
    src.x_mm = 0.0;
    src.y_mm = 0.0;
    doc.add_component(src);
    doc.resolve_connectivity();
    const auto issues = doc.validate();
    CHECK_FALSE(issues.empty());
    auto dc_res = CircuitSimulationRunner::run_dc(doc);
    CHECK(dc_res.is_err());
}

TEST_CASE("E2E-02-BUSBAR: 2D->3D Copper Busbar Mesh, Joule Heating & Thermal Expansion") {
    EngineeringWorkspaceState ws;

    Material cu(Uuid::generate_random(), "Pure_Copper", MaterialPhysicalState::Solid, 8960.0);
    cu.thermal().specific_heat_capacity = 385.0;
    cu.thermal().thermal_conductivity = 401.0;
    cu.thermal().thermal_expansion_coeff = 16.5e-6;
    cu.electrical().electrical_conductivity = 5.96e7;
    cu.electrical().electrical_resistivity = 1.0 / 5.96e7;
    const Uuid cu_id = cu.id();
    REQUIRE(ws.material_registry().register_material(cu).is_ok());

    PhysicalObject busbar;
    busbar.name = "Cu_Busbar_Cylinder";
    busbar.geometry.kind = ObjectShapeKind::Cylinder;
    busbar.geometry.dim_y_m = 0.20;
    busbar.geometry.radius_m = 0.005;
    busbar.geometry.position = {0.0, 0.5, 0.0};
    busbar.regions.push_back(ObjectRegion{"core", "Cu Core", cu_id, 1.0, 0.0, true});

    auto mesh = busbar.geometry.generate_solid_mesh();
    CHECK(mesh.vertices.size() > 8);
    CHECK(mesh.triangles.size() >= 12);
    CHECK(mesh.analyze_manifold().is_watertight);

    auto bid = ws.object_manager().add_object(busbar, ws.material_registry());
    REQUIRE(bid.is_ok());

    // Step multiphysics runtime with 120 W Joule heat source
    ws.physics_runtime().initialize_from_objects(ws.object_manager(), 298.15, 9.80665, 10.0);
    ws.physics_runtime().set_body_heat_source(bid.value(), 120.0);
    ws.physics_runtime().set_body_static(bid.value(), true);

    double last_temp = 298.15;
    for (int s = 0; s < 40; ++s) {
        auto step_res = ws.physics_runtime().step(0.05);
        REQUIRE(step_res.is_ok());
        last_temp = step_res.value().average_temperature_k;
    }

    const auto* body = ws.physics_runtime().find_body(bid.value());
    REQUIRE(body != nullptr);
    CHECK(last_temp > 298.15);
    CHECK(body->thermal_strain > 0.0);
}
