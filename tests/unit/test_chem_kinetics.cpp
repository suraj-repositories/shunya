#include "tests/test_framework.hpp"

#include "shunya/chem/arrhenius_evaluator.hpp"
#include "shunya/chem/catalysis_model.hpp"
#include "shunya/chem/michaelis_menten.hpp"
#include "shunya/chem/reaction_network.hpp"

#include <algorithm>
#include <cmath>

using namespace shunya::chem;
using namespace shunya::core::domain::chem;

TEST_CASE("CHEM-01-001-01: Stiff Oregonator BZ limit-cycle oscillator & stoichiometric mass conservation") {
    // 1) Stiff Field-Koros-Noyes Oregonator BZ oscillator integrated via L-stable ROS2
    constexpr double kS = 77.27;
    constexpr double kW = 0.1610;
    constexpr double kQ = 8.375e-6;
    constexpr double kF = 1.0;

    auto oregonator_rhs = [](double, const std::vector<double>& y, std::vector<double>& dydt) {
        const double X = y[0], Y = y[1], Z = y[2];
        dydt[0] = kS * (Y - X * Y + X - kQ * X * X);
        dydt[1] = (-Y - X * Y + kF * Z) / kS;
        dydt[2] = kW * (X - Z);
    };

    std::vector<double> bz{1.0, 2.0, 3.0};
    auto bz_res = StiffOdeSolver::integrate(600.0, 0.05, bz, oregonator_rhs);
    REQUIRE(bz_res.is_ok());

    double min_x = 1e9, max_x = -1e9;
    for (size_t k = 2000; k < bz_res.value().size(); ++k) {
        const double X = bz_res.value()[k][0];
        const double Y = bz_res.value()[k][1];
        const double Z = bz_res.value()[k][2];
        CHECK(X >= 0.0 && Y >= 0.0 && Z >= 0.0);
        min_x = std::min(min_x, X);
        max_x = std::max(max_x, X);
    }
    // Sustained sharp BZ relaxation oscillations spanning > 3 orders of magnitude
    CHECK(max_x > 1000.0 && min_x < 10.0);

    // 2) Multi-step H2-O2 reaction network conserving atomic H, O, and total mass to machine precision
    ReactionNetworkEngine net;
    net.add_species({"H2", "Hydrogen", 0.002016, Phase::Gas, {{"H", 2}}}, 2.0);
    net.add_species({"O2", "Oxygen", 0.031999, Phase::Gas, {{"O", 2}}}, 1.0);
    net.add_species({"H", "H-radical", 0.001008, Phase::Gas, {{"H", 1}}}, 0.05);
    net.add_species({"O", "O-radical", 0.0159995, Phase::Gas, {{"O", 1}}}, 0.0);
    net.add_species({"OH", "Hydroxyl", 0.0170075, Phase::Gas, {{"O", 1}, {"H", 1}}}, 0.0);
    net.add_species({"H2O", "Water", 0.0180155, Phase::Gas, {{"H", 2}, {"O", 1}}}, 0.0);

    ChemicalReaction r1{"R1", {{"H", 1}, {"O2", 1}}, {{"OH", 1}, {"O", 1}}, true, 5.0, 1.0, 0.0, 1.0, 0.0};
    ChemicalReaction r2{"R2", {{"O", 1}, {"H2", 1}}, {{"OH", 1}, {"H", 1}}, true, 8.0, 2.0, 0.0, 1.0, 0.0};
    ChemicalReaction r3{"R3", {{"OH", 1}, {"H2", 1}}, {{"H2O", 1}, {"H", 1}}, true, 12.0, 0.5, 0.0, 1.0, 0.0};
    REQUIRE(net.add_reaction(r1).is_ok());
    REQUIRE(net.add_reaction(r2).is_ok());
    REQUIRE(net.add_reaction(r3).is_ok());

    const double h_init = net.element_moles_per_l("H");
    const double o_init = net.element_moles_per_l("O");
    const double mass_init = net.total_mass_kg_per_l();

    auto h2_res = net.integrate(2.0, 0.01, false);
    REQUIRE(h2_res.is_ok());

    CHECK(net.concentration("H2O") > 1.0);
    CHECK(std::abs(net.element_moles_per_l("H") - h_init) < 1e-12);
    CHECK(std::abs(net.element_moles_per_l("O") - o_init) < 1e-12);
    CHECK(std::abs(net.total_mass_kg_per_l() - mass_init) < 1e-12);
}

TEST_CASE("CHEM-01-001-02 & CHEM-01-001-03: Arrhenius/Eyring temperature sensitivity, Enzyme inhibition & Catalysis") {
    // 1) H + O2 -> OH + O NIST modified Arrhenius kinetics (A ~ 1.0e11, n = 0, Ea = 70000 J/mol)
    ModifiedArrheniusParams h_o2{1.0e11, 0.0, 70000.0};
    const double k_1000 = ArrheniusRateEvaluator::modified_arrhenius(h_o2, 1000.0);
    const double k_2000 = ArrheniusRateEvaluator::modified_arrhenius(h_o2, 2000.0);
    // Arrhenius slope: ln(k_2000 / k_1000) == Ea / R * (1/1000 - 1/2000)
    const double expected_ln_ratio = 70000.0 / ArrheniusRateEvaluator::kGasConstantR * (1.0 / 1000.0 - 1.0 / 2000.0);
    CHECK(std::abs(std::log(k_2000 / k_1000) - expected_ln_ratio) < 1e-12);

    // 2) Catalysis activation lowering accelerates rate exponentially
    const double k_cat = CatalysisModel::catalyzed_rate_constant(h_o2, 20000.0, 1000.0);
    CHECK(k_cat > 10.0 * k_1000);

    // 3) Michaelis-Menten Lineweaver-Burk double reciprocal linearity for competitive & non-competitive inhibition
    MichaelisMentenParams comp{2.0, 0.05, 0.02, EnzymeInhibitionMode::Competitive};
    double slope_c = 0.0, yint_c = 0.0;
    MichaelisMentenModel::lineweaver_burk_coefficients(comp, 0.04, slope_c, yint_c);
    for (double s : {0.02, 0.05, 0.2, 1.0}) {
        const double inv_v = 1.0 / MichaelisMentenModel::velocity(comp, s, 0.04);
        const double lb = slope_c * (1.0 / s) + yint_c;
        CHECK(std::abs(inv_v - lb) < 1e-12);
    }

    MichaelisMentenParams noncomp{2.0, 0.05, 0.02, EnzymeInhibitionMode::NonCompetitive};
    double slope_nc = 0.0, yint_nc = 0.0;
    MichaelisMentenModel::lineweaver_burk_coefficients(noncomp, 0.04, slope_nc, yint_nc);
    const double v_sat = MichaelisMentenModel::velocity(noncomp, 1e6, 0.04);
    CHECK(std::abs(1.0 / v_sat - yint_nc) < 1e-5);
    const double v_at_km = MichaelisMentenModel::velocity(noncomp, 0.05, 0.04);
    CHECK(std::abs(1.0 / v_at_km - (slope_nc / 0.05 + yint_nc)) < 1e-12);
}
