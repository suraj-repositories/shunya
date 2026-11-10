#include "tests/test_framework.hpp"

#include "shunya/phy/convection_model.hpp"
#include "shunya/phy/radiation_solver.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <cmath>
#include <numbers>

using namespace shunya::phy;

TEST_CASE("PHY-02-001-01: 3D Crank-Nicolson transient heat conduction matches analytical Fourier series < 0.2% error") {
    ThermalGridSpec3D spec;
    spec.nx = 41;
    spec.ny = 3;
    spec.nz = 3;
    spec.lx_m = 1.0;
    spec.ly_m = 0.05;
    spec.lz_m = 0.05;
    // diffusivity alpha = k / (rho * cp) = 200 / (2000 * 1000) = 1e-4 m^2/s
    spec.conductivity_w_m_k = 200.0;
    spec.density_kg_m3 = 2000.0;
    spec.specific_heat_j_kg_k = 1000.0;
    spec.initial_temp_k = 300.0;

    ThermalSolver3D solver(spec);
    solver.set_dirichlet_x_min_plane(400.0);
    solver.set_dirichlet_x_max_plane(300.0);

    const double t_end = 500.0; // seconds -> thermal diffusion length sqrt(alpha * t) ~ 0.22 m
    auto res = solver.advance(t_end, 1.0);
    REQUIRE(res.is_ok());

    const double alpha = spec.conductivity_w_m_k / (spec.density_kg_m3 * spec.specific_heat_j_kg_k);
    const double L = spec.lx_m;
    const double pi = std::numbers::pi;

    for (size_t ix = 1; ix < spec.nx - 1; ++ix) {
        const double x = static_cast<double>(ix) / static_cast<double>(spec.nx - 1) * L;
        double exact = 400.0 - 100.0 * (x / L);
        for (int n = 1; n <= 40; ++n) {
            const double lambda = static_cast<double>(n) * pi / L;
            exact -= (200.0 / (static_cast<double>(n) * pi)) * std::sin(lambda * x) *
                     std::exp(-alpha * lambda * lambda * t_end);
        }
        const double num = solver.temperature(ix, 1, 1);
        const double rel_err = std::abs(num - exact) / exact;
        CHECK(rel_err < 2e-3); // Strictly < 0.2% error
    }
}

TEST_CASE("PHY-02-001-02: Churchill-Chu natural convection & Churchill-Bernstein forced convection correlations") {
    // Air at film temperature ~350 K
    FluidPropertiesAirWater air;
    air.k_w_m_k = 0.030;
    air.nu_m2_s = 2.092e-5;
    air.prandtl = 0.700;
    air.beta_per_k = 1.0 / 350.0;

    // 1) Vertical flat plate L = 0.5 m, T_s = 400 K, T_inf = 300 K (Delta T = 100 K)
    auto vert = ConvectionBoundaryModel::evaluate(
        ConvectionGeometry::VerticalFlatPlateNatural, 400.0, 300.0, 0.5, 1.0, air);
    // Ra = 9.80665 * (1/350) * 100 * 0.5^3 / (2.092e-5)^2 * 0.700 ~ 5.6e8 (Turbulent/transitional natural conv h ~ 5-9 W/m2K)
    CHECK(vert.rayleigh_or_reynolds > 4e8 && vert.rayleigh_or_reynolds < 7e8);
    CHECK(vert.h_w_m2_k > 4.5 && vert.h_w_m2_k < 9.5);
    CHECK(std::abs(vert.heat_flux_w - vert.h_w_m2_k * 1.0 * 100.0) < 1e-9);

    // 2) Forced cross-flow cylinder D = 0.05 m, U = 10 m/s
    auto cyl = ConvectionBoundaryModel::evaluate(
        ConvectionGeometry::CylinderCrossFlowForced, 400.0, 300.0, 0.05, 0.1, air, 10.0);
    // Re = 10 * 0.05 / 2.092e-5 ~ 23900 => Nu ~ 80-100, h ~ 45-65 W/m2K
    CHECK(cyl.rayleigh_or_reynolds > 20000.0 && cyl.rayleigh_or_reynolds < 28000.0);
    CHECK(cyl.nusselt > 65.0 && cyl.nusselt < 115.0);
    CHECK(cyl.h_w_m2_k > 40.0 && cyl.h_w_m2_k < 70.0);
}

TEST_CASE("PHY-02-001-03: Radiative view factor enclosure sum/reciprocity & Stefan-Boltzmann parallel plate equilibrium") {
    // 1) Closed 6-sided unit cube enclosure [0, 1]^3: inward normals -> sum_j F_ij == 1.0 and A_i F_ij == A_j F_ji
    std::vector<RadiativePatch3D> cube{
        {{0.5, 0.5, 0.0}, { 0,  0,  1}, {1, 0, 0}, {0, 1, 0}, 0.5, 0.5, 1.0, 300.0},
        {{0.5, 0.5, 1.0}, { 0,  0, -1}, {1, 0, 0}, {0, 1, 0}, 0.5, 0.5, 1.0, 300.0},
        {{0.0, 0.5, 0.5}, { 1,  0,  0}, {0, 1, 0}, {0, 0, 1}, 0.5, 0.5, 1.0, 300.0},
        {{1.0, 0.5, 0.5}, {-1,  0,  0}, {0, 1, 0}, {0, 0, 1}, 0.5, 0.5, 1.0, 300.0},
        {{0.5, 0.0, 0.5}, { 0,  1,  0}, {1, 0, 0}, {0, 0, 1}, 0.5, 0.5, 1.0, 300.0},
        {{0.5, 1.0, 0.5}, { 0, -1,  0}, {1, 0, 0}, {0, 0, 1}, 0.5, 0.5, 1.0, 300.0}
    };
    const auto F = ViewFactorRaymarcher::compute_view_factor_matrix(cube, 8, 16);
    for (size_t i = 0; i < 6; ++i) {
        double sum_f = 0.0;
        for (size_t j = 0; j < 6; ++j) {
            sum_f += F[i][j];
            CHECK(std::abs(cube[i].area() * F[i][j] - cube[j].area() * F[j][i]) < 1e-12);
        }
        // Quadrature accuracy over flat cube faces ~ 1.0 +/- 0.02
        CHECK(std::abs(sum_f - 1.0) < 0.03);
    }

    // Check analytical parallel equal squares view factor (opposite faces 0 & 1 of unit cube: exact F12 = 0.199825)
    CHECK(std::abs(F[0][1] - 0.199825) < 0.01);

    // 2) Parallel blackbody plates reaching exact Stefan-Boltzmann radiative equilibrium: T_eq = ((T1 + T2) / 2)
    auto eq = RadiationSolver::simulate_parallel_plates_equilibrium(600.0, 300.0, 1.0, 500.0, 400.0, 0.5, 1.0);
    REQUIRE(eq.is_ok());
    CHECK(std::abs(eq.value().first - 450.0) < 0.1);
    CHECK(std::abs(eq.value().second - 450.0) < 0.1);
}
