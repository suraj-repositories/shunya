#include "shunya/core/domain/fluid_dynamics.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::core::domain::fluid {

// =============================================================================
// FluidSolver2D Implementation
// =============================================================================

FluidSolver2D::FluidSolver2D(Grid2DConfig grid, FluidProperties fluid)
    : grid_(grid), fluid_(std::move(fluid)) {
    size_t total_cells = grid_.nx * grid_.ny;
    u_.assign(total_cells, 0.0);
    v_.assign(total_cells, 0.0);
    p_.assign(total_cells, 0.0);
}

void FluidSolver2D::set_boundary(std::string_view side, BoundaryCondition bc) {
    if (side == "top") {
        bc_top_ = bc;
    } else if (side == "bottom") {
        bc_bottom_ = bc;
    } else if (side == "left") {
        bc_left_ = bc;
    } else if (side == "right") {
        bc_right_ = bc;
    }
}

base::Result<void> FluidSolver2D::initialize() {
    size_t total_cells = grid_.nx * grid_.ny;
    u_.assign(total_cells, 0.0);
    v_.assign(total_cells, 0.0);
    p_.assign(total_cells, 0.0);
    apply_boundary_conditions();
    return base::Result<void>::ok();
}

double FluidSolver2D::u(size_t i, size_t j) const noexcept {
    if (i < grid_.nx && j < grid_.ny) {
        return u_[j * grid_.nx + i];
    }
    return 0.0;
}

double FluidSolver2D::v(size_t i, size_t j) const noexcept {
    if (i < grid_.nx && j < grid_.ny) {
        return v_[j * grid_.nx + i];
    }
    return 0.0;
}

double FluidSolver2D::p(size_t i, size_t j) const noexcept {
    if (i < grid_.nx && j < grid_.ny) {
        return p_[j * grid_.nx + i];
    }
    return 0.0;
}

void FluidSolver2D::set_u(size_t i, size_t j, double val) {
    if (i < grid_.nx && j < grid_.ny) {
        u_[j * grid_.nx + i] = val;
    }
}

void FluidSolver2D::set_v(size_t i, size_t j, double val) {
    if (i < grid_.nx && j < grid_.ny) {
        v_[j * grid_.nx + i] = val;
    }
}

void FluidSolver2D::apply_boundary_conditions() {
    size_t nx = grid_.nx;
    size_t ny = grid_.ny;

    // Bottom boundary (j = 0)
    for (size_t i = 0; i < nx; ++i) {
        u_[0 * nx + i] = bc_bottom_.u_prescribed;
        v_[0 * nx + i] = bc_bottom_.v_prescribed;
    }

    // Top boundary (j = ny - 1)
    for (size_t i = 0; i < nx; ++i) {
        u_[(ny - 1) * nx + i] = bc_top_.u_prescribed;
        v_[(ny - 1) * nx + i] = bc_top_.v_prescribed;
    }

    // Left boundary (i = 0)
    for (size_t j = 0; j < ny; ++j) {
        u_[j * nx + 0] = bc_left_.u_prescribed;
        v_[j * nx + 0] = bc_left_.v_prescribed;
    }

    // Right boundary (i = nx - 1)
    for (size_t j = 0; j < ny; ++j) {
        if (bc_right_.type == BoundaryType::PressureOutlet) {
            // Zero-gradient for velocity at outlet
            u_[j * nx + (nx - 1)] = u_[j * nx + (nx - 2)];
            v_[j * nx + (nx - 1)] = v_[j * nx + (nx - 2)];
            p_[j * nx + (nx - 1)] = bc_right_.p_prescribed;
        } else {
            u_[j * nx + (nx - 1)] = bc_right_.u_prescribed;
            v_[j * nx + (nx - 1)] = bc_right_.v_prescribed;
        }
    }
}

