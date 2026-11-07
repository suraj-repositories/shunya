#include "tests/test_framework.hpp"
#include "shunya/core/domain/material_runtime.hpp"
#include "shunya/core/domain/material_adapters.hpp"

#include <cmath>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("Chemical element and isotope definitions (MATRT-01, MATRT-02)") {
    ChemicalElementDefinition carbon;
    carbon.atomic_number = 6;
    carbon.symbol = "C";
    carbon.name = "Carbon";
    carbon.period = 2;
    carbon.group = 14;
    carbon.block = 'p';
    carbon.standard_atomic_weight = 12.011;
    carbon.electron_configuration = "[He] 2s2 2p2";

    IsotopeDefinition c12{6, 12, 12.0, 98.93, true, std::nullopt, "None", "0+", 0.0035};
    IsotopeDefinition c13{6, 13, 13.003355, 1.07, true, std::nullopt, "None", "1/2-", 0.0014};
    IsotopeDefinition c14{6, 14, 14.003241, 0.0, false, 1.808e11, "BetaMinus", "0+", 0.0}; // half-life 5730 yr

    carbon.isotopes = {c12, c13, c14};

    REQUIRE(carbon.atomic_number == 6);
    REQUIRE(carbon.symbol == "C");
    REQUIRE(carbon.has_isotope(12));
    REQUIRE(carbon.has_isotope(13));
    REQUIRE(carbon.has_isotope(14));
    REQUIRE_FALSE(carbon.has_isotope(15));

    const auto* iso14 = carbon.get_isotope(14);
    REQUIRE(iso14 != nullptr);
    REQUIRE(iso14->is_stable == false);
    REQUIRE(iso14->decay_mode == "BetaMinus");
    REQUIRE(iso14->half_life_seconds.has_value());
    REQUIRE(*iso14->half_life_seconds > 1e11);
}

TEST_CASE("Material definition for pure elements, compounds, alloys and mixtures (MATRT-03)") {
    // 1. Pure element
    MaterialDefinition copper_mat(base::Uuid::generate_random(), "Copper Pure", MaterialClassification::PureElement);
    copper_mat.add_constituent({"Cu", FractionBasis::MassFraction, 1.0});
    auto rep_cu = copper_mat.validate_composition();
    REQUIRE(!rep_cu.has_errors());

    // 2. Alloy: Stainless Steel 316L (Fe 68%, Cr 17%, Ni 12%, Mo 2.5%, Mn 0.5%)
    MaterialDefinition ss316(base::Uuid::generate_random(), "Stainless Steel 316L", MaterialClassification::Alloy);
    ss316.add_constituent({"Fe", FractionBasis::MassFraction, 0.68});
    ss316.add_constituent({"Cr", FractionBasis::MassFraction, 0.17});
    ss316.add_constituent({"Ni", FractionBasis::MassFraction, 0.12});
    ss316.add_constituent({"Mo", FractionBasis::MassFraction, 0.025});
    ss316.add_constituent({"Mn", FractionBasis::MassFraction, 0.005});

    auto rep_alloy = ss316.validate_composition();
    REQUIRE(!rep_alloy.has_errors());
    REQUIRE(ss316.constituents().size() == 5);

    // 3. Invalid composition (sum != 1.0)
    MaterialDefinition bad_mat(base::Uuid::generate_random(), "Bad Alloy", MaterialClassification::Alloy);
    bad_mat.add_constituent({"Fe", FractionBasis::MassFraction, 0.5});
    bad_mat.add_constituent({"Ni", FractionBasis::MassFraction, 0.2}); // sum = 0.7 != 1.0
    auto rep_bad = bad_mat.validate_composition();
    REQUIRE(rep_bad.has_errors());

    // Serialization roundtrip
    std::string json = ss316.to_json();
    auto ss316_rt = MaterialDefinition::from_json(json);
    REQUIRE(ss316_rt.is_ok());
    REQUIRE(ss316_rt.value().name() == "Stainless Steel 316L");
    REQUIRE(ss316_rt.value().constituents().size() == 5);
}

