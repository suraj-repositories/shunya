#include "shunya/env/wind_tunnel_fixture.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

namespace {
constexpr double kPi = 3.14159265358979323846;

phy::MacGridConfig to_mac_config(const WindTunnelConfig& cfg) {
    phy::MacGridConfig m;
    m.nx = cfg.nx;
    m.ny = cfg.ny;
    m.nz = 1;
    m.dx_m = cfg.dx_m;
    m.density_kg_m3 = cfg.fluid_density_kg_m3;
    m.kinematic_viscosity_m2_s = cfg.kinematic_viscosity_m2_s;
    m.lid_top_u_m_s = 0.0;
    m.inlet_left_u_m_s = cfg.inlet_velocity_m_s;
    m.open_right_outlet = true;
    return m;
}
} // namespace

WindTunnelFixture::WindTunnelFixture(WindTunnelConfig cfg)
    : cfg_(cfg),
      solver_(to_mac_config(cfg)),
      solid_mask_(cfg.nx * cfg.ny, false) {
    reset();
}

void WindTunnelFixture::reset() {
    std::fill(solid_mask_.begin(), solid_mask_.end(), false);
    solver_ = phy::MacGridFluidSolver3D(to_mac_config(cfg_));
    for (size_t j = 1; j + 1 < cfg_.ny; ++j) {
        for (size_t i = 0; i <= cfg_.nx; ++i) {
            solver_.set_u(i, j, 0, cfg_.inlet_velocity_m_s);
        }
    }
    apply_tunnel_bcs();
}

void WindTunnelFixture::apply_tunnel_bcs() {
    for (size_t j = 0; j < cfg_.ny; ++j) {
        for (size_t i = 0; i < cfg_.nx; ++i) {
            if (j == 0 || j == cfg_.ny - 1 || solid_mask_[j * cfg_.nx + i]) {
                solver_.set_cell_type(i, j, 0, phy::CellFluidKind::Solid);
            } else if (i == cfg_.nx - 1) {
                solver_.set_cell_type(i, j, 0, phy::CellFluidKind::OpenAir);
            } else {
                solver_.set_cell_type(i, j, 0, phy::CellFluidKind::Fluid);
            }
        }
    }

    // Laminar inflow boundary face at x = 0
    for (size_t j = 1; j + 1 < cfg_.ny; ++j) {
        solver_.set_u(0, j, 0, cfg_.inlet_velocity_m_s);
        solver_.set_v(0, j, 0, 0.0);
    }
}

void WindTunnelFixture::place_naca0012_airfoil(
    double center_x_m,
    double center_y_m,
    double chord_m,
    double angle_of_attack_deg) {
    reference_area_m2_ = std::max(cfg_.dx_m, chord_m);
    const double alpha = angle_of_attack_deg * (kPi / 180.0);
    const double c_a = std::cos(alpha);
    const double s_a = std::sin(alpha);
    constexpr double t_rel = 0.12;

    for (size_t j = 1; j + 1 < cfg_.ny; ++j) {
        for (size_t i = 2; i + 2 < cfg_.nx; ++i) {
            const double px = (static_cast<double>(i) + 0.5) * cfg_.dx_m - center_x_m;
            const double py = (static_cast<double>(j) + 0.5) * cfg_.dx_m - center_y_m;

            // Body frame rotated by +AoA nose-up
            const double bx = px * c_a - py * s_a;
            const double by = px * s_a + py * c_a;
            const double xc = (bx / chord_m) + 0.5;
            if (xc >= 0.0 && xc <= 1.0) {
                const double yt = 5.0 * t_rel * chord_m * (
                    0.2969 * std::sqrt(xc) -
                    0.1260 * xc -
                    0.3516 * xc * xc +
                    0.2843 * xc * xc * xc -
                    0.1015 * xc * xc * xc * xc
                );
                const double half_thick = std::max(yt, 0.55 * cfg_.dx_m);
                if (std::abs(by) <= half_thick) {
                    solid_mask_[j * cfg_.nx + i] = true;
                }
            }
        }
    }
    apply_tunnel_bcs();
}

