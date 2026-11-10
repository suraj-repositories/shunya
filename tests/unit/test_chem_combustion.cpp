#include "tests/test_framework.hpp"

#include "shunya/chem/combustion_model.hpp"
#include "shunya/chem/explosion_pressure.hpp"

#include <cmath>

using namespace shunya::chem;

TEST_CASE("CHEM-02-002-01: Hydrocarbon flammability limits (CH4 < 5% LFL or < 600C fails) & O2 depletion") {
    // 1) Lean methane below 5% LFL in air (4.0% vol at 950 K) does NOT ignite
    auto lean = CombustionKineticsModel::simulate_combustion(
        FuelKind::Methane, 4.0, 20.16, 75.84, 950.0, 1.0);
    REQUIRE(lean.is_ok());
    CHECK(!lean.value().ignited);
    CHECK(std::abs(lean.value().fuel_mol_m3 - 4.0) < 1e-12);

    // 2) Stoichiometric methane (~9.5% vol) below 600 C (873.15 K), e.g. at 800 K, does NOT auto-ignite
    auto cold = CombustionKineticsModel::simulate_combustion(
        FuelKind::Methane, 9.5, 19.0, 71.5, 800.0, 1.0);
    REQUIRE(cold.is_ok());
    CHECK(!cold.value().ignited);

    // 3) Stoichiometric Propane-air (phi = 1.0: 1 C3H8 + 5 O2 + 18.8 N2) above auto-ignition T burns O2 to depletion
    auto burn = CombustionKineticsModel::simulate_combustion(
        FuelKind::Propane, 1.0, 5.0, 18.8, 900.0, 0.5, 0.0005);
    REQUIRE(burn.is_ok());
    CHECK(burn.value().ignited);
    CHECK(burn.value().fuel_mol_m3 < 1e-3);
    CHECK(burn.value().o2_mol_m3 < 1e-2);
    CHECK(burn.value().co2_mol_m3 > 2.9);
    CHECK(burn.value().h2o_mol_m3 > 3.9);
}

TEST_CASE("CHEM-02-002-02: Methane adiabatic flame temperature (~2220 K) & closed-vessel deflagration P_max") {
    // 1) Methane-air stoichiometric adiabatic flame temperature (~2220 K with Shomate + high-T dissociation)
    auto ch4_exp = ExplosionPressureCalculator::compute_closed_vessel_explosion(FuelKind::Methane, 1.0, 298.15, 1.01325);
    REQUIRE(ch4_exp.is_ok());
    CHECK(std::abs(ch4_exp.value().adiabatic_flame_cp_k - 2220.0) < 55.0);
    // Constant-volume explosion temperature & peak pressure (~8-9.5 bar)
    CHECK(ch4_exp.value().flame_temp_cv_k > 2500.0 && ch4_exp.value().flame_temp_cv_k < 2900.0);
    CHECK(ch4_exp.value().peak_pressure_bar > 8.0 && ch4_exp.value().peak_pressure_bar < 10.0);

    // 2) Hydrogen-air stoichiometric adiabatic flame (~2350-2550 K) & closed-vessel pressure (~7.5-9.5 bar)
    auto h2_exp = ExplosionPressureCalculator::compute_closed_vessel_explosion(FuelKind::Hydrogen, 1.0, 298.15, 1.01325);
    REQUIRE(h2_exp.is_ok());
    CHECK(h2_exp.value().adiabatic_flame_cp_k > 2300.0 && h2_exp.value().adiabatic_flame_cp_k < 2600.0);
    CHECK(h2_exp.value().peak_pressure_bar > 7.5 && h2_exp.value().peak_pressure_bar < 9.5);
}