TEST_CASE("Material state mutation, phase fractions and version tracking (MATRT-05)") {
    MaterialState state;
    REQUIRE(state.state_version == 1);
    REQUIRE(std::abs(state.temperature_k - 293.15) < 1e-4);
    REQUIRE(std::abs(state.pressure_pa - 101325.0) < 1e-4);

    state.set_temperature(350.0);
    REQUIRE(state.state_version == 2);
    REQUIRE(std::abs(state.temperature_k - 350.0) < 1e-4);

    state.set_phase_fraction("Solid", 0.75);
    state.set_phase_fraction("Liquid", 0.25);
    REQUIRE(state.state_version == 4);
    REQUIRE(std::abs(state.get_phase_fraction("Solid") - 0.75) < 1e-4);
    REQUIRE(std::abs(state.get_phase_fraction("Liquid") - 0.25) < 1e-4);
    REQUIRE(std::abs(state.get_phase_fraction("Gas") - 0.0) < 1e-4);

    state.accumulated_plastic_strain = 0.015;
    state.internal_energy_j_kg = 5.4e5;
    state.history_variables["back_stress_kpa"] = 120.0;

    // State JSON serialization roundtrip (MATRT-11)
    std::string state_json = state.to_json();
    auto state_rt = MaterialState::from_json(state_json);
    REQUIRE(state_rt.is_ok());
    const auto& restored = state_rt.value();
    REQUIRE(std::abs(restored.temperature_k - 350.0) < 1e-4);
    REQUIRE(std::abs(restored.accumulated_plastic_strain - 0.015) < 1e-6);
    REQUIRE(std::abs(restored.internal_energy_j_kg - 5.4e5) < 1.0);
    REQUIRE(std::abs(restored.get_phase_fraction("Solid") - 0.75) < 1e-4);
    REQUIRE(std::abs(restored.history_variables.at("back_stress_kpa") - 120.0) < 1e-4);
}

TEST_CASE("Material region attachment to geometry and state persistence (MATRT-04, MATRT-11)") {
    auto mat_id = base::Uuid::generate_random();
    auto reg_id = base::Uuid::generate_random();
    MaterialRegion region(reg_id, "TurbineBlade_Root", mat_id, "mesh/volume_patch_04");
    region.set_volume_m3(0.0025);

    region.state().set_temperature(1250.0);
    region.state().set_pressure(2.5e6);
    region.state().history_variables["creep_strain"] = 0.0042;

    REQUIRE(region.id() == reg_id);
    REQUIRE(region.material_id() == mat_id);
    REQUIRE(region.geometry_ref() == "mesh/volume_patch_04");
    REQUIRE(std::abs(region.volume_m3() - 0.0025) < 1e-6);
    REQUIRE(std::abs(region.state().temperature_k - 1250.0) < 1e-4);

    // Full restart roundtrip
    std::string reg_json = region.to_json();
    auto reg_rt = MaterialRegion::from_json(reg_json);
    REQUIRE(reg_rt.is_ok());
    const auto& r_restored = reg_rt.value();
    REQUIRE(r_restored.name() == "TurbineBlade_Root");
    REQUIRE(r_restored.material_id() == mat_id);
    REQUIRE(r_restored.geometry_ref() == "mesh/volume_patch_04");
    REQUIRE(std::abs(r_restored.volume_m3() - 0.0025) < 1e-6);
    REQUIRE(std::abs(r_restored.state().temperature_k - 1250.0) < 1e-4);
    REQUIRE(std::abs(r_restored.state().history_variables.at("creep_strain") - 0.0042) < 1e-6);
}

