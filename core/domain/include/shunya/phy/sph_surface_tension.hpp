#pragma once

#include "shunya/core/base/math.hpp"

#include <cmath>

namespace shunya::phy {

struct SphSurfaceTensionConfig {
    double surface_tension_n_m{0.0728}; // Water-air sigma at 20 C (N/m)
    double contact_angle_deg{60.0};     // Young equilibrium wetting contact angle theta
};

/**
 * @brief Akinci-style pairwise surface tension cohesion, wall wetting adhesion, and Jurin/Rayleigh analytical helpers.
 */
class SphSurfaceTensionModule {
public:
    [[nodiscard]] static core::base::math::Vector3<double> compute_cohesion_accel(
        const core::base::math::Vector3<double>& r_ij,
        double mass_j,
        double rho_i,
        double rho_j,
        double rest_density,
        double h,
        double sigma_n_m) noexcept;

    [[nodiscard]] static core::base::math::Vector3<double> compute_wall_adhesion_accel(
        const core::base::math::Vector3<double>& r_ib,
        double boundary_mass,
        double rho_0,
        double h,
        double sigma_n_m,
        double contact_angle_deg) noexcept;

    /**
     * @brief Analytical Rayleigh oscillation frequency omega_n for l=2 spherical droplet mode.
     */
    [[nodiscard]] static double rayleigh_oscillation_omega(
        double surface_tension_n_m,
        double density_kg_m3,
        double radius_m,
        int mode_l = 2) noexcept;

    /**
     * @brief Jurin's Law analytical capillary rise height h = 2 * sigma * cos(theta) / (rho * g * r).
     */
    [[nodiscard]] static double jurin_capillary_height_m(
        double surface_tension_n_m,
        double contact_angle_deg,
        double density_kg_m3,
        double tube_radius_m,
        double gravity_m_s2 = 9.80665) noexcept;
};

} // namespace shunya::phy