void FluidSolver2D::solve_pressure_poisson(double dt, size_t max_iters, double tol) {
    size_t nx = grid_.nx;
    size_t ny = grid_.ny;
    double dx = grid_.dx();
    double dy = grid_.dy();
    double dx2 = dx * dx;
    double dy2 = dy * dy;
    double rho = fluid_.density;

    diagnostics_.converged = false;
    std::vector<double> p_new = p_;
    size_t iter = 0;
    for (; iter < max_iters; ++iter) {
        double max_err = 0.0;
        double max_p = 0.0;
        for (size_t j = 1; j < ny - 1; ++j) {
            for (size_t i = 1; i < nx - 1; ++i) {
                // Divergence of intermediate velocity
                double div_u = ((u_[j * nx + (i + 1)] - u_[j * nx + (i - 1)]) / (2.0 * dx)) +
                               ((v_[(j + 1) * nx + i] - v_[(j - 1) * nx + i]) / (2.0 * dy));
                double rhs = (rho / dt) * div_u;

                double p_val = ((p_[j * nx + (i + 1)] + p_[j * nx + (i - 1)]) * dy2 +
                                (p_[(j + 1) * nx + i] + p_[(j - 1) * nx + i]) * dx2 -
                                rhs * dx2 * dy2) / (2.0 * (dx2 + dy2));

                double diff = std::abs(p_val - p_[j * nx + i]);
                max_err = std::max(max_err, diff);
                max_p = std::max(max_p, std::abs(p_val));
                p_new[j * nx + i] = p_val;
            }
        }
        p_ = p_new;
        if (max_err < tol || max_err <= tol * (1.0 + max_p)) {
            diagnostics_.converged = true;
            break;
        }
    }
    diagnostics_.pressure_poisson_iterations = iter;
}

base::Result<void> FluidSolver2D::step(double dt) {
    if (dt <= 0.0) {
        return base::Result<void>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::Simulation,
            .message = "Time step dt must be positive",
            .details = {}
        });
    }

    size_t nx = grid_.nx;
    size_t ny = grid_.ny;
    double dx = grid_.dx();
    double dy = grid_.dy();
    double nu = fluid_.kinematic_viscosity();
    double rho = fluid_.density;

    std::vector<double> u_star = u_;
    std::vector<double> v_star = v_;

    // 1. Intermediate velocity (Advection + Diffusion)
    for (size_t j = 1; j < ny - 1; ++j) {
        for (size_t i = 1; i < nx - 1; ++i) {
            double u_curr = u_[j * nx + i];
            double v_curr = v_[j * nx + i];

            // Upwind advection
            double du_dx = (u_curr >= 0.0) ? (u_curr - u_[j * nx + (i - 1)]) / dx
                                           : (u_[j * nx + (i + 1)] - u_curr) / dx;
            double du_dy = (v_curr >= 0.0) ? (u_curr - u_[(j - 1) * nx + i]) / dy
                                           : (u_[(j + 1) * nx + i] - u_curr) / dy;

            double dv_dx = (u_curr >= 0.0) ? (v_curr - v_[j * nx + (i - 1)]) / dx
                                           : (v_[j * nx + (i + 1)] - v_curr) / dx;
            double dv_dy = (v_curr >= 0.0) ? (v_curr - v_[(j - 1) * nx + i]) / dy
                                           : (v_[(j + 1) * nx + i] - v_curr) / dy;

            // Diffusion (Laplacian)
            double lap_u = (u_[j * nx + (i + 1)] - 2.0 * u_curr + u_[j * nx + (i - 1)]) / (dx * dx) +
                           (u_[(j + 1) * nx + i] - 2.0 * u_curr + u_[(j - 1) * nx + i]) / (dy * dy);
            double lap_v = (v_[j * nx + (i + 1)] - 2.0 * v_curr + v_[j * nx + (i - 1)]) / (dx * dx) +
                           (v_[(j + 1) * nx + i] - 2.0 * v_curr + v_[(j - 1) * nx + i]) / (dy * dy);

            u_star[j * nx + i] = u_curr + dt * (-(u_curr * du_dx + v_curr * du_dy) + nu * lap_u);
            v_star[j * nx + i] = v_curr + dt * (-(u_curr * dv_dx + v_curr * dv_dy) + nu * lap_v);
        }
    }

    u_ = u_star;
    v_ = v_star;
    apply_boundary_conditions();

    // 2. Solve Pressure Poisson Equation
    solve_pressure_poisson(dt);

    // 3. Project velocity field to satisfy continuity (divergence-free)
    double max_div = 0.0;
    double max_vel = 0.0;
    for (size_t j = 1; j < ny - 1; ++j) {
        for (size_t i = 1; i < nx - 1; ++i) {
            double dp_dx = (p_[j * nx + (i + 1)] - p_[j * nx + (i - 1)]) / (2.0 * dx);
            double dp_dy = (p_[(j + 1) * nx + i] - p_[(j - 1) * nx + i]) / (2.0 * dy);

            u_[j * nx + i] -= (dt / rho) * dp_dx;
            v_[j * nx + i] -= (dt / rho) * dp_dy;

            double speed = std::sqrt(u_[j * nx + i] * u_[j * nx + i] + v_[j * nx + i] * v_[j * nx + i]);
            max_vel = std::max(max_vel, speed);

            double div = std::abs(((u_[j * nx + (i + 1)] - u_[j * nx + (i - 1)]) / (2.0 * dx)) +
                                  ((v_[(j + 1) * nx + i] - v_[(j - 1) * nx + i]) / (2.0 * dy)));
            max_div = std::max(max_div, div);
        }
    }

    apply_boundary_conditions();
    diagnostics_.max_divergence = max_div;
    diagnostics_.max_velocity = max_vel;

    return base::Result<void>::ok();
}

