#include "tests/test_framework.hpp"

#include "shunya/eng/physical_object.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cmath>

using namespace shunya::eng;
using namespace shunya::mat;
using namespace shunya::matdb;
using namespace shunya::core::base;
using namespace shunya::core::domain;

TEST_CASE("OBJMAT: Pure element & mixed material objects compute exact geometry, mass, density & R") {
    REQUIRE(ElementDatabase::global().load_default().is_ok());
    MaterialRegistry reg;

    // Pure Copper wire material
    MixtureExperiment cu_exp;
    cu_exp.name = "Pure_Cu_Wire_Mat";
    cu_exp.scenario = FormationScenario::PhysicalMixture;
    cu_exp.inputs = {{"Cu", QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
    auto cu_mat_res = MaterialMixingLab::promote_to_material(cu_exp);
    REQUIRE(cu_mat_res.is_ok());
    const Material cu_mat = cu_mat_res.value();
    REQUIRE(reg.register_material(cu_mat).is_ok());

    // Cylinder Copper Wire: r = 1 mm, L = 1.0 m -> A = pi * 1e-6 m^2, V = pi * 1e-6 m^3
    PhysicalObject wire;
    wire.name = "Copper_Wire_1m";
    wire.category = "Wire";
    wire.geometry.kind = ObjectShapeKind::Cylinder;
    wire.geometry.radius_m = 0.001;
    wire.geometry.dim_y_m = 1.0;
    wire.regions = {{"whole", "Copper Core", cu_mat.id(), 1.0, 0.0, true}};

    auto id_res = PhysicalObjectManager{}.add_object(wire, reg);
    PhysicalObjectManager mgr;
    auto add_res = mgr.add_object(wire, reg);
    REQUIRE(add_res.is_ok());
    (void)id_res;

    const PhysicalObject* stored = mgr.get_object(add_res.value());
    REQUIRE(stored != nullptr);
    REQUIRE(stored->computed.valid);
    const double expected_vol = 3.14159265358979323846 * 1e-6 * 1.0;
    CHECK(std::abs(stored->computed.volume_m3 - expected_vol) < 1e-10);
    CHECK(std::abs(stored->computed.effective_density_kg_m3 - 8920.0) < 5.0);
    CHECK(std::abs(stored->computed.mass_kg - expected_vol * stored->computed.effective_density_kg_m3) < 1e-9);
    CHECK(stored->computed.has_conductive_path);
    CHECK(stored->computed.axial_electrical_resistance_ohm > 0.0);
    CHECK(stored->computed.axial_electrical_resistance_ohm < 0.05); // ~5.4 mOhm for 1 mm Cu wire
}

TEST_CASE("OBJGEO: Reactive propagation updates object properties on material change & rejects invalid inputs") {
    MaterialRegistry reg;
    Material mat(Uuid::generate_random(), "Custom_Alloy_X", MaterialPhysicalState::Solid, 5000.0);
    ThermalProperties tp;
    tp.specific_heat_capacity = 500.0;
    tp.thermal_conductivity = 50.0;
    mat.set_thermal(tp);
    ElectricalProperties ep;
    ep.electrical_conductivity = 1e6;
    mat.set_electrical(ep);
    REQUIRE(reg.register_material(mat).is_ok());

    PhysicalObjectManager mgr;
    PhysicalObject box;
    box.name = "Alloy_Block";
    box.geometry.kind = ObjectShapeKind::Box;
    box.geometry.dim_x_m = 0.2;
    box.geometry.dim_y_m = 0.1;
    box.geometry.dim_z_m = 0.05; // V = 0.001 m^3
    box.regions = {{"whole", "Body", mat.id(), 1.0, 0.0, true}};

    auto box_id_res = mgr.add_object(box, reg);
    REQUIRE(box_id_res.is_ok());
    const Uuid bid = box_id_res.value();
    CHECK(std::abs(mgr.get_object(bid)->computed.mass_kg - 5.0) < 1e-6);

    // Mutate material density from 5000 -> 8000 kg/m^3 and propagate reactively
    Material updated = mat;
    updated.set_density(8000.0);
    REQUIRE(reg.unregister_material(mat.id()).is_ok());
    REQUIRE(reg.register_material(updated).is_ok());
    mgr.recalculate_for_material(mat.id(), reg);
    CHECK(std::abs(mgr.get_object(bid)->computed.mass_kg - 8.0) < 1e-6);

    // JSON round-trip preserves dimensions & regions
    const std::string json = mgr.get_object(bid)->to_json();
    auto parsed = PhysicalObject::from_json(json);
    REQUIRE(parsed.is_ok());
    CHECK(parsed.value().name == "Alloy_Block");
    CHECK(std::abs(parsed.value().geometry.dim_x_m - 0.2) < 1e-9);
    REQUIRE(parsed.value().regions.size() == 1u);

    // Invalid geometry (negative dimension) rejected
    PhysicalObject bad = box;
    bad.geometry.dim_x_m = -0.1;
    CHECK(mgr.add_object(bad, reg).is_err());

    // Missing material rejected
    PhysicalObject orphan = box;
    orphan.regions[0].material_id = Uuid::generate_random();
    CHECK(mgr.add_object(orphan, reg).is_err());
}
