#include "tests/test_framework.hpp"

#include "shunya/phy/sph_solver_3d.hpp"

#include <algorithm>
#include <cmath>

using namespace shunya::phy;

TEST_CASE("PHY-03-001-01: SPH Wendland C2 Tait EOS 3D Dam Break surge front propagation & zero wall leakage") {
    SphSimulationConfig cfg;
    cfg.spacing_m = 0.025;
    cfg.smoothing_h_m = 0.0325;
    cfg.rest_density_kg_m3 = 1000.0;
    cfg.speed_of_sound_m_s = 25.0;
    cfg.gravity_m_s2 = {0.0, -9.81, 0.0};
    cfg.viscosity.dynamic_viscosity_pa_s = 0.05;
    cfg.viscosity.artificial_alpha = 0.08;
    cfg.viscosity.speed_of_sound_m_s = 25.0;

    SphSolver3D solver(cfg);
    // Water column a = 0.15 m wide, H = 0.20 m tall, slab z in [0, 0.10]
    solver.add_fluid_box({0.025, 0.025, 0.0}, {0.175, 0.225, 0.10});
    const double x_init = solver.max_x_extent();

    std::vector<SphBoundaryParticle> walls;
    BoundaryParticleGenerator::add_box_floor_and_walls(
        walls, {0.0, 0.0, -0.025}, {0.50, 0.30, 0.125}, 0.025, true, true, true);
    solver.add_boundary_particles(std::move(walls));

    // Advance 0.22 s -> Martin & Moyce t* = t * sqrt(g/a) ~ 0.22 * sqrt(9.81 / 0.15) ~ 1.78
    auto res = solver.advance(0.22, 0.0005);
    REQUIRE(res.is_ok());

    const double x_final = solver.max_x_extent();
    CHECK(x_final > x_init + 0.10); // Surge front raced forward cleanly along the tank floor

    // Zero particles penetrated floor (y < -spacing) or left wall (x < -spacing)
    for (const auto& p : solver.particles()) {
        CHECK(p.position.y > -0.015);
        CHECK(p.position.x > -0.015);
    }
}

TEST_CASE("PHY-03-001-02: SPH Newtonian viscosity & no-slip walls Poiseuille parabolic velocity profile") {
    // Periodic channel x in [0, 0.12], z in [0, 0.09], walls at y = 0 and y = L = 0.15 m
    SphSimulationConfig cfg;
    cfg.spacing_m = 0.025;
    cfg.smoothing_h_m = 0.0325;
    cfg.rest_density_kg_m3 = 1000.0;
    cfg.speed_of_sound_m_s = 12.0;
    cfg.gravity_m_s2 = {0.0, 0.0, 0.0};
    const double body_fx = 0.5; // m/s^2
    cfg.external_body_accel = {body_fx, 0.0, 0.0};
    const double nu = 0.01;     // m^2/s (mu = 10 Pa*s -> Re ~ 1, fast laminar convergence)
    cfg.viscosity.dynamic_viscosity_pa_s = cfg.rest_density_kg_m3 * nu;
    cfg.viscosity.artificial_alpha = 0.0;
    cfg.periodic_x_m = 0.125;
    cfg.periodic_z_m = 0.10;

    SphSolver3D solver(cfg);
    const double L = 0.15;
    solver.add_fluid_box({0.0, 0.0125, 0.0}, {0.125, L - 0.0125, 0.10});

    // Multi-layer no-slip top & bottom channel plates (3 layers thick)
    std::vector<SphBoundaryParticle> plates;
    for (double off : {0.0, -0.025, -0.050}) {
        for (double x = 0.0; x < 0.1249; x += 0.025) {
            for (double z = 0.0; z < 0.0999; z += 0.025) {
                plates.push_back({{x, off, z}, {0, 1, 0}, {0, 0, 0}, 0.025 * 0.025 * 0.025});
                plates.push_back({{x, L - off, z}, {0, -1, 0}, {0, 0, 0}, 0.025 * 0.025 * 0.025});
            }
        }
    }
    solver.add_boundary_particles(std::move(plates));

    // Analytical centerline max velocity u_max = F_x * L^2 / (8 * nu) = 0.5 * 0.0225 / 0.08 = 0.140625 m/s
    auto res = solver.advance(0.60, 0.0004);
    REQUIRE(res.is_ok());

    double center_u_sum = 0.0;
    int center_cnt = 0;
    double edge_u_sum = 0.0;
    int edge_cnt = 0;
    for (const auto& p : solver.particles()) {
        if (std::abs(p.position.y - 0.5 * L) < 0.02) {
            center_u_sum += p.velocity.x;
            ++center_cnt;
        } else if (p.position.y < 0.035 || p.position.y > L - 0.035) {
            edge_u_sum += p.velocity.x;
            ++edge_cnt;
        }
        CHECK(p.position.y > -0.01 && p.position.y < L + 0.01);
    }
    REQUIRE(center_cnt > 0 && edge_cnt > 0);
    const double u_center = center_u_sum / center_cnt;
    const double u_edge = edge_u_sum / edge_cnt;
    const double u_analytical = (body_fx * L * L) / (8.0 * nu);
    // Centerline strongly exceeds wall boundary layer (parabolic profile) and matches order & magnitude (~0.14 m/s)
    CHECK(u_center > 1.8 * u_edge);
    CHECK(u_center > 0.65 * u_analytical && u_center < 1.35 * u_analytical);
}

TEST_CASE("PHY-03-001-03: Surface tension droplet spherification, Rayleigh oscillation & Jurin capillary law") {
    // 1) Zero-gravity elongated droplet relaxes toward isotropic spherical equilibrium
    SphSimulationConfig cfg;
    cfg.spacing_m = 0.02;
    cfg.smoothing_h_m = 0.026;
    cfg.rest_density_kg_m3 = 1000.0;
    cfg.speed_of_sound_m_s = 10.0;
    cfg.gravity_m_s2 = {0.0, 0.0, 0.0};
    cfg.viscosity.dynamic_viscosity_pa_s = 0.2;
    cfg.enable_surface_tension = true;
    cfg.surface_tension.surface_tension_n_m = 2.0;

    SphSolver3D solver(cfg);
    solver.add_fluid_box({0.0, 0.0, 0.0}, {0.16, 0.08, 0.08}); // 2:1 elongated slab
    const auto ext0 = solver.bounding_box_extent();
    const double ratio0 = ext0.x / std::max(1e-6, ext0.y);

    auto res = solver.advance(0.15, 0.0005);
    REQUIRE(res.is_ok());
    const auto ext1 = solver.bounding_box_extent();
    const double ratio1 = ext1.x / std::max(1e-6, ext1.y);
    CHECK(ratio1 < ratio0); // Aspect ratio contracts cleanly toward spherical equilibrium

    // 2) Analytical Rayleigh l=2 droplet mode & Jurin capillary rise checks
    const double omega_r = SphSurfaceTensionModule::rayleigh_oscillation_omega(0.0728, 998.2, 0.001, 2);
    // omega = sqrt(8 * 0.0728 / (998.2 * 1e-9)) = 763.8 rad/s
    CHECK(std::abs(omega_r - 763.8) < 1.0);

    const double h_water = SphSurfaceTensionModule::jurin_capillary_height_m(0.0728, 0.0, 998.2, 0.0005);
    // h = 2 * 0.0728 / (998.2 * 9.80665 * 0.0005) = 0.02975 m (~30 mm)
    CHECK(std::abs(h_water - 0.02975) < 0.0002);
}
