#include "tests/test_framework.hpp"

#include "shunya/chem/chemo_thermal_coupler.hpp"
#include "shunya/chem/reaction_enthalpy.hpp"

#include <cmath>

using namespace shunya::chem;
using namespace shunya::phy;

TEST_CASE("CHEM-02-001-01: Hess Law standard heat of combustion of Methane (-890.7 kJ/mol < 0.1%) & Hydrocarbons") {
    // 1) Methane CH4 + 2 O2 -> CO2 + 2 H2O(l) at 298.15 K
    // Exact = -393.51 + 2*(-285.83) - (-74.87) = -890.30 kJ/mol (HHV; reference ~ -890.7 kJ/mol within 0.05%)
    const auto res_ch4 = ReactionEnthalpyCalculator::reaction_enthalpy_kj_mol({
        {"CH4", -1.0}, {"O2", -2.0}, {"CO2", 1.0}, {"H2O(l)", 2.0}
    }, 298.15);
    REQUIRE(res_ch4.is_ok());
    CHECK(std::abs(res_ch4.value() - (-890.7)) / 890.7 < 1e-3); // Strictly < 0.1%

    // 2) Propane C3H8 + 5 O2 -> 3 CO2 + 4 H2O(l) (~ -2219.2 kJ/mol)
    const auto res_c3h8 = ReactionEnthalpyCalculator::reaction_enthalpy_kj_mol({
        {"C3H8", -1.0}, {"O2", -5.0}, {"CO2", 3.0}, {"H2O(l)", 4.0}
    }, 298.15);
    REQUIRE(res_c3h8.is_ok());
    CHECK(std::abs(res_c3h8.value() - (-2219.2)) / 2219.2 < 1e-3);

    // 3) Ethanol C2H5OH + 3 O2 -> 2 CO2 + 3 H2O(l) (~ -1366.8 kJ/mol)
    const auto res_etoh = ReactionEnthalpyCalculator::reaction_enthalpy_kj_mol({
        {"C2H5OH", -1.0}, {"O2", -3.0}, {"CO2", 2.0}, {"H2O(l)", 3.0}
    }, 298.15);
    REQUIRE(res_etoh.is_ok());
    CHECK(std::abs(res_etoh.value() - (-1366.8)) / 1366.8 < 1e-3);

    // 4) Octane C8H18 + 12.5 O2 -> 8 CO2 + 9 H2O(l) (~ -5470 kJ/mol)
    const auto res_oct = ReactionEnthalpyCalculator::reaction_enthalpy_kj_mol({
        {"C8H18", -1.0}, {"O2", -12.5}, {"CO2", 8.0}, {"H2O(l)", 9.0}
    }, 298.15);
    REQUIRE(res_oct.is_ok());
    CHECK(std::abs(res_oct.value() - (-5470.4)) / 5470.4 < 2e-3);
}

TEST_CASE("CHEM-02-001-02: Exact adiabatic temperature rise, Thermal Runaway ignition & ThermalSolver3D coupling") {
    // 1) Adiabatic exothermic complete conversion: Delta T_ad = C0 * (-Delta H) / (rho * cp)
    AdiabaticReactorConfig cfg;
    cfg.initial_conc_mol_m3 = 500.0;
    cfg.initial_temp_k = 500.0;
    cfg.ambient_temp_k = 500.0;
    cfg.density_kg_m3 = 1000.0;
    cfg.cp_j_kg_k = 2000.0;
    cfg.delta_h_rxn_j_mol = -400000.0;
    cfg.heat_loss_u_w_m3_k = 0.0; // Strictly insulated adiabatic
    cfg.rate_params = {5.0e6, 0.0, 60000.0};

    // Theoretical Delta T = 500 * 400,000 / (1000 * 2000) = +100.0 K => T_final = 600.0 K
    auto runaway = ChemoThermalCoupler::simulate_reactor(cfg, 60.0, 0.05);
    REQUIRE(runaway.is_ok());
    const auto& last = runaway.value().back();
    CHECK(last.conc_mol_m3 < 1e-3);
    CHECK(std::abs(last.temp_k - 600.0) < 0.05);

    // 2) Coupling Q_gen directly into 3D thermal grid raises cell temperature above ambient
    ThermalGridSpec3D tspec;
    tspec.nx = 5; tspec.ny = 3; tspec.nz = 3;
    tspec.lx_m = 0.1; tspec.ly_m = 0.06; tspec.lz_m = 0.06;
    tspec.initial_temp_k = 450.0;
    ThermalSolver3D thermal(tspec);
    const double q_injected = ChemoThermalCoupler::couple_to_thermal_cell(
        thermal, 2, 1, 1, 500.0, cfg.rate_params, cfg.delta_h_rxn_j_mol);
    CHECK(q_injected > 1e5);
    REQUIRE(thermal.advance(5.0, 0.5).is_ok());
    CHECK(thermal.temperature(2, 1, 1) > 450.5);
}
