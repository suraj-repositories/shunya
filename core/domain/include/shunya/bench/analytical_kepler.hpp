#pragma once

#include "shunya/core/base/math.hpp"

#include <cmath>

namespace shunya::bench {

struct OrbitBenchmarkResult {
    double eccentricity{0.0};
    double expected_apogee{1.0};
    double simulated_apogee{1.0};
    double expected_perigee{1.0};
    double simulated_perigee{1.0};
    double rel_apogee_err{0.0};
    double rel_perigee_err{0.0};
};

/**
 * @brief Closed-form Keplerian orbital mechanics & quadratic-drag RK4 trajectory verifier.
 */
class BenchmarkKinematics {
public:
    static constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

    [[nodiscard]] static OrbitBenchmarkResult verify_kepler_orbit(
        double semi_major_a,
        double eccentricity,
        double mu = 1.0,
        int steps_per_orbit = 4000) noexcept {
        const double r_p_exact = semi_major_a * (1.0 - eccentricity);
        const double r_a_exact = semi_major_a * (1.0 + eccentricity);
        const double period = kTwoPi * std::sqrt((semi_major_a * semi_major_a * semi_major_a) / mu);
        const double dt = period / static_cast<double>(steps_per_orbit);

        // Start at perigee (x = r_p, y = 0, vx = 0, vy = v_perigee)
        double x = r_p_exact;
        double y = 0.0;
        double vx = 0.0;
        double vy = std::sqrt((mu / semi_major_a) * ((1.0 + eccentricity) / (1.0 - eccentricity)));

        double r_min = x;
        double r_max = x;

        auto accel = [&](double px, double py) {
            const double r2 = px * px + py * py;
            const double inv_r3 = 1.0 / (r2 * std::sqrt(r2));
            return std::pair<double, double>{-mu * px * inv_r3, -mu * py * inv_r3};
        };

        // 4th-order Runge-Kutta orbital integrator
        for (int s = 0; s < steps_per_orbit; ++s) {
            const auto [ax1, ay1] = accel(x, y);
            const double kx1 = vx, ky1 = vy, kvx1 = ax1, kvy1 = ay1;

            const double x2 = x + 0.5 * dt * kx1, y2 = y + 0.5 * dt * ky1;
            const double vx2 = vx + 0.5 * dt * kvx1, vy2 = vy + 0.5 * dt * kvy1;
            const auto [ax2, ay2] = accel(x2, y2);
            const double kx2 = vx2, ky2 = vy2, kvx2 = ax2, kvy2 = ay2;

            const double x3 = x + 0.5 * dt * kx2, y3 = y + 0.5 * dt * ky2;
            const double vx3 = vx + 0.5 * dt * kvx2, vy3 = vy + 0.5 * dt * kvy2;
            const auto [ax3, ay3] = accel(x3, y3);
            const double kx3 = vx3, ky3 = vy3, kvx3 = ax3, kvy3 = ay3;

            const double x4 = x + dt * kx3, y4 = y + dt * ky3;
            const double vx4 = vx + dt * kvx3, vy4 = vy + dt * kvy3;
            const auto [ax4, ay4] = accel(x4, y4);
            const double kx4 = vx4, ky4 = vy4, kvx4 = ax4, kvy4 = ay4;

            x += (dt / 6.0) * (kx1 + 2.0 * kx2 + 2.0 * kx3 + kx4);
            y += (dt / 6.0) * (ky1 + 2.0 * ky2 + 2.0 * ky3 + ky4);
            vx += (dt / 6.0) * (kvx1 + 2.0 * kvx2 + 2.0 * kvx3 + kvx4);
            vy += (dt / 6.0) * (kvy1 + 2.0 * kvy2 + 2.0 * kvy3 + kvy4);

            const double r = std::sqrt(x * x + y * y);
            r_min = std::min(r_min, r);
            r_max = std::max(r_max, r);
        }

        OrbitBenchmarkResult res;
        res.eccentricity = eccentricity;
        res.expected_apogee = r_a_exact;
        res.simulated_apogee = r_max;
        res.expected_perigee = r_p_exact;
        res.simulated_perigee = r_min;
        res.rel_apogee_err = std::abs(r_max - r_a_exact) / r_a_exact;
        res.rel_perigee_err = std::abs(r_min - r_p_exact) / r_p_exact;
        return res;
    }

    [[nodiscard]] static double simulate_quadratic_drag_fall_speed(
        double mass_kg,
        double drag_coeff_kg_m,
        double gravity_m_s2,
        double total_time_s,
        double dt_s) noexcept {
        double v = 0.0;
        const int steps = static_cast<int>(std::ceil(total_time_s / dt_s));
        for (int i = 0; i < steps; ++i) {
            const double a = gravity_m_s2 - (drag_coeff_kg_m / mass_kg) * v * std::abs(v);
            v += a * dt_s;
        }
        return v;
    }
};

} // namespace shunya::bench
