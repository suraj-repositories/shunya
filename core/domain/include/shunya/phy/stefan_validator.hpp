#pragma once

#include <cmath>

namespace shunya::phy {

/**
 * @brief Exact analytical 1D single-phase Stefan problem solver (transcendental similarity root lambda).
 */
class StefanValidator {
public:
    /**
     * @brief Solves lambda * exp(lambda^2) * erf(lambda) = St / sqrt(pi) via Newton-Raphson.
     * @param stefan_number St = cp * |T_wall - T_melt| / L_latent
     */
    [[nodiscard]] static double solve_transcendental_lambda(double stefan_number) noexcept {
        if (stefan_number <= 0.0) return 0.0;
        const double rhs = stefan_number / std::sqrt(3.14159265358979323846);
        double lam = std::sqrt(0.5 * stefan_number);
        for (int iter = 0; iter < 50; ++iter) {
            const double e2 = std::exp(lam * lam);
            const double erf_l = std::erf(lam);
            const double f = lam * e2 * erf_l - rhs;
            const double df = e2 * ((1.0 + 2.0 * lam * lam) * erf_l +
                                    (2.0 / std::sqrt(3.14159265358979323846)) * lam * std::exp(-lam * lam));
            if (std::abs(df) < 1e-18) break;
            const double step = f / df;
            lam = std::max(1e-9, lam - step);
            if (std::abs(step) < 1e-12) break;
        }
        return lam;
    }

    /**
     * @brief Analytical melting/freezing interface location s(t) = 2 * lambda * sqrt(alpha * t).
     */
    [[nodiscard]] static double interface_position_m(
        double stefan_number,
        double diffusivity_m2_s,
        double time_s) noexcept {
        if (time_s <= 0.0 || diffusivity_m2_s <= 0.0) return 0.0;
        const double lam = solve_transcendental_lambda(stefan_number);
        return 2.0 * lam * std::sqrt(diffusivity_m2_s * time_s);
    }
};

} // namespace shunya::phy
