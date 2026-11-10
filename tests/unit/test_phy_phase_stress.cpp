#include "tests/test_framework.hpp"

#include "shunya/phy/phase_change_solver.hpp"
#include "shunya/phy/stefan_validator.hpp"
#include "shunya/phy/thermo_stress_solver.hpp"

#include <cmath>

using namespace shunya::phy;
using namespace shunya::mat;

TEST_CASE("PHY-02-002-01: Ice melting at 0C absorbs 334 kJ/kg plateau & 1D Stefan front tracks sqrt(t)") {
    // 1) 1 kg ice warming from -10 C -> 0 C plateau -> +10 C liquid at constant 1000 W
    auto ice_params = PhaseChangeEnthalpyModel::water_ice().parameters();
    const auto trace = EnthalpyPhaseChangeSolver::simulate_lumped_melting(
        1.0, 263.15, 1000.0, ice_params, 400.0, 1.0);
    REQUIRE(!trace.empty());

    // Solid warming time from -10C to 0C = 1 * 2108 * 10 / 1000 = 21.08 s
    // Latent melting plateau from ~22 s to ~(21.08 + 334) = 355.08 s at exact 273.15 K
    double plateau_min_T = 1e9;
    double plateau_max_T = -1e9;
    for (const auto& sample : trace) {
        if (sample.time_s >= 25.0 && sample.time_s <= 350.0) {
            plateau_min_T = std::min(plateau_min_T, sample.temperature_k);
            plateau_max_T = std::max(plateau_max_T, sample.temperature_k);
        }
    }
    CHECK(std::abs(plateau_min_T - 273.15) < 1e-6);
    CHECK(std::abs(plateau_max_T - 273.15) < 1e-6);
    // Absorbed heat at 355 s ~ 355 kJ; at 400 s > 399 kJ, fully molten
    CHECK(trace.back().liquid_fraction == 1.0);
    CHECK(trace.back().temperature_k > 282.0);

    // 2) Two/Single-phase 1D Stefan problem front position s(t) ~ 2 * lambda * sqrt(alpha * t)
    PhaseChangeDomain1D dom;
    dom.num_cells = 120;
    dom.length_m = 0.05;
    dom.wall_temp_k = 323.15;     // +50 C boundary
    dom.initial_temp_k = 273.15;  // Saturated ice at 0 C
    dom.t_melt_k = 273.15;
    dom.density_kg_m3 = 1000.0;
    dom.k_liquid_w_mk = 0.60;
    dom.cp_liquid_j_kg_k = 4184.0;
    dom.latent_heat_j_kg = 334000.0;

    const double St = dom.cp_liquid_j_kg_k * (dom.wall_temp_k - dom.t_melt_k) / dom.latent_heat_j_kg;
    const double alpha_l = dom.k_liquid_w_mk / (dom.density_kg_m3 * dom.cp_liquid_j_kg_k);

    auto r1 = EnthalpyPhaseChangeSolver::solve_1d_stefan(dom, 200.0, 0.2);
    auto r2 = EnthalpyPhaseChangeSolver::solve_1d_stefan(dom, 800.0, 0.2); // 4x time -> 2x interface depth!
    REQUIRE(r1.is_ok() && r2.is_ok());

    const double exact_s1 = StefanValidator::interface_position_m(St, alpha_l, 200.0);
    const double exact_s2 = StefanValidator::interface_position_m(St, alpha_l, 800.0);
    CHECK(std::abs(r1.value().interface_position_m - exact_s1) / exact_s1 < 0.03);
    CHECK(std::abs(r2.value().interface_position_m - exact_s2) / exact_s2 < 0.03);
    // Verify sqrt(t) scaling: s(800) / s(200) == 2.0
    CHECK(std::abs((r2.value().interface_position_m / r1.value().interface_position_m) - 2.0) < 0.02);
}

TEST_CASE("PHY-02-002-02: Constrained compressive thermal stress (-E*alpha*dT) & Timoshenko bimetallic strip") {
    // 1) Constrained steel bar heated by +100 K (E=200 GPa, alpha=12e-6 /K => sigma = -240 MPa)
    const auto stress = ThermoMechanicalCoupler::constrained_thermal_stress(200e9, 0.3, 12e-6, 100.0, false);
    CHECK(std::abs(stress.sxx - (-240e6)) < 1.0);
    CHECK(std::abs(VonMisesEvaluator::von_mises_stress(stress) - 240e6) < 1.0);

    // 2) Bimetallic strip curvature under 100 K temperature elevation vs analytical Timoshenko formula
    BimetallicStripSpec strip;
    strip.e1_pa = 200e9;
    strip.alpha1_per_k = 11e-6;
    strip.thickness1_m = 0.001;
    strip.e2_pa = 110e9;
    strip.alpha2_per_k = 19e-6;
    strip.thickness2_m = 0.0015;
    strip.delta_t_k = 100.0;

    auto res = ThermoMechanicalCoupler::solve_bimetallic_curvature(strip, 128);
    REQUIRE(res.is_ok());
    const double rel_diff = std::abs(res.value().numerical_curvature_1_m - res.value().analytical_timoshenko_curvature_1_m) /
                            std::abs(res.value().analytical_timoshenko_curvature_1_m);
    CHECK(rel_diff < 1e-4);
    CHECK(res.value().bending_radius_m > 1.0 && res.value().bending_radius_m < 3.0);
    CHECK(res.value().peak_von_mises_pa > 1e7);
}
