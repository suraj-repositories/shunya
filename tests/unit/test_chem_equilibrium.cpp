#include "tests/test_framework.hpp"

#include "shunya/chem/equilibrium_shifter.hpp"
#include "shunya/chem/gibbs_minimizer.hpp"

#include <cmath>

using namespace shunya::chem;

TEST_CASE("CHEM-01-002-01: GibbsMinimizer Haber-Bosch ammonia equilibrium (450C, 200 atm) & Water-Gas Shift") {
    // 1) Haber-Bosch: N2 + 3 H2 <=> 2 NH3 at T = 723.15 K (450 C), P = 202.65 bar (200 atm)
    // NIST standard properties at 298.15 K:
    //   N2:  Hf = 0,          S = 191.61 J/(mol K), Cp ~ 29.1
    //   H2:  Hf = 0,          S = 130.68 J/(mol K), Cp ~ 28.8
    //   NH3: Hf = -45900,     S = 192.77 J/(mol K), Cp ~ 35.1 -> at 723 K Kp ~ 6.5e-3 bar^-2 (0.5 N2 + 1.5 H2 -> NH3: sqrt(Kp) ~ 0.08)
    std::vector<ThermoSpeciesEntry> haber{
        {"N2",  {{"N", 2}},          0.0,     191.61, 29.1},
        {"H2",  {{"H", 2}},          0.0,     130.68, 28.8},
        {"NH3", {{"N", 1}, {"H", 3}}, -45900.0, 192.77, 35.1}
    };

    const double T_hb = 723.15;
    const double P_hb = 202.65; // 200 atm
    auto hb = GibbsMinimizer::minimize(haber, {1.0, 3.0, 0.0}, T_hb, P_hb);
    REQUIRE(hb.is_ok());

    const double y_nh3 = hb.value().fraction_map.at("NH3");
    const double y_n2  = hb.value().fraction_map.at("N2");
    const double y_h2  = hb.value().fraction_map.at("H2");

    // Verify exact thermodynamic Kp = P_NH3^2 / (P_N2 * P_H2^3) = exp(-Delta G0(T) / RT) to < 0.01%
    const double dg0 = 2.0 * haber[2].standard_gibbs_j_mol(T_hb) -
                       (haber[0].standard_gibbs_j_mol(T_hb) + 3.0 * haber[1].standard_gibbs_j_mol(T_hb));
    const double kp_exact = std::exp(-dg0 / (GibbsMinimizer::kGasConstantR * T_hb));
    const double kp_obs = std::pow(y_nh3 * P_hb, 2) / ((y_n2 * P_hb) * std::pow(y_h2 * P_hb, 3));
    CHECK(std::abs(kp_obs - kp_exact) / kp_exact < 1e-6);
    // Ideal-gas Haber-Bosch NH3 equilibrium mole fraction at 450 C, 200 atm is ~0.235 (~23.5% mole fraction -> ~38% N2 conversion)
    CHECK(y_nh3 > 0.20 && y_nh3 < 0.30);

    // 2) Water-Gas Shift: CO + H2O <=> CO2 + H2 across 400 K -> 1500 K
    std::vector<ThermoSpeciesEntry> wgs{
        {"CO",  {{"C", 1}, {"O", 1}},          -110530.0, 197.66, 29.1},
        {"H2O", {{"H", 2}, {"O", 1}},          -241826.0, 188.84, 33.6},
        {"CO2", {{"C", 1}, {"O", 2}},          -393510.0, 213.78, 37.1},
        {"H2",  {{"H", 2}},                     0.0,      130.68, 28.8}
    };
    double prev_y_co2 = 1.0;
    for (double T : {400.0, 700.0, 1000.0, 1500.0}) {
        auto r = GibbsMinimizer::minimize(wgs, {1.0, 1.0, 0.0, 0.0}, T, 1.0);
        REQUIRE(r.is_ok());
        const double y_co2 = r.value().fraction_map.at("CO2");
        const double y_h2  = r.value().fraction_map.at("H2");
        const double y_co  = r.value().fraction_map.at("CO");
        const double y_h2o = r.value().fraction_map.at("H2O");
        const double k_wgs = (y_co2 * y_h2) / (y_co * y_h2o);
        const double dg_wgs = (wgs[2].standard_gibbs_j_mol(T) + wgs[3].standard_gibbs_j_mol(T)) -
                              (wgs[0].standard_gibbs_j_mol(T) + wgs[1].standard_gibbs_j_mol(T));
        const double k_th = std::exp(-dg_wgs / (GibbsMinimizer::kGasConstantR * T));
        CHECK(std::abs(k_wgs - k_th) / k_th < 1e-6);
        // Exothermic WGS shifts left (less CO2) as temperature increases
        CHECK(y_co2 < prev_y_co2);
        prev_y_co2 = y_co2;
    }
}

TEST_CASE("CHEM-01-002-02: Le Chatelier shifts & van 't Hoff slope for N2O4 <=> 2 NO2") {
    // Endothermic dissociation N2O4(g) <=> 2 NO2(g): Delta H0 ~ +57.2 kJ/mol
    // NIST 298.15 K:
    //   N2O4: Hf = +9160 J/mol,  S = 304.38 J/(mol K), Cp ~ 77.3
    //   NO2:  Hf = +33180 J/mol, S = 240.04 J/(mol K), Cp ~ 37.2
    std::vector<ThermoSpeciesEntry> nox{
        {"N2O4", {{"N", 2}, {"O", 4}},  9160.0, 304.38, 77.3},
        {"NO2",  {{"N", 1}, {"O", 2}}, 33180.0, 240.04, 37.2}
    };

    // 1) Compressing from 0.5 bar to 5.0 bar at 298.15 K increases colorless dimer N2O4 fraction
    auto low_p  = GibbsMinimizer::minimize(nox, {1.0, 0.0}, 298.15, 0.5);
    auto high_p = GibbsMinimizer::minimize(nox, {1.0, 0.0}, 298.15, 5.0);
    REQUIRE(low_p.is_ok() && high_p.is_ok());
    CHECK(high_p.value().fraction_map.at("N2O4") > low_p.value().fraction_map.at("N2O4"));

    // 2) Heating from 298.15 K to 330 K shifts toward brown NO2 and recovers van 't Hoff Delta_H0 (~57 kJ/mol)
    auto shift = EquilibriumShiftController::evaluate_shift(
        nox, {1.0, 0.0}, 298.15, 1.0, 330.0, 1.0, "NO2", 2, "N2O4", 1);
    REQUIRE(shift.is_ok());
    CHECK(shift.value().perturbed.fraction_map.at("NO2") > shift.value().baseline.fraction_map.at("NO2"));

    // Expected average Delta H0 over 298..330 K is ~57,100 J/mol
    CHECK(std::abs(shift.value().implied_delta_h_j_mol - 57100.0) < 500.0);
}