void WindTunnelFixture::place_bluff_box(
    double min_x_m, double min_y_m,
    double max_x_m, double max_y_m) {
    reference_area_m2_ = std::max(cfg_.dx_m, max_y_m - min_y_m);
    for (size_t j = 1; j + 1 < cfg_.ny; ++j) {
        for (size_t i = 2; i + 2 < cfg_.nx; ++i) {
            const double px = (static_cast<double>(i) + 0.5) * cfg_.dx_m;
            const double py = (static_cast<double>(j) + 0.5) * cfg_.dx_m;
            if (px >= min_x_m && px <= max_x_m && py >= min_y_m && py <= max_y_m) {
                solid_mask_[j * cfg_.nx + i] = true;
            }
        }
    }
    apply_tunnel_bcs();
}

WindTunnelForceReport WindTunnelFixture::integrate_solid_forces() const noexcept {
    double fx = 0.0;
    double fy = 0.0;
    for (size_t j = 1; j + 1 < cfg_.ny; ++j) {
        for (size_t i = 1; i + 1 < cfg_.nx; ++i) {
            if (!solid_mask_[j * cfg_.nx + i]) {
                continue;
            }
            if (!solid_mask_[j * cfg_.nx + (i - 1)]) {
                fx += solver_.pressure(i - 1, j, 0) * cfg_.dx_m;
            }
            if (!solid_mask_[j * cfg_.nx + (i + 1)]) {
                fx -= solver_.pressure(i + 1, j, 0) * cfg_.dx_m;
            }
            if (!solid_mask_[(j - 1) * cfg_.nx + i]) {
                fy += solver_.pressure(i, j - 1, 0) * cfg_.dx_m;
            }
            if (!solid_mask_[(j + 1) * cfg_.nx + i]) {
                fy -= solver_.pressure(i, j + 1, 0) * cfg_.dx_m;
            }
        }
    }

    const double q_dyn = 0.5 * cfg_.fluid_density_kg_m3 *
                         cfg_.inlet_velocity_m_s * cfg_.inlet_velocity_m_s *
                         reference_area_m2_;
    WindTunnelForceReport r;
    r.drag_force_n = fx;
    r.lift_force_n = fy;
    if (q_dyn > 1e-12) {
        r.drag_coeff_cd = fx / q_dyn;
        r.lift_coeff_cl = fy / q_dyn;
    }
    r.max_divergence = solver_.max_divergence();
    return r;
}

core::base::Result<WindTunnelForceReport> WindTunnelFixture::advance_and_measure(
    int steps,
    double dt_s) {
    if (steps <= 0 || dt_s <= 0.0) {
        return core::base::Error{
            341, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Wind tunnel step count and dt_s must be positive", ""
        };
    }

    for (int s = 0; s < steps; ++s) {
        apply_tunnel_bcs();
        auto res = solver_.step(dt_s);
        if (!res.is_ok()) {
            return res.error();
        }
    }
    return integrate_solid_forces();
}

double WindTunnelFixture::naca0012_reference_cl(double angle_of_attack_deg) noexcept {
    // Thin-airfoil NASA/Abbott-von Doenhoff smooth post-stall correlation (linear ~ 2*pi*sin(alpha) up to ~15 deg)
    const double alpha_clamped = std::clamp(angle_of_attack_deg, -30.0, 30.0);
    const double rad = alpha_clamped * (kPi / 180.0);
    const double abs_deg = std::abs(alpha_clamped);
    if (abs_deg <= 14.0) {
        return 2.0 * kPi * std::sin(rad) * 0.96; // finite Re ~ 0.105/deg
    }
    const double sign = (alpha_clamped >= 0.0) ? 1.0 : -1.0;
    const double peak = 2.0 * kPi * std::sin(14.0 * kPi / 180.0) * 0.96;
    const double drop = std::max(0.5, 1.0 - 0.035 * (abs_deg - 14.0));
    return sign * peak * drop;
}

double WindTunnelFixture::naca0012_reference_cd(double angle_of_attack_deg) noexcept {
    const double rad = angle_of_attack_deg * (kPi / 180.0);
    return 0.0085 + 0.065 * rad * rad + 0.45 * std::pow(std::sin(rad), 4);
}

} // namespace shunya::env
