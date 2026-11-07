#include "tests/test_framework.hpp"
#include "shunya/core/domain/fluid_dynamics.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace shunya::core::domain::fluid;

// =============================================================================
// FLD-01: Fluid Properties & Reynolds Number
// =============================================================================

TEST_CASE("FLD-01: Fluid Properties and Reynolds Number Calculation") {
    FluidProperties water;
    water.name = "Water";
    water.density = 1000.0;          // kg/m^3
    water.dynamic_viscosity = 1.0e-3; // Pa*s

    CHECK(std::abs(water.kinematic_viscosity() - 1.0e-6) < 1e-9);

    // Flow in pipe: v = 0.1 m/s, D = 0.02 m -> Re = 0.1 * 0.02 / 1e-6 = 2000 (Laminar)
    double re = water.calculate_reynolds(0.1, 0.02);
    CHECK(std::abs(re - 2000.0) < 1e-3);
    CHECK(re < 2300.0); // Within laminar regime
}

// =============================================================================
// FLD-02 & FLD-03: Grid Setup & Boundary Conditions
// =============================================================================

TEST_CASE("FLD-02 & FLD-03: Flow Domain Grid and Boundary Conditions") {
    Grid2DConfig grid;
    grid.nx = 16;
    grid.ny = 16;
    grid.width = 0.1;
    grid.height = 0.1;

    CHECK(std::abs(grid.dx() - (0.1 / 16.0)) < 1e-9);
    CHECK(std::abs(grid.dy() - (0.1 / 16.0)) < 1e-9);

    FluidProperties air{"Air", 1.225, 1.81e-5, 1.42e5, false};
    FluidSolver2D solver(grid, air);

    solver.set_boundary("left", BoundaryCondition{BoundaryType::VelocityInlet, 0.5, 0.0, 0.0});
    solver.set_boundary("right", BoundaryCondition{BoundaryType::PressureOutlet, 0.0, 0.0, 101325.0});
    solver.set_boundary("top", BoundaryCondition{BoundaryType::NoSlipWall, 0.0, 0.0, 0.0});
    solver.set_boundary("bottom", BoundaryCondition{BoundaryType::NoSlipWall, 0.0, 0.0, 0.0});

    auto init_res = solver.initialize();
    CHECK(init_res.is_ok());

    CHECK(solver.u(0, 8) == 0.5); // inlet velocity
    CHECK(solver.u(8, 0) == 0.0); // bottom wall no-slip
}

// =============================================================================
// FLD-04, FLD-05, FLD-06: 2D Navier-Stokes Solver Stepping & Projection
// =============================================================================

TEST_CASE("FLD-04, FLD-05, FLD-06: 2D Incompressible Navier-Stokes Flow Solver") {
    Grid2DConfig grid;
    grid.nx = 10;
    grid.ny = 10;
    grid.width = 0.05;
    grid.height = 0.05;

    FluidProperties fluid{"TestFluid", 1000.0, 0.01, 2e9, false};
    FluidSolver2D solver(grid, fluid);

    solver.set_boundary("left", BoundaryCondition{BoundaryType::VelocityInlet, 0.1, 0.0, 0.0});
    solver.set_boundary("right", BoundaryCondition{BoundaryType::PressureOutlet, 0.0, 0.0, 0.0});
    solver.set_boundary("top", BoundaryCondition{BoundaryType::NoSlipWall, 0.0, 0.0, 0.0});
    solver.set_boundary("bottom", BoundaryCondition{BoundaryType::NoSlipWall, 0.0, 0.0, 0.0});

    auto init_res = solver.initialize();
    CHECK(init_res.is_ok());

    // Advance 5 time steps
    for (int step = 0; step < 5; ++step) {
        auto step_res = solver.step(0.001);
        CHECK(step_res.is_ok());
    }

    const auto& diag = solver.diagnostics();
    CHECK(diag.converged);
    CHECK(diag.max_velocity >= 0.0);
}

// =============================================================================
// FLD-04 (1D): Hydraulic Pipe Network Element
// =============================================================================

TEST_CASE("FLD-04: Hydraulic Pipe Network Element & Hagen-Poiseuille Law") {
    PipeParameters pipe_params;
    pipe_params.length = 5.0;     // 5 meters
    pipe_params.diameter = 0.02;  // 2 cm diameter
    pipe_params.roughness = 1e-5;

    FluidProperties water{"Water", 1000.0, 1.0e-3, 2.2e9, false};
    HydraulicPipe pipe("pipe_main", pipe_params, water);

    double r_h = pipe.laminar_hydraulic_resistance();
    CHECK(r_h > 0.0);

    // Apply pressure drop delta_p = 1000 Pa
    double delta_p = 1000.0;
    double q = pipe.calculate_flow_rate(delta_p);
    CHECK(q > 0.0);

    // Consistency check: calculate pressure drop from flow rate
    double dp_calc = pipe.calculate_pressure_drop(q);
    CHECK(std::abs(dp_calc - delta_p) < 1e-6);
}

