#include "tests/test_framework.hpp"

#include "shunya/bench/analytical_kepler.hpp"
#include "shunya/bench/analytical_top.hpp"
#include "shunya/bench/benchmark_report_gen.hpp"

#include <cmath>
#include <vector>

using namespace shunya::bench;

TEST_CASE("TEST-01-001 Keplerian closed orbits across 25 eccentricities & quadratic air drag") {
    // Sweep 25 eccentricities from circular (e=0.0) to highly eccentric (e=0.72)
    for (int idx = 0; idx < 25; ++idx) {
        const double e = static_cast<double>(idx) * 0.03;
        const auto res = BenchmarkKinematics::verify_kepler_orbit(2.5, e, 1.0, 5000);
        CHECK(res.rel_apogee_err < 1e-5);
        CHECK(res.rel_perigee_err < 1e-5);
    }

    // Projectile / free-fall quadratic air drag terminal speed v_term = sqrt(m * g / c_d)
    const double m = 2.0, c_d = 0.05, g = 9.80665;
    const double v_terminal = std::sqrt((m * g) / c_d);
    const double v_sim = BenchmarkKinematics::simulate_quadratic_drag_fall_speed(m, c_d, g, 20.0, 0.005);
    CHECK(std::abs(v_sim - v_terminal) / v_terminal < 1e-4);
}

TEST_CASE("TEST-01-001 Gyroscopic heavy top steady precession frequency and angular momentum conservation") {
    const std::vector<double> tilts_deg{20.0, 35.0, 55.0};
    const std::vector<double> spins_rad_s{800.0, 1200.0}; // Fast-top ~7600-11500 RPM regime

    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    for (double tilt : tilts_deg) {
        for (double omega_s : spins_rad_s) {
            const auto rep = BenchmarkGyroscopic::verify_steady_precession(
                0.45, 0.05, 8.0e-4, 1.2e-3, omega_s, tilt * kDegToRad);
            // Fast-top formula Omega_p = M*g*r / (I_z * omega_s) matches within < 0.1%
            CHECK(rep.relative_error < 1e-3);
            CHECK(rep.lz_drift_relative < 1e-10);
        }
    }
}
