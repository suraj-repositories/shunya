#pragma once

#include "shunya/core/base/math.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <random>

namespace shunya::env {

/**
 * @brief Dryden atmospheric turbulence PSD model and continuous harmonic synthesis generator.
 */
class DrydenTurbulenceGenerator {
public:
    static constexpr size_t kHarmonics = 16;

    explicit DrydenTurbulenceGenerator(
        double sigma_u_m_s = 1.0,
        double length_scale_m = 50.0,
        double mean_wind_speed_m_s = 10.0,
        uint64_t seed = 42)
        : sigma_u_(std::max(0.0, sigma_u_m_s)),
          length_scale_(std::max(0.5, length_scale_m)),
          mean_speed_(std::max(0.1, mean_wind_speed_m_s)) {
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * 3.14159265358979323846);

        const double omega_min = 0.05;
        const double omega_max = 15.0;
        const double d_log = std::log(omega_max / omega_min) / static_cast<double>(kHarmonics);

        for (size_t i = 0; i < kHarmonics; ++i) {
            const double w_lo = omega_min * std::exp(static_cast<double>(i) * d_log);
            const double w_hi = omega_min * std::exp(static_cast<double>(i + 1) * d_log);
            const double w_mid = 0.5 * (w_lo + w_hi);
            const double dw = w_hi - w_lo;

            omegas_[i] = w_mid;
            // Integrate PSD on each band: variance contribution = Phi_u(w) * dw
            const double psd = dryden_psd_u(w_mid);
            amplitudes_u_[i] = std::sqrt(2.0 * std::max(0.0, psd * dw));
            phases_x_[i] = phase_dist(rng);
            phases_y_[i] = phase_dist(rng);
            phases_z_[i] = phase_dist(rng);
        }
    }

    /**
     * @brief Single-sided Dryden longitudinal power spectral density Phi_u(omega) [(m/s)^2 / (rad/s)].
     */
    [[nodiscard]] double dryden_psd_u(double omega_rad_s) const noexcept {
        const double tau = length_scale_ / mean_speed_;
        const double denom = 1.0 + std::pow(tau * omega_rad_s, 2);
        constexpr double kPi = 3.14159265358979323846;
        return (2.0 * sigma_u_ * sigma_u_ * tau / kPi) / denom;
    }

    [[nodiscard]] core::base::math::Vector3<double> sample_fluctuation(
        double time_s,
        const core::base::math::Vector3<double>& pos_m = {0.0, 0.0, 0.0}) const noexcept {
        if (sigma_u_ <= 0.0) {
            return {0.0, 0.0, 0.0};
        }
        double ux = 0.0;
        double uy = 0.0;
        double uz = 0.0;
        const double advect = time_s + (pos_m.x / mean_speed_);
        for (size_t i = 0; i < kHarmonics; ++i) {
            const double arg = omegas_[i] * advect;
            ux += amplitudes_u_[i] * std::cos(arg + phases_x_[i]);
            uy += 0.5 * amplitudes_u_[i] * std::cos(arg + phases_y_[i]);
            uz += 0.7 * amplitudes_u_[i] * std::cos(arg + phases_z_[i]);
        }
        return {ux, uy, uz};
    }

    [[nodiscard]] double target_sigma_u() const noexcept { return sigma_u_; }

private:
    double sigma_u_;
    double length_scale_;
    double mean_speed_;
    std::array<double, kHarmonics> omegas_{};
    std::array<double, kHarmonics> amplitudes_u_{};
    std::array<double, kHarmonics> phases_x_{};
    std::array<double, kHarmonics> phases_y_{};
    std::array<double, kHarmonics> phases_z_{};
};

} // namespace shunya::env
