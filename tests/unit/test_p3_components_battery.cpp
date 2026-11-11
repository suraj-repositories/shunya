#include "tests/test_framework.hpp"

#include "shunya/eng/battery_engineer.hpp"
#include "shunya/eng/component_generator.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cmath>

using namespace shunya::eng;
using namespace shunya::mat;
using namespace shunya::matdb;
using namespace shunya::core::base;
using namespace shunya::core::domain;

TEST_CASE("OBJCMP: Conductive Copper object becomes wire while insulating polymer fails wire qualification") {
    REQUIRE(ElementDatabase::global().load_default().is_ok());
    MaterialRegistry reg;

    MixtureExperiment cu_exp;
    cu_exp.name = "Cu_Mat";
    cu_exp.scenario = FormationScenario::PhysicalMixture;
    cu_exp.inputs = {{"Cu", QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
    Material cu = MaterialMixingLab::promote_to_material(cu_exp).value();
    REQUIRE(reg.register_material(cu).is_ok());

    Material ins(Uuid::generate_random(), "PTFE_Insulator", MaterialPhysicalState::Solid, 2200.0);
    ElectricalProperties ins_ep;
    ins_ep.electrical_conductivity = 1e-14;
    ins_ep.electrical_resistivity = 1e14;
    ins_ep.relative_permittivity = 2.1;
    ins.set_electrical(ins_ep);
    ThermalProperties ins_tp;
    ins_tp.thermal_conductivity = 0.25;
    ins_tp.specific_heat_capacity = 1000.0;
    ins.set_thermal(ins_tp);
    REQUIRE(reg.register_material(ins).is_ok());

    PhysicalObjectManager objs;
    PhysicalObject cu_bar;
    cu_bar.name = "Cu_Busbar";
    cu_bar.geometry.kind = ObjectShapeKind::Box;
    cu_bar.geometry.dim_x_m = 0.1;
    cu_bar.geometry.dim_y_m = 0.005;
    cu_bar.geometry.dim_z_m = 0.002;
    cu_bar.regions = {{"whole", "Cu", cu.id(), 1.0, 0.0, true}};
    auto cu_obj_id = objs.add_object(cu_bar, reg).value();

    PhysicalObject ptfe_bar = cu_bar;
    ptfe_bar.id = Uuid::generate_random();
    ptfe_bar.name = "PTFE_Block";
    ptfe_bar.regions = {{"whole", "Insulator", ins.id(), 1.0, 0.0, false}};
    auto ins_obj_id = objs.add_object(ptfe_bar, reg).value();

    ComponentTypeConfig cfg;
    cfg.conductor_length_m = 0.1;
    std::vector<ComponentTerminal> terms = {
        {"A", ElectricalPinPolarity::PassiveBiDirectional, {-0.05, 0.0, 0.0}, "whole"},
        {"B", ElectricalPinPolarity::PassiveBiDirectional, {0.05, 0.0, 0.0}, "whole"}
    };

    auto wire_comp = ComponentGenerator::generate(
        "Cu_Bus_Wire", *objs.get_object(cu_obj_id), ComponentKind::ConductorWire, cfg, terms, reg);
    REQUIRE(wire_comp.is_ok());
    CHECK(wire_comp.value().derived.is_qualified);
    CHECK(wire_comp.value().derived.primary_value > 0.0);
    CHECK(wire_comp.value().derived.primary_value < 0.01);

    // An insulating block MUST NOT automatically qualify as a conductor wire or resistor!
    auto bad_wire = ComponentGenerator::generate(
        "Insulator_As_Wire", *objs.get_object(ins_obj_id), ComponentKind::ConductorWire, cfg, terms, reg);
    CHECK(bad_wire.is_err());

    // But with dielectric config it CAN form a Capacitor!
    ComponentTypeConfig cap_cfg;
    cap_cfg.dielectric_thickness_m = 1e-4;
    cap_cfg.cross_section_area_m2 = 0.01;
    auto cap_comp = ComponentGenerator::generate(
        "PTFE_Capacitor", *objs.get_object(ins_obj_id), ComponentKind::Capacitor, cap_cfg, terms, reg);
    REQUIRE(cap_comp.is_ok());
    CHECK(cap_comp.value().derived.primary_value > 1e-10);
}

TEST_CASE("BAT: Lithium battery compatibility, parameter derivation, and discharge/charge curves") {
    REQUIRE(ElementDatabase::global().load_default().is_ok());
    MaterialRegistry reg;

    auto mk_el_mat = [&](const char* name, const char* sym) -> Material {
        MixtureExperiment exp;
        exp.name = name;
        exp.scenario = FormationScenario::PhysicalMixture;
        exp.inputs = {{sym, QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
        return MaterialMixingLab::promote_to_material(exp).value();
    };

    Material li_cathode = [&]() {
        MixtureExperiment exp;
        exp.name = "LiFePO4_Cathode";
        exp.scenario = FormationScenario::ChemicalCompound;
        exp.inputs = {
            {"Li", QuantityInputBasis::MoleCount, 1.0, 0.0, false},
            {"Fe", QuantityInputBasis::MoleCount, 1.0, 0.0, false},
            {"P",  QuantityInputBasis::MoleCount, 1.0, 0.0, false},
            {"O",  QuantityInputBasis::MoleCount, 4.0, 0.0, false}
        };
        return MaterialMixingLab::promote_to_material(exp).value();
    }();
    Material graphite = mk_el_mat("Graphite_Anode", "C");
    Material cu = mk_el_mat("Cu_Collector", "Cu");
    Material al = mk_el_mat("Al_Collector", "Al");

    Material sep(Uuid::generate_random(), "PP_Separator", MaterialPhysicalState::Solid, 920.0);
    sep.electrical().electrical_conductivity = 1e-12;
    sep.electrical().electrical_resistivity = 1e12;
    sep.thermal().specific_heat_capacity = 1900.0;

    Material elec(Uuid::generate_random(), "LiPF6_Electrolyte", MaterialPhysicalState::Liquid, 1210.0);
    elec.electrical().electrical_conductivity = 1.1;
    elec.electrical().electrical_resistivity = 1.0 / 1.1;
    elec.thermal().specific_heat_capacity = 1600.0;

    for (const auto& m : {li_cathode, graphite, cu, al, sep, elec}) {
        REQUIRE(reg.register_material(m).is_ok());
    }

    BatteryDesignSpec spec;
    spec.name = "Lab_LiFePO4_Cell";
    spec.chemistry_label = "LiFePO4 / Graphite";
    spec.materials.anode_material_id = graphite.id();
    spec.materials.cathode_material_id = li_cathode.id();
    spec.materials.electrolyte_material_id = elec.id();
    spec.materials.separator_material_id = sep.id();
    spec.materials.anode_collector_material_id = cu.id();
    spec.materials.cathode_collector_material_id = al.id();
    spec.materials.casing_material_id = al.id();
    spec.materials.positive_terminal_material_id = al.id();
    spec.materials.negative_terminal_material_id = cu.id();

    auto derived = BatteryEngineer::evaluate_and_validate(spec, reg);
    CHECK(derived.is_compatible);
    CHECK(derived.nominal_capacity_ah > 0.1);
    CHECK(derived.internal_r0_25c_ohm > 0.0);
    CHECK(derived.specific_energy_wh_kg > 20.0);
    CHECK(!derived.fidelity_disclosure.empty());

    // Simulate 1C discharge + 0.5C charge
    auto sim_res = BatteryEngineer::simulate_discharge_and_charge(spec, reg, 1.0, 600.0, 0.5, 600.0, 5.0);
    REQUIRE(sim_res.is_ok());
    const auto& sim = sim_res.value();
    CHECK(sim.completed);
    REQUIRE(sim.samples.size() > 100u);
    CHECK(sim.total_energy_wh > 0.0);
    CHECK(sim.peak_temperature_k >= spec.ambient_temp_k);

    // Metallic separator MUST be rejected (internal short circuit!)
    BatteryDesignSpec shorted = spec;
    shorted.materials.separator_material_id = cu.id();
    auto bad_derived = BatteryEngineer::evaluate_and_validate(shorted, reg);
    CHECK_FALSE(bad_derived.is_compatible);
    CHECK(!bad_derived.compatibility_errors.empty());

    // Excessive 12C discharge triggers thermal/C-rate warning
    auto abuse_res = BatteryEngineer::simulate_discharge_and_charge(spec, reg, 12.0, 120.0, 0.0, 0.0, 1.0);
    REQUIRE(abuse_res.is_ok());
    CHECK(abuse_res.value().had_limit_warnings);
}
