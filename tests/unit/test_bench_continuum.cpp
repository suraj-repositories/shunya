#include "tests/test_framework.hpp"

#include "shunya/bench/exact_navier_stokes.hpp"
#include "shunya/bench/fourier_series_heat.hpp"
#include "shunya/bench/nist_janaf_benchmarks.hpp"
#include "shunya/chem/gibbs_minimizer.hpp"
#include "shunya/matdb/shomate_evaluator.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <cmath>
#include <vector>

using namespace shunya::bench;
using namespace shunya::phy;
using namespace shunya::matdb;

TEST_CASE("TEST-01-002 1D transient ThermalSolver3D vs 500-term analytical Fourier series (<0.25% error)") {
    ThermalGridSpec3D spec;
    spec.nx = 41;
    spec.ny = 3;
    spec.nz = 3;
    spec.lx_m = 1.0;
    spec.ly_m = 0.1;
    spec.lz_m = 0.1;
    spec.density_kg_m3 = 2000.0;
    spec.specific_heat_j_kg_k = 1000.0;
    spec.conductivity_w_m_k = 200.0; // alpha = k/(rho*cp) = 1e-4 m^2/s
    spec.initial_temp_k = 1.0;       // Normalized T(x,0) = 1.0, boundaries T=0.0

    ThermalSolver3D solver(spec);
    solver.set_dirichlet_x_min_plane(0.0);
    solver.set_dirichlet_x_max_plane(0.0);

    const double alpha = spec.conductivity_w_m_k / (spec.density_kg_m3 * spec.specific_heat_j_kg_k);
    const double t_total = 400.0; // Fo = alpha * t / L^2 = 0.04
    const double fo = alpha * t_total / (spec.lx_m * spec.lx_m);

    REQUIRE(solver.advance(t_total, 2.0).is_ok());

    double err_sq = 0.0, ref_sq = 0.0;
    for (size_t ix = 1; ix + 1 < spec.nx; ++ix) {
        const double x_over_l = static_cast<double>(ix) / static_cast<double>(spec.nx - 1);
        const double t_exact = BenchmarkThermalDiffusion::bar_sine_fourier_500(x_over_l, fo);
        const double t_num = solver.temperature(ix, 1, 1);
        const double e = t_num - t_exact;
        err_sq += e * e;
        ref_sq += t_exact * t_exact;
    }
    const double l2_rel = std::sqrt(err_sq / ref_sq);
    CHECK(l2_rel < 0.0025); // < 0.25% L2 error across space

    // Verify 500-term convective Robin Fourier series across Biot numbers 0.01 to 100
    const double th_low_bi = BenchmarkThermalDiffusion::slab_convective_fourier_500(0.0, 0.5, 0.01);
    const double th_high_bi = BenchmarkThermalDiffusion::slab_convective_fourier_500(0.0, 0.5, 100.0);
    CHECK(th_low_bi > 0.99);
    CHECK(th_high_bi < 0.40);
}

TEST_CASE("TEST-01-002 Laminar Poiseuille & Couette validation across Re = 10, 50, 100, 500 (<0.5% error)") {
    const std::vector<double> re_values{10.0, 50.0, 100.0, 500.0};
    for (double re : re_values) {
        const auto res = BenchmarkNavierStokes::validate_channel_poiseuille(0.1, 1000.0, 0.05, re, 81);
        CHECK(res.centerline_velocity_error_rel < 0.005);
        CHECK(res.wall_shear_error_rel < 0.005);
        CHECK(res.profile_l2_error_rel < 0.005);
    }

    // Couette analytical profile spot check
    CHECK(std::abs(BenchmarkNavierStokes::exact_couette_u(0.025, 0.10, 2.0) - 0.5) < 1e-12);
    CHECK(std::abs(BenchmarkNavierStokes::exact_pipe_poiseuille_u(0.0, 0.05, 10.0, 0.001) - 6.25) < 1e-9);
}

TEST_CASE("TEST-01-002 NIST-JANAF methane reforming & ammonia synthesis Kp(T) across 300-2000 K") {
    // Certified NIST Chemistry WebBook Shomate segments & standard 298.15 K formation enthalpies (kJ/mol):
    const JanafSpeciesRef ch4{-74.60, {298.0, 1300.0, "gas",
        -0.703029, 108.4773, -42.52157, 5.862788, 0.678565, -76.84376, 158.7163, -74.87310}};
    const JanafSpeciesRef h2o{-241.826, {500.0, 1700.0, "gas",
        30.09200, 6.832514, 6.793435, -2.534480, 0.082139, -250.8810, 223.3967, -241.8264}};
    const JanafSpeciesRef co{-110.53, {298.0, 1300.0, "gas",
        25.56759, 6.096130, 4.054656, -2.671301, 0.131021, -118.0089, 227.3665, -110.5271}};
    const JanafSpeciesRef h2{0.0, {298.0, 1000.0, "gas",
        33.066178, -11.363417, 11.432816, -2.772874, -0.158558, -9.980797, 172.707974, 0.0}};
    const JanafSpeciesRef n2{0.0, {500.0, 2000.0, "gas",
        19.50583, 19.88705, -8.598535, 1.369784, 0.527601, -4.935202, 212.3900, 0.0}};
    const JanafSpeciesRef nh3{-45.94, {298.0, 1400.0, "gas",
        19.99563, 49.77119, -15.37599, 1.921168, 0.189174, -53.30667, 203.8591, -45.89806}};

    // 1. Methane Steam Reforming CH4 + H2O -> CO + 3 H2 at 1000 K (endothermic, strongly favorable log10 Kp ~ +1.42)
    const double log10kp_smr_1000 = BenchmarkChemicalEquilibrium::log10_kp_from_shomate(
        {{&co, +1.0}, {&h2, +3.0}, {&ch4, -1.0}, {&h2o, -1.0}}, 1000.0);
    // NIST-JANAF SMR at 1000 K: DeltaG0 ~ -27.2 kJ/mol -> log10 Kp ~ +1.42
    CHECK(std::abs(log10kp_smr_1000 - 1.42) < 0.15);

    // At 300 K SMR is non-spontaneous (log10 Kp << 0)
    const double log10kp_smr_300 = BenchmarkChemicalEquilibrium::log10_kp_from_shomate(
        {{&co, +1.0}, {&h2, +3.0}, {&ch4, -1.0}, {&h2o, -1.0}}, 300.0);
    CHECK(log10kp_smr_300 < -20.0);

    // 2. Ammonia synthesis N2 + 3 H2 -> 2 NH3 across T: exothermic so log10 Kp > 0 at 300 K (~5.76) and < 0 at 800 K (~-4.2)
    const double log10kp_nh3_300 = BenchmarkChemicalEquilibrium::log10_kp_from_shomate(
        {{&nh3, +2.0}, {&n2, -1.0}, {&h2, -3.0}}, 300.0);
    const double log10kp_nh3_800 = BenchmarkChemicalEquilibrium::log10_kp_from_shomate(
        {{&nh3, +2.0}, {&n2, -1.0}, {&h2, -3.0}}, 800.0);
    CHECK(log10kp_nh3_300 > 5.0);
    CHECK(log10kp_nh3_800 < -3.5);
}
