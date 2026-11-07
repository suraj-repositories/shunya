#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain::fluid {

// =============================================================================
// FLD-01: Fluid Properties & Regime Selection
// =============================================================================

enum class FlowRegime {
    IncompressibleLaminar,
    IncompressibleTurbulent,
    CompressibleSubsonic,
    HydraulicNetwork1D
};

struct FluidProperties {
    std::string name{"Water"};
    double density{1000.0};          // kg / m^3
    double dynamic_viscosity{1.0e-3}; // Pa * s (N * s / m^2)
    double bulk_modulus{2.2e9};      // Pa
    bool compressible{false};

    [[nodiscard]] double kinematic_viscosity() const noexcept {
        return density > 0.0 ? (dynamic_viscosity / density) : 0.0;
    }

    [[nodiscard]] double calculate_reynolds(double velocity, double characteristic_length) const noexcept {
        double nu = kinematic_viscosity();
        return nu > 0.0 ? (std::abs(velocity) * characteristic_length / nu) : 0.0;
    }
};

// =============================================================================
// FLD-02: Structured Grid & Geometry Domain
// =============================================================================

struct Grid2DConfig {
    size_t nx{10};      // Number of cells in X
    size_t ny{10};      // Number of cells in Y
    double width{1.0};  // meters
    double height{1.0}; // meters

    [[nodiscard]] double dx() const noexcept { return width / static_cast<double>(nx); }
    [[nodiscard]] double dy() const noexcept { return height / static_cast<double>(ny); }
};

// =============================================================================
// FLD-03: Boundary Conditions
// =============================================================================

enum class BoundaryType {
    NoSlipWall,
    MovingWall,
    VelocityInlet,
    PressureOutlet,
    Symmetry
};

struct BoundaryCondition {
    BoundaryType type{BoundaryType::NoSlipWall};
    double u_prescribed{0.0}; // Prescribed X-velocity (e.g. for inlet or moving lid)
    double v_prescribed{0.0}; // Prescribed Y-velocity
    double p_prescribed{0.0}; // Prescribed Pressure (e.g. for outlet)
};

// =============================================================================
// FLD-04, FLD-05, FLD-06: 2D Navier-Stokes Solver (Projection Method)
// =============================================================================

struct SolverDiagnostics {
    double max_divergence{0.0};
    double max_velocity{0.0};
    size_t pressure_poisson_iterations{0};
    bool converged{false};
};

class FluidSolver2D {
public:
    explicit FluidSolver2D(Grid2DConfig grid, FluidProperties fluid);

    void set_boundary(std::string_view side, BoundaryCondition bc); // "top", "bottom", "left", "right"
    [[nodiscard]] base::Result<void> initialize();

    // Advances the flow field by time step dt using Chorin's Projection Method
    [[nodiscard]] base::Result<void> step(double dt);

    [[nodiscard]] double u(size_t i, size_t j) const noexcept;
    [[nodiscard]] double v(size_t i, size_t j) const noexcept;
    [[nodiscard]] double p(size_t i, size_t j) const noexcept;

    void set_u(size_t i, size_t j, double val);
    void set_v(size_t i, size_t j, double val);

    [[nodiscard]] const SolverDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] const Grid2DConfig& grid() const noexcept { return grid_; }
    [[nodiscard]] const FluidProperties& fluid() const noexcept { return fluid_; }

private:
    Grid2DConfig grid_;
    FluidProperties fluid_;

    std::vector<double> u_; // X-velocity field [ (nx+1) * ny ] staggered or collocated
    std::vector<double> v_; // Y-velocity field [ nx * (ny+1) ]
    std::vector<double> p_; // Pressure field [ nx * ny ]

    BoundaryCondition bc_top_{BoundaryType::NoSlipWall, 0.0, 0.0, 0.0};
    BoundaryCondition bc_bottom_{BoundaryType::NoSlipWall, 0.0, 0.0, 0.0};
    BoundaryCondition bc_left_{BoundaryType::VelocityInlet, 1.0, 0.0, 0.0};
    BoundaryCondition bc_right_{BoundaryType::PressureOutlet, 0.0, 0.0, 0.0};

    SolverDiagnostics diagnostics_{};

    void apply_boundary_conditions();
    void solve_pressure_poisson(double dt, size_t max_iters = 100, double tol = 1e-5);
};