TEST_CASE("Phase transition model and apparent heat capacity (MATRT-08)") {
    PhaseTransitionModel ptm;
    ptm.solidus_temperature_k = 1600.0;
    ptm.liquidus_temperature_k = 1700.0;
    ptm.latent_heat_fusion_j_kg = 2.5e5; // 250 kJ/kg
    double base_cp = 500.0; // J/(kg*K)

    // Solid below solidus
    REQUIRE(std::abs(ptm.compute_liquid_fraction(1500.0) - 0.0) < 1e-6);
    REQUIRE(std::abs(ptm.compute_apparent_heat_capacity(1500.0, base_cp) - base_cp) < 1e-6);

    // Liquid above liquidus
    REQUIRE(std::abs(ptm.compute_liquid_fraction(1800.0) - 1.0) < 1e-6);
    REQUIRE(std::abs(ptm.compute_apparent_heat_capacity(1800.0, base_cp) - base_cp) < 1e-6);

    // Mushy zone mid-point T = 1650 K (liquid fraction = 0.5)
    REQUIRE(std::abs(ptm.compute_liquid_fraction(1650.0) - 0.5) < 1e-6);
    // Apparent cp = base_cp + L_f / delta_T = 500 + 250000 / 100 = 500 + 2500 = 3000 J/(kg*K)
    double expected_app_cp = 3000.0;
    REQUIRE(std::abs(ptm.compute_apparent_heat_capacity(1650.0, base_cp) - expected_app_cp) < 1e-3);
}

TEST_CASE("Property cache invalidation on state and model updates (MATRT-10)") {
    PropertyCache cache;
    auto mat_id = base::Uuid::generate_random();

    MaterialState state;
    state.set_temperature(300.0);
    state.set_pressure(101325.0);

    // Initial cache miss
    auto val1 = cache.get(mat_id, "thermal_conductivity", state);
    REQUIRE(!val1.has_value());
    REQUIRE(cache.misses() == 1);
    REQUIRE(cache.hits() == 0);

    // Store value
    cache.put(mat_id, "thermal_conductivity", state, 401.0);
    REQUIRE(cache.size() == 1);

    // Cache hit
    auto val2 = cache.get(mat_id, "thermal_conductivity", state);
    REQUIRE(val2.has_value());
    REQUIRE(std::abs(*val2 - 401.0) < 1e-6);
    REQUIRE(cache.hits() == 1);

    // Temperature change creates new state version and key -> cache miss
    state.set_temperature(350.0);
    auto val3 = cache.get(mat_id, "thermal_conductivity", state);
    REQUIRE(!val3.has_value());
    REQUIRE(cache.misses() == 2);

    // Selective material invalidation
    cache.put(mat_id, "thermal_conductivity", state, 395.0);
    REQUIRE(cache.size() == 2);
    cache.invalidate_material(mat_id);
    REQUIRE(cache.size() == 0);
}

