#pragma once

#include <algorithm>
#include <cmath>

namespace shunya::phy {

struct FluidPropertiesAirWater {
    double k_w_m_k{0.0263};      // Fluid thermal conductivity
    double nu_m2_s{1.589e-5};    // Kinematic viscosity
    double prandtl{0.707};       // Prandtl number nu / alpha
    double beta_per_k{3.33e-3};  // Thermal expansion coefficient 1 / T_film
};

/**
 * @brief Empirical Nusselt correlations (Churchill-Chu & Churchill-Bernstein) and dimensionless numbers.
 */
class NusseltCorrelations {
public:
    static constexpr double kGravity = 9.80665;

    [[nodiscard]] static double grashof(
        double beta_per_k, double delta_t_k, double char_len_m, double nu_m2_s) noexcept {
        return kGravity * beta_per_k * std::abs(delta_t_k) *
               (char_len_m * char_len_m * char_len_m) / std::max(1e-24, nu_m2_s * nu_m2_s);
    }

    [[nodiscard]] static double rayleigh(
        double beta_per_k, double delta_t_k, double char_len_m, double nu_m2_s, double prandtl) noexcept {
        return grashof(beta_per_k, delta_t_k, char_len_m, nu_m2_s) * prandtl;
    }

    [[nodiscard]] static double reynolds(double velocity_m_s, double char_len_m, double nu_m2_s) noexcept {
        return std::abs(velocity_m_s) * char_len_m / std::max(1e-18, nu_m2_s);
    }

    /**
     * @brief Churchill-Chu (1975) correlation for free convection on a vertical flat plate over all Rayleigh regimes.
     */
    [[nodiscard]] static double churchill_chu_vertical_plate(double ra, double pr) noexcept {
        if (ra <= 0.0) return 0.68;
        const double denom = std::pow(1.0 + std::pow(0.492 / std::max(1e-4, pr), 9.0 / 16.0), 8.0 / 27.0);
        const double inner = 0.825 + (0.387 * std::pow(ra, 1.0 / 6.0)) / denom;
        return inner * inner;
    }

    /**
     * @brief Churchill-Chu (1975) correlation for free convection around a horizontal cylinder.
     */
    [[nodiscard]] static double churchill_chu_horizontal_cylinder(double ra, double pr) noexcept {
        if (ra <= 0.0) return 0.60;
        const double denom = std::pow(1.0 + std::pow(0.559 / std::max(1e-4, pr), 9.0 / 16.0), 8.0 / 27.0);
        const double inner = 0.60 + (0.387 * std::pow(ra, 1.0 / 6.0)) / denom;
        return inner * inner;
    }

    /**
     * @brief Churchill-Bernstein (1977) correlation for forced cross-flow convection over a cylinder.
     */
    [[nodiscard]] static double churchill_bernstein_cylinder(double re, double pr) noexcept {
        if (re <= 0.0) return 0.3;
        const double term1 = (0.62 * std::sqrt(re) * std::cbrt(pr)) /
                             std::pow(1.0 + std::pow(0.4 / std::max(1e-4, pr), 2.0 / 3.0), 0.25);
        const double term2 = std::pow(1.0 + std::pow(re / 282000.0, 5.0 / 8.0), 0.8);
        return 0.3 + term1 * term2;
    }
};

} // namespace shunya::phy
