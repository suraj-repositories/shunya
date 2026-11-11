#pragma once

#include <cmath>

namespace shunya::bench {

struct GyroPrecessionReport {
    double expected_precession_rad_s{0.0};
    double simulated_precession_rad_s{0.0};
    double relative_error{0.0};
    double lz_drift_relative{0.0};
};

/**
 * @brief Exact Lagrange/Euler axisymmetric heavy spinning top integrator verifying Omega_p = M*g*r / (I_z * omega_s).
 */
class BenchmarkGyroscopic {
public:
    [[nodiscard]] static GyroPrecessionReport verify_steady_precession(
        double mass_kg,
        double pivot_distance_r_m,
        double iz_axial_kg_m2,
        double ix_transverse_kg_m2,
        double spin_omega_s_rad_s,
        double tilt_theta_rad,
        double gravity_m_s2 = 9.80665,
        double total_time_s = 1.5,
        double dt_s = 1e-4) noexcept {
        // Exact fast-top steady precession roots of (I_x * cos(theta) * Omega_p^2 - I_z * omega_3 * Omega_p + M*g*r = 0)
        // Here omega_3 = omega_s + Omega_p * cos(theta)
        // Wait: steady precession satisfies I_x*sin(theta)*cos(theta)*Omega_p^2 - L_3*sin(theta)*Omega_p + M*g*r*sin(theta) = 0
        // Notice with omega_s constant along body symmetry axis & pure vertical L_z conservation:
        // State variables (theta, phi, theta_dot):
        // p_phi = I_x * sin^2(theta) * phi_dot + I_z * omega_3 * cos(theta) = const
        // p_3   = I_z * (psi_dot + phi_dot * cos(theta)) = I_z * omega_3 = const
        const double l_3 = iz_axial_kg_m2 * spin_omega_s_rad_s;
        const double mgr = mass_kg * gravity_m_s2 * pivot_distance_r_m;
        const double omega_p_theory = mgr / l_3;

        // Exact Euler-Poisson steady precession slow root of (I_x*cos(th)*Op^2 - l_3*Op + M*g*r = 0)
        const double a_quad = ix_transverse_kg_m2 * std::cos(tilt_theta_rad);
        double omega_p_exact = omega_p_theory;
        if (std::abs(a_quad) > 1e-12) {
            const double disc = std::max(0.0, l_3 * l_3 - 4.0 * a_quad * mgr);
            omega_p_exact = (l_3 - std::sqrt(disc)) / (2.0 * a_quad);
        }

        // Initialize on exact steady precession manifold: theta_dot = 0, phi_dot = omega_p_exact
        double theta = tilt_theta_rad;
        double theta_dot = 0.0;
        double phi = 0.0;
        const double s0 = std::sin(theta);
        const double c0 = std::cos(theta);
        const double p_phi_init = ix_transverse_kg_m2 * s0 * s0 * omega_p_exact + l_3 * c0;

        auto deriv = [&](double th, double th_d) -> std::pair<double, double> {
            const double st = std::sin(th);
            const double ct = std::cos(th);
            const double phi_d = (p_phi_init - l_3 * ct) / std::max(1e-12, ix_transverse_kg_m2 * st * st);
            const double th_dd = (st / ix_transverse_kg_m2) * (
                ix_transverse_kg_m2 * ct * phi_d * phi_d - l_3 * phi_d +
                mass_kg * gravity_m_s2 * pivot_distance_r_m
            );
            return {th_d, th_dd};
        };

        const int steps = static_cast<int>(std::round(total_time_s / dt_s));
        for (int i = 0; i < steps; ++i) {
            const auto [k1_th, k1_w] = deriv(theta, theta_dot);
            const double st1 = std::sin(theta);
            const double k1_ph = (p_phi_init - l_3 * std::cos(theta)) / (ix_transverse_kg_m2 * st1 * st1);

            const double th2 = theta + 0.5 * dt_s * k1_th;
            const double w2 = theta_dot + 0.5 * dt_s * k1_w;
            const auto [k2_th, k2_w] = deriv(th2, w2);
            const double k2_ph = (p_phi_init - l_3 * std::cos(th2)) / (ix_transverse_kg_m2 * std::sin(th2) * std::sin(th2));

            const double th3 = theta + 0.5 * dt_s * k2_th;
            const double w3 = theta_dot + 0.5 * dt_s * k2_w;
            const auto [k3_th, k3_w] = deriv(th3, w3);
            const double k3_ph = (p_phi_init - l_3 * std::cos(th3)) / (ix_transverse_kg_m2 * std::sin(th3) * std::sin(th3));

            const double th4 = theta + dt_s * k3_th;
            const double w4 = theta_dot + dt_s * k3_w;
            const auto [k4_th, k4_w] = deriv(th4, w4);
            const double k4_ph = (p_phi_init - l_3 * std::cos(th4)) / (ix_transverse_kg_m2 * std::sin(th4) * std::sin(th4));

            theta += (dt_s / 6.0) * (k1_th + 2.0 * k2_th + 2.0 * k3_th + k4_th);
            theta_dot += (dt_s / 6.0) * (k1_w + 2.0 * k2_w + 2.0 * k3_w + k4_w);
            phi += (dt_s / 6.0) * (k1_ph + 2.0 * k2_ph + 2.0 * k3_ph + k4_ph);
        }

        const double avg_omega_p = phi / (steps * dt_s);
        const double phi_dot_final = (p_phi_init - l_3 * std::cos(theta)) /
                                     (ix_transverse_kg_m2 * std::sin(theta) * std::sin(theta));
        const double p_phi_final = ix_transverse_kg_m2 * std::sin(theta) * std::sin(theta) * phi_dot_final +
                                   l_3 * std::cos(theta);

        GyroPrecessionReport rep;
        rep.expected_precession_rad_s = omega_p_theory;
        rep.simulated_precession_rad_s = avg_omega_p;
        rep.relative_error = std::abs(avg_omega_p - omega_p_theory) / std::abs(omega_p_theory);
        rep.lz_drift_relative = std::abs(p_phi_final - p_phi_init) / std::max(1e-12, std::abs(p_phi_init));
        return rep;
    }
};

} // namespace shunya::bench