TEST_CASE("Material domain solver adapters for multiphysics (MATRT-09)") {
    MaterialDefinition steel(base::Uuid::generate_random(), "Steel", MaterialClassification::Alloy);
    steel.set_reference_density(7850.0);
    steel.set_scalar_property("thermal_conductivity", 45.0);
    steel.set_scalar_property("specific_heat_capacity", 480.0);
    steel.set_scalar_property("density", 7850.0);
    steel.set_scalar_property("thermal_expansion_coeff", 1.2e-5);
    steel.set_scalar_property("emissivity", 0.75);

    steel.set_scalar_property("youngs_modulus", 200.0e9);
    steel.set_scalar_property("poissons_ratio", 0.28);
    steel.set_scalar_property("yield_strength", 300.0e6);

    steel.set_scalar_property("electrical_conductivity", 7.0e6);
    steel.set_scalar_property("relative_permeability", 150.0);

    MaterialState state;
    state.set_temperature(300.0);

    // 1. Thermal Domain Adapter
    auto th_res = MaterialDomainAdapter::extract_thermal(steel, state);
    REQUIRE(th_res.is_ok());
    const auto& th = th_res.value();
    REQUIRE(std::abs(th.thermal_conductivity - 45.0) < 1e-3);
    REQUIRE(std::abs(th.specific_heat_capacity - 480.0) < 1e-3);
    REQUIRE(std::abs(th.density - 7850.0) < 1e-3);

    // 2. Mechanical Domain Adapter
    auto mech_res = MaterialDomainAdapter::extract_mechanical(steel, state);
    REQUIRE(mech_res.is_ok());
    const auto& mech = mech_res.value();
    REQUIRE(std::abs(mech.youngs_modulus - 200.0e9) < 1e-3);
    REQUIRE(std::abs(mech.poissons_ratio - 0.28) < 1e-3);
    // Derived G = 200e9 / (2 * 1.28) = 78.125 GPa
    REQUIRE(std::abs(mech.shear_modulus - (200.0e9 / 2.56)) < 1.0);

    // 3. Electrical Domain Adapter
    auto elec_res = MaterialDomainAdapter::extract_electrical(steel, state);
    REQUIRE(elec_res.is_ok());
    const auto& elec = elec_res.value();
    REQUIRE(std::abs(elec.electrical_conductivity - 7.0e6) < 1e-3);
    REQUIRE(std::abs(elec.electrical_resistivity - (1.0 / 7.0e6)) < 1e-12);
    REQUIRE(std::abs(elec.relative_permeability - 150.0) < 1e-3);

    // 4. Fluid Domain Adapter (Water test)
    MaterialDefinition water(base::Uuid::generate_random(), "Water", MaterialClassification::ChemicalCompound);
    water.set_reference_density(998.2);
    water.set_scalar_property("dynamic_viscosity", 1.002e-3);
    water.set_scalar_property("bulk_modulus", 2.2e9);

    auto fl_res = MaterialDomainAdapter::extract_fluid(water, state);
    REQUIRE(fl_res.is_ok());
    const auto& fl = fl_res.value();
    REQUIRE(std::abs(fl.dynamic_viscosity - 1.002e-3) < 1e-6);
    REQUIRE(fl.speed_of_sound > 1400.0);

    // 5. Optical Domain Adapter (Glass test)
    MaterialDefinition glass(base::Uuid::generate_random(), "Glass", MaterialClassification::ChemicalCompound);
    glass.set_scalar_property("refractive_index_real", 1.52);
    glass.set_scalar_property("refractive_index_imag", 0.0);

    auto opt_res = MaterialDomainAdapter::extract_optical(glass, state);
    REQUIRE(opt_res.is_ok());
    const auto& opt = opt_res.value();
    REQUIRE(std::abs(opt.refractive_index_real - 1.52) < 1e-4);
    // Reflectivity R = ((1.52 - 1)/(1.52 + 1))^2 = (0.52 / 2.52)^2 = 0.2063^2 = 0.04257
    REQUIRE(std::abs(opt.reflectivity - 0.04257) < 1e-3);
}

TEST_CASE("Per-region diagnostics for unsupported conditions and missing properties (MATRT-12)") {
    auto mat_id = base::Uuid::generate_random();
    MaterialDefinition mat(mat_id, "Incomplete Metal", MaterialClassification::PureElement);
    // Only density is provided, missing conductivity and moduli
    mat.set_scalar_property("density", 5000.0);

    MaterialRegion reg(base::Uuid::generate_random(), "Bracket", mat_id, "geom/solid_1");
    reg.set_volume_m3(0.01);

    // Nominal conditions
    reg.state().set_temperature(300.0);
    reg.state().set_pressure(101325.0);

    // Diagnostics with Thermal and Mechanical solvers active
    auto diag = RegionDiagnosticsEngine::diagnose_region(reg, mat, {"Thermal", "Mechanical"});
    REQUIRE(diag.has_errors());
    // Missing thermal conductivity
    REQUIRE(diag.error_count() >= 2);

    // Extreme condition: temperature below absolute zero
    reg.state().set_temperature(-15.0);
    auto diag_cold = RegionDiagnosticsEngine::diagnose_region(reg, mat, {});
    REQUIRE(diag_cold.has_errors());

    // Volume zero or negative
    reg.set_volume_m3(0.0);
    auto diag_zero_vol = RegionDiagnosticsEngine::diagnose_region(reg, mat, {});
    REQUIRE(diag_zero_vol.has_errors());
}
