#include "tests/test_framework.hpp"

#include "shunya/phy/mac_grid_solver_3d.hpp"

#include <cmath>
#include <numbers>

using namespace shunya::phy;

TEST_CASE("PHY-03-002-01: Staggered MAC 3D vortex BFECC advection & incompressibility (< 1e-6 divergence)") {
    // Solid-body rotation vortex in 3D slice (24 x 24 x 8): advect Gaussian dye blob 1 full revolution
    MacGridConfig cfg;
    cfg.nx = 24;
    cfg.ny = 24;
    cfg.nz = 8;
    cfg.dx_m = 1.0 / 24.0;
    cfg.density_kg_m3 = 1.0;
    cfg.kinematic_viscosity_m2_s = 0.0;

    MacGridFluidSolver3D solver(cfg);

    // Seed an off-center dye plume and divergence-free swirling velocity field around (0.5, 0.5)
    for (size_t k = 0; k < cfg.nz; ++k) {
        const double z = (k + 0.5) * cfg.dx_m;
        for (size_t j = 0; j < cfg.ny; ++j) {
            const double y = (j + 0.5) * cfg.dx_m;
            for (size_t i = 0; i < cfg.nx; ++i) {
                const double x = (i + 0.5) * cfg.dx_m;
                const double r2 = (x - 0.65) * (x - 0.65) + (y - 0.50) * (y - 0.50) + (z - 0.50) * (z - 0.50);
                solver.set_dye(i, j, k, std::exp(-r2 / 0.02));
            }
        }
    }
    const double dye_init = solver.total_dye();

    for (size_t k = 0; k < cfg.nz; ++k) {
        for (size_t j = 0; j < cfg.ny; ++j) {
            const double y = (j + 0.5) * cfg.dx_m - 0.5;
            for (size_t i = 1; i < cfg.nx; ++i) {
                solver.set_u(i, j, k, -y);
            }
        }
        for (size_t j = 1; j < cfg.ny; ++j) {
            const double y = j * cfg.dx_m - 0.5;
            (void)y;
            for (size_t i = 0; i < cfg.nx; ++i) {
                const double x = (i + 0.5) * cfg.dx_m - 0.5;
                solver.set_v(i, j, k, x);
            }
        }
    }

    for (int s = 0; s < 30; ++s) {
        auto stats = solver.step(0.02);
        REQUIRE(stats.is_ok());
        CHECK(solver.max_divergence() < 1e-6);
    }
    const double dye_after = solver.total_dye();
    CHECK(std::abs(dye_after - dye_init) / dye_init < 0.05);
}

TEST_CASE("PHY-03-002-02: Ghia et al. Lid-Driven Cavity benchmark at Re=100 and Re=400 (< 1e-6 div(u))") {
    auto run_cavity = [](double re, size_t n, double total_t, double dt) {
        MacGridConfig cfg;
        cfg.nx = n;
        cfg.ny = n;
        cfg.nz = 1;
        cfg.dx_m = 1.0 / static_cast<double>(n);
        cfg.density_kg_m3 = 1.0;
        cfg.lid_top_u_m_s = 1.0;
        cfg.kinematic_viscosity_m2_s = 1.0 / re;

        MacGridFluidSolver3D solver(cfg);
        const int steps = static_cast<int>(std::round(total_t / dt));
        double worst_div = 0.0;
        for (int s = 0; s < steps; ++s) {
            auto stats = solver.step(dt);
            CHECK(stats.is_ok());
            worst_div = std::max(worst_div, solver.max_divergence());
        }
        CHECK(worst_div < 1e-6);
        return solver.find_primary_vortex_center_2d();
    };

    // 1) Re = 100 (Ghia et al. 1982: primary vortex center ~ (0.617, 0.734))
    const auto v100 = run_cavity(100.0, 32, 6.0, 0.01);
    CHECK(std::abs(v100.x - 0.617) < 0.045);
    CHECK(std::abs(v100.y - 0.734) < 0.045);

    // 2) Re = 400 (Ghia et al. 1982: primary vortex migrates toward domain center ~ (0.555, 0.606))
    const auto v400 = run_cavity(400.0, 40, 12.0, 0.01);
    CHECK(std::abs(v400.x - 0.555) < 0.055);
    CHECK(std::abs(v400.y - 0.606) < 0.055);
    CHECK(v400.y < v100.y); // Higher Re pulls vortex center deeper downward toward (0.5, 0.5)
}