// =============================================================================
// HydraulicPipe Implementation
// =============================================================================

double HydraulicPipe::laminar_hydraulic_resistance() const noexcept {
    double d = params_.diameter;
    double d4 = d * d * d * d;
    double pi = 3.14159265358979323846;
    return (128.0 * fluid_.dynamic_viscosity * params_.length) / (pi * d4);
}

double HydraulicPipe::calculate_flow_rate(double delta_p) const noexcept {
    double r_h = laminar_hydraulic_resistance();
    return r_h > 0.0 ? (delta_p / r_h) : 0.0;
}

double HydraulicPipe::calculate_pressure_drop(double volumetric_flow) const noexcept {
    return volumetric_flow * laminar_hydraulic_resistance();
}

// =============================================================================
// ParticleTracer Implementation
// =============================================================================

void ParticleTracer::step(double dt) {
    if (!solver_ || dt <= 0.0) {
        return;
    }

    const auto& grid = solver_->grid();
    double dx = grid.dx();
    double dy = grid.dy();

    for (auto& p : particles_) {
        // Bilinear lookup cell
        double fi = p.x / dx;
        double fj = p.y / dy;

        size_t i = static_cast<size_t>(std::clamp(std::floor(fi), 0.0, static_cast<double>(grid.nx - 1)));
        size_t j = static_cast<size_t>(std::clamp(std::floor(fj), 0.0, static_cast<double>(grid.ny - 1)));

        double vx = solver_->u(i, j);
        double vy = solver_->v(i, j);

        p.x += vx * dt;
        p.y += vy * dt;
        p.age += dt;

        // Keep inside bounds
        p.x = std::clamp(p.x, 0.0, grid.width);
        p.y = std::clamp(p.y, 0.0, grid.height);
    }
}

// =============================================================================
// FluidValidationBenchmark Implementation
// =============================================================================

double FluidValidationBenchmark::analytical_poiseuille_umax(
    double delta_p, double channel_height, double channel_length, double mu) {
    if (channel_length <= 0.0 || mu <= 0.0) {
        return 0.0;
    }
    double h = channel_height;
    return (delta_p * h * h) / (8.0 * mu * channel_length);
}

double FluidValidationBenchmark::analytical_poiseuille_profile(
    double y, double u_max, double channel_height) {
    double h = channel_height;
    if (h <= 0.0) return 0.0;
    double norm_y = y / (0.5 * h);
    return u_max * (1.0 - norm_y * norm_y);
}

} // namespace shunya::core::domain::fluid