// =============================================================================
// FLD-04 (1D): Hydraulic Pipe Network Element
// =============================================================================

struct PipeParameters {
    double length{10.0};    // m
    double diameter{0.05};  // m
    double roughness{1e-5}; // m
};

class HydraulicPipe {
public:
    HydraulicPipe(std::string id, PipeParameters params, FluidProperties fluid)
        : id_(std::move(id)), params_(params), fluid_(std::move(fluid)) {}

    [[nodiscard]] double area() const noexcept {
        double r = 0.5 * params_.diameter;
        return 3.14159265358979323846 * r * r;
    }

    // Hagen-Poiseuille laminar resistance: R_h = 128 * mu * L / (pi * D^4)
    [[nodiscard]] double laminar_hydraulic_resistance() const noexcept;

    // Calculates volumetric flow rate Q (m^3/s) given pressure drop delta_p (Pa)
    [[nodiscard]] double calculate_flow_rate(double delta_p) const noexcept;

    // Darcy-Weisbach head loss / pressure drop given flow rate Q
    [[nodiscard]] double calculate_pressure_drop(double volumetric_flow) const noexcept;

private:
    std::string id_;
    PipeParameters params_;
    FluidProperties fluid_;
};

// =============================================================================
// FLD-07: Multiphase Volume of Fluid (VOF)
// =============================================================================

class MultiphaseVOF {
public:
    MultiphaseVOF(Grid2DConfig grid, FluidProperties primary, FluidProperties secondary)
        : grid_(grid), primary_(std::move(primary)), secondary_(std::move(secondary)) {
        alpha_.assign(grid.nx * grid.ny, 1.0); // 1.0 = 100% primary phase
    }

    void set_volume_fraction(size_t i, size_t j, double alpha) {
        if (i < grid_.nx && j < grid_.ny) {
            alpha_[j * grid_.nx + i] = std::clamp(alpha, 0.0, 1.0);
        }
    }

    [[nodiscard]] double volume_fraction(size_t i, size_t j) const {
        if (i < grid_.nx && j < grid_.ny) {
            return alpha_[j * grid_.nx + i];
        }
        return 1.0;
    }

    // Effective mixed density: rho = alpha * rho1 + (1 - alpha) * rho2
    [[nodiscard]] double effective_density(size_t i, size_t j) const {
        double a = volume_fraction(i, j);
        return a * primary_.density + (1.0 - a) * secondary_.density;
    }

    // Effective mixed viscosity: mu = alpha * mu1 + (1 - alpha) * mu2
    [[nodiscard]] double effective_viscosity(size_t i, size_t j) const {
        double a = volume_fraction(i, j);
        return a * primary_.dynamic_viscosity + (1.0 - a) * secondary_.dynamic_viscosity;
    }

private:
    Grid2DConfig grid_;
    FluidProperties primary_;
    FluidProperties secondary_;
    std::vector<double> alpha_;
};

// =============================================================================
// FLD-08: Lagrangian Particle & Tracer Advection
// =============================================================================

struct FluidParticle {
    double x{0.0};
    double y{0.0};
    double age{0.0};
};

class ParticleTracer {
public:
    explicit ParticleTracer(std::shared_ptr<const FluidSolver2D> solver)
        : solver_(std::move(solver)) {}

    void spawn_particle(double x, double y) {
        particles_.push_back(FluidParticle{x, y, 0.0});
    }

    void step(double dt);

    [[nodiscard]] const std::vector<FluidParticle>& particles() const noexcept { return particles_; }
    void clear() noexcept { particles_.clear(); }

private:
    std::shared_ptr<const FluidSolver2D> solver_;
    std::vector<FluidParticle> particles_;
};

// =============================================================================
// FLD-09 & FLD-10: Benchmarks & Validation
// =============================================================================

class FluidValidationBenchmark {
public:
    // Analytical Poiseuille flow centerline velocity: u_max = delta_p * h^2 / (8 * mu * L)
    static double analytical_poiseuille_umax(double delta_p, double channel_height, double channel_length, double mu);

    // Analytical velocity at y in [-h/2, +h/2]
    static double analytical_poiseuille_profile(double y, double u_max, double channel_height);
};

} // namespace shunya::core::domain::fluid
