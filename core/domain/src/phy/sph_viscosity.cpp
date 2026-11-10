#include "shunya/phy/sph_viscosity.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

using core::base::math::Vector3;

Vector3<double> SphViscosityModule::compute_pair_viscous_accel(
    const Vector3<double>& r_ij,
    const Vector3<double>& v_ij,
    const Vector3<double>& grad_w_ij,
    double mass_j,
    double rho_i,
    double rho_j,
    double h,
    const SphViscosityConfig& cfg) noexcept {

    const double r2 = r_ij.length_squared();
    const double eps = 0.01 * h * h;
    Vector3<double> acc{0.0, 0.0, 0.0};

    // 1) Morris / Cleary physical Newtonian viscous Laplacian (pulling v_i -> v_j)
    if (cfg.dynamic_viscosity_pa_s > 0.0) {
        const double r_dot_gw = r_ij.dot(grad_w_ij); // <= 0
        const double coeff = mass_j * (2.0 * cfg.dynamic_viscosity_pa_s / (rho_i * rho_j)) *
                             (r_dot_gw / (r2 + eps));
        // coeff <= 0, so coeff * (v_i - v_j) = |coeff| * (v_j - v_i)
        acc += v_ij * coeff;
    }

    // 2) Monaghan artificial viscosity for approaching pairs (stability & shock capturing)
    const double v_dot_r = v_ij.dot(r_ij);
    if (cfg.artificial_alpha > 0.0 && v_dot_r < 0.0) {
        const double mu_art = (h * v_dot_r) / (r2 + eps);
        const double rho_avg = 0.5 * (rho_i + rho_j);
        const double pi_ij = (-cfg.artificial_alpha * cfg.speed_of_sound_m_s * mu_art) / std::max(1.0, rho_avg);
        acc -= grad_w_ij * (mass_j * pi_ij);
    }

    return acc;
}

} // namespace shunya::phy
