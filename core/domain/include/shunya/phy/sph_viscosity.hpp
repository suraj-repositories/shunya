#pragma once

#include "shunya/core/base/math.hpp"

namespace shunya::phy {

struct SphViscosityConfig {
    double dynamic_viscosity_pa_s{1.0e-3}; // Physical dynamic viscosity mu
    double artificial_alpha{0.05};         // Monaghan shock/stability stabilization
    double speed_of_sound_m_s{30.0};
};

/**
 * @brief Morris physical Newtonian viscous shear acceleration and Monaghan artificial viscosity.
 */
class SphViscosityModule {
public:
    [[nodiscard]] static core::base::math::Vector3<double> compute_pair_viscous_accel(
        const core::base::math::Vector3<double>& r_ij,
        const core::base::math::Vector3<double>& v_ij,
        const core::base::math::Vector3<double>& grad_w_ij,
        double mass_j,
        double rho_i,
        double rho_j,
        double h,
        const SphViscosityConfig& cfg) noexcept;
};

} // namespace shunya::phy
