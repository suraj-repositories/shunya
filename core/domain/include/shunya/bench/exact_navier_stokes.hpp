#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace shunya::bench {

struct FluidValidationResult {
    double centerline_velocity_error_rel{0.0};
    double wall_shear_error_rel{0.0};
    double profile_l2_error_rel{0.0};
};

/**
 * @brief Exact Navier-Stokes Plane Couette, Plane Poiseuille, and Circular Hagen-Poiseuille validator.
 */
class BenchmarkNavierStokes {
public:
    [[nodiscard]] static double exact_couette_u(double y_m, double height_m, double u_top_m_s) noexcept {
        return u_top_m_s * (y_m / height_m);
    }

    [[nodiscard]] static double exact_plane_poiseuille_u(
        double y_m,
        double height_m,
        double u_max_m_s) noexcept {
        const double r = y_m / height_m;
        return 4.0 * u_max_m_s * r * (1.0 - r);
    }

    [[nodiscard]] static double exact_pipe_poiseuille_u(
        double radial_r_m,
        double radius_r_m,
        double dp_dx_mag_pa_m,
        double viscosity_pa_s) noexcept {
        const double r2 = std::min(radial_r_m * radial_r_m, radius_r_m * radius_r_m);
        return (dp_dx_mag_pa_m / (4.0 * viscosity_pa_s)) * (radius_r_m * radius_r_m - r2);
    }

    /**
     * @brief Simulates transient 1D incompressible viscous channel Poiseuille startup to steady state across Re regimes.
     */
    [[nodiscard]] static FluidValidationResult validate_channel_poiseuille(
        double height_m,
        double density_kg_m3,
        double u_centerline_target_m_s,
        double reynolds_number,
        size_t ny_nodes = 81) noexcept {
        const double nu = (u_centerline_target_m_s * height_m) / std::max(1.0, reynolds_number);
        const double mu = density_kg_m3 * nu;
        const double body_accel_x = (8.0 * nu * u_centerline_target_m_s) / (height_m * height_m);
        const double dy = height_m / static_cast<double>(ny_nodes - 1);
        const double dt = 0.25 * (dy * dy) / nu;
        const double r = nu * dt / (dy * dy);

        // Integrate diffusion time T_diff = 3.0 * (H^2 / nu) to reach < 1e-10 of steady parabola
        const double t_total = 3.0 * (height_m * height_m) / nu;
        const size_t steps = static_cast<size_t>(std::ceil(t_total / dt));

        std::vector<double> u(ny_nodes, 0.0), u_next(ny_nodes, 0.0);
        for (size_t s = 0; s < steps; ++s) {
            for (size_t j = 1; j + 1 < ny_nodes; ++j) {
                u_next[j] = u[j] + r * (u[j - 1] - 2.0 * u[j] + u[j + 1]) + body_accel_x * dt;
            }
            u.swap(u_next);
        }

        double err_sq = 0.0, ref_sq = 0.0;
        for (size_t j = 0; j < ny_nodes; ++j) {
            const double y = static_cast<double>(j) * dy;
            const double u_ex = exact_plane_poiseuille_u(y, height_m, u_centerline_target_m_s);
            const double e = u[j] - u_ex;
            err_sq += e * e;
            ref_sq += u_ex * u_ex;
        }

        const size_t mid = ny_nodes / 2;
        const double tau_exact = (4.0 * mu * u_centerline_target_m_s) / height_m;
        const double tau_sim = mu * (-3.0 * u[0] + 4.0 * u[1] - u[2]) / (2.0 * dy);

        FluidValidationResult res;
        res.centerline_velocity_error_rel = std::abs(u[mid] - u_centerline_target_m_s) / u_centerline_target_m_s;
        res.wall_shear_error_rel = std::abs(tau_sim - tau_exact) / tau_exact;
        res.profile_l2_error_rel = std::sqrt(err_sq / std::max(1e-24, ref_sq));
        return res;
    }
};

} // namespace shunya::bench
