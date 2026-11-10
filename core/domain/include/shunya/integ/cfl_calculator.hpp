#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace shunya::integ {

/**
 * @brief Courant-Friedrichs-Lewy (CFL), thermal Fourier, and mechanical stiffness step-limit calculator.
 */
class CflCalculator {
public:
    [[nodiscard]] static double advective_cfl_dt(
        double dx_m,
        double max_velocity_m_s,
        double cfl_number = 0.5) noexcept {
        if (dx_m <= 0.0 || max_velocity_m_s <= 1e-12) {
            return std::numeric_limits<double>::infinity();
        }
        return std::clamp(cfl_number, 0.05, 1.0) * (dx_m / max_velocity_m_s);
    }

    [[nodiscard]] static double thermal_fourier_dt(
        double dx_m,
        double thermal_diffusivity_m2_s,
        double fourier_limit = 0.25) noexcept {
        if (dx_m <= 0.0 || thermal_diffusivity_m2_s <= 1e-18) {
            return std::numeric_limits<double>::infinity();
        }
        return std::clamp(fourier_limit, 0.01, 0.5) * (dx_m * dx_m) / thermal_diffusivity_m2_s;
    }

    [[nodiscard]] static double contact_oscillation_dt(
        double effective_mass_kg,
        double contact_stiffness_n_m,
        double samples_per_period = 20.0) noexcept {
        if (effective_mass_kg <= 0.0 || contact_stiffness_n_m <= 1e-12) {
            return std::numeric_limits<double>::infinity();
        }
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        const double period_s = kTwoPi * std::sqrt(effective_mass_kg / contact_stiffness_n_m);
        return period_s / std::max(4.0, samples_per_period);
    }
};

} // namespace shunya::integ
