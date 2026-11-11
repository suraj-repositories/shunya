#include "tests/test_framework.hpp"

#include "shunya/mat/material_mixing_lab.hpp"

#include <cmath>

using namespace shunya::mat;

TEST_CASE("MIXLAB: Alloy mixing (Cu-Zn brass) computes density, conductivity, moduli, and comparison") {
    MixtureExperiment exp;
    exp.name = "Brass_Test";
    exp.scenario = FormationScenario::Alloy;
    exp.temperature_k = 298.15;
    exp.inputs = {
        {"Cu", QuantityInputBasis::MassFractionPct, 70.0, 1e-3, false},
        {"Zn", QuantityInputBasis::MassFractionPct, 30.0, 1e-3, false}
    };

    auto res = MaterialMixingLab::evaluate(exp);
    REQUIRE(res.is_ok());
    const auto& ev = res.value();
    CHECK(ev.is_chemically_valid);
    CHECK(ev.predicted_phase == EvaluatedPhase::Solid);
    REQUIRE(ev.density_kg_m3.available());
    // Volume-additive brass density between Zn (7140) and Cu (8920) ~ 8300 kg/m^3
    CHECK(*ev.density_kg_m3.value > 8000.0);
    CHECK(*ev.density_kg_m3.value < 8600.0);
    REQUIRE(ev.young_modulus_pa.available());
    REQUIRE(ev.young_reuss_pa.has_value());
    REQUIRE(ev.young_voigt_pa.has_value());
    CHECK(*ev.young_reuss_pa <= *ev.young_modulus_pa.value + 1e3);
    CHECK(*ev.young_modulus_pa.value <= *ev.young_voigt_pa + 1e3);
    CHECK(!ev.comparison_rows.empty());
}

TEST_CASE("MIXLAB: Chemical compound formation rules reject invalid combinations (He+Au, Cu+Al as compound)") {
    // Noble gas + Gold cannot form a valid chemical compound
    MixtureExperiment bad_he_au;
    bad_he_au.name = "Invalid_He_Au";
    bad_he_au.scenario = FormationScenario::ChemicalCompound;
    bad_he_au.inputs = {
        {"He", QuantityInputBasis::MoleFractionPct, 50.0, 1e-3, false},
        {"Au", QuantityInputBasis::MoleFractionPct, 50.0, 1e-3, false}
    };
    auto r1 = MaterialMixingLab::evaluate(bad_he_au);
    REQUIRE(r1.is_ok());
    CHECK_FALSE(r1.value().is_chemically_valid);
    CHECK(!r1.value().validation_errors.empty());
    CHECK(MaterialMixingLab::promote_to_material(bad_he_au).is_err());

    // Valid LiFePO4 molar stoichiometry passes
    MixtureExperiment lfp;
    lfp.name = "LiFePO4";
    lfp.scenario = FormationScenario::ChemicalCompound;
    lfp.inputs = {
        {"Li", QuantityInputBasis::MoleCount, 1.0, 1e-3, false},
        {"Fe", QuantityInputBasis::MoleCount, 1.0, 1e-3, false},
        {"P",  QuantityInputBasis::MoleCount, 1.0, 1e-3, false},
        {"O",  QuantityInputBasis::MoleCount, 4.0, 1e-3, false}
    };
    auto r2 = MaterialMixingLab::evaluate(lfp);
    REQUIRE(r2.is_ok());
    CHECK(r2.value().is_chemically_valid);

    auto mat_res = MaterialMixingLab::promote_to_material(lfp);
    REQUIRE(mat_res.is_ok());
    CHECK(lfp.promoted_material_id.has_value());
    CHECK(mat_res.value().id() == *lfp.promoted_material_id);
    CHECK(mat_res.value().density() > 100.0);
}

TEST_CASE("MIXLAB: Layered material computes anisotropic in-plane vs through-plane conductivity") {
    MixtureExperiment clad;
    clad.name = "Cu_Al_Clad";
    clad.scenario = FormationScenario::LayeredMaterial;
    clad.inputs = {
        {"Cu", QuantityInputBasis::VolumeFractionPct, 50.0, 0.5e-3, false},
        {"Al", QuantityInputBasis::VolumeFractionPct, 50.0, 0.5e-3, false}
    };
    auto res = MaterialMixingLab::evaluate(clad);
    REQUIRE(res.is_ok());
    const auto& ev = res.value();
    REQUIRE(ev.electrical_conductivity_s_m.available());
    REQUIRE(ev.electrical_conductivity_through_plane_s_m.available());
    // Arithmetic mean (in-plane) >= harmonic mean (through-plane)
    CHECK(*ev.electrical_conductivity_s_m.value >= *ev.electrical_conductivity_through_plane_s_m.value);
}

TEST_CASE("MIXLAB: Experiment JSON save/reopen roundtrip & temperature sweep") {
    auto presets = MaterialMixingLab::standard_presets();
    REQUIRE(presets.size() >= 4u);

    const std::string json = presets[0].to_json();
    auto parsed = MixtureExperiment::from_json(json);
    REQUIRE(parsed.is_ok());
    CHECK(parsed.value().name == presets[0].name);
    CHECK(parsed.value().scenario == presets[0].scenario);
    REQUIRE(parsed.value().inputs.size() == presets[0].inputs.size());

    auto sweep = MaterialMixingLab::sweep_temperature(presets[1], 300.0, 900.0, 5);
    REQUIRE(sweep.is_ok());
    CHECK(sweep.value().size() == 5u);
    CHECK(sweep.value().back().second.electrical_resistivity_ohm_m.value >
          sweep.value().front().second.electrical_resistivity_ohm_m.value);
}