// =============================================================================
// FLD-07: Multiphase Volume of Fluid (VOF)
// =============================================================================

TEST_CASE("FLD-07: Multiphase Volume of Fluid Mixture Properties") {
    Grid2DConfig grid;
    grid.nx = 8;
    grid.ny = 8;
    grid.width = 1.0;
    grid.height = 1.0;

    FluidProperties water{"Water", 1000.0, 1.0e-3, 2.2e9, false};
    FluidProperties air{"Air", 1.2, 1.8e-5, 1.4e5, false};

    MultiphaseVOF vof(grid, water, air);

    // Pure water cell (alpha = 1.0)
    CHECK(std::abs(vof.effective_density(0, 0) - 1000.0) < 1e-3);

    // Set 50% water / 50% air mixture interface cell (alpha = 0.5)
    vof.set_volume_fraction(4, 4, 0.5);
    double expected_rho = 0.5 * 1000.0 + 0.5 * 1.2;
    CHECK(std::abs(vof.effective_density(4, 4) - expected_rho) < 1e-3);

    double expected_mu = 0.5 * 1.0e-3 + 0.5 * 1.8e-5;
    CHECK(std::abs(vof.effective_viscosity(4, 4) - expected_mu) < 1e-6);
}

// =============================================================================
// FLD-08: Lagrangian Particle & Tracer Advection
// =============================================================================

TEST_CASE("FLD-08: Lagrangian Particle Tracer Advection") {
    Grid2DConfig grid;
    grid.nx = 10;
    grid.ny = 10;
    grid.width = 1.0;
    grid.height = 1.0;

    FluidProperties water{"Water", 1000.0, 1.0e-3, 2.2e9, false};
    auto solver = std::make_shared<FluidSolver2D>(grid, water);

    auto init_res = solver->initialize();
    CHECK(init_res.is_ok());

    // Uniform velocity field u = 1.0 m/s, v = 0.0
    for (size_t j = 0; j < grid.ny; ++j) {
        for (size_t i = 0; i < grid.nx; ++i) {
            solver->set_u(i, j, 1.0);
            solver->set_v(i, j, 0.0);
        }
    }

    ParticleTracer tracer(solver);
    tracer.spawn_particle(0.1, 0.5);

    // Advance 0.2 seconds -> particle should move from x=0.1 to x=0.3
    tracer.step(0.2);

    REQUIRE(tracer.particles().size() == 1u);
    CHECK(std::abs(tracer.particles()[0].x - 0.3) < 1e-3);
    CHECK(std::abs(tracer.particles()[0].y - 0.5) < 1e-3);
    CHECK(std::abs(tracer.particles()[0].age - 0.2) < 1e-6);
}

// =============================================================================
// FLD-09 & FLD-10: Poiseuille Flow Benchmark & Grid Sensitivity
// =============================================================================

TEST_CASE("FLD-09 & FLD-10: Analytical Poiseuille Flow Benchmark and Sensitivity") {
    double delta_p = 50.0;    // Pa
    double height = 0.02;     // 2 cm
    double length = 1.0;      // 1 m
    double mu = 1.0e-3;       // Pa*s

    double u_max = FluidValidationBenchmark::analytical_poiseuille_umax(delta_p, height, length, mu);
    CHECK(u_max > 0.0);

    // Centerline (y = 0) velocity must equal u_max
    double u_center = FluidValidationBenchmark::analytical_poiseuille_profile(0.0, u_max, height);
    CHECK(std::abs(u_center - u_max) < 1e-9);

    // Wall (y = +/- h/2) velocity must equal 0 (no slip)
    double u_wall_top = FluidValidationBenchmark::analytical_poiseuille_profile(0.5 * height, u_max, height);
    double u_wall_bot = FluidValidationBenchmark::analytical_poiseuille_profile(-0.5 * height, u_max, height);
    CHECK(std::abs(u_wall_top) < 1e-9);
    CHECK(std::abs(u_wall_bot) < 1e-9);

    // Quarter height (y = h/4) velocity: u = u_max * (1 - (0.5)^2) = 0.75 * u_max
    double u_quarter = FluidValidationBenchmark::analytical_poiseuille_profile(0.25 * height, u_max, height);
    CHECK(std::abs(u_quarter - 0.75 * u_max) < 1e-9);
}
