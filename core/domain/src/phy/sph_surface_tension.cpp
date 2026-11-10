#include "shunya/phy/sph_surface_tension.hpp"
#include "shunya/phy/sph_kernels.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

using core::base::math::Vector3;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Akinci (2013) cohesion spline weight on support r in [0, 2h]
double cohesion_spline(double r, double h_sup) noexcept {
    if (r <= 1e-15 || r >= h_sup || h_sup <= 0.0) return 0.0;
    const double q = r / h_sup;
    const double q3 = (1.0 - q);
    const double term = q3 * q3 * q3 * (q * q * q);
    if (q > 0.5) {
        return 2.0 * term;
    }
    return 2.0 * term - (1.0 / 64.0);
}

} // namespace

Vector3<double> SphSurfaceTensionModule::compute_cohesion_accel(
    const Vector3<double>& r_ij,
    double mass_j,
    double rho_i,
    double rho_j,
    double rest_density,
    double h,
    double sigma_n_m) noexcept {
    const double r = r_ij.length();
    const double h_sup = 2.0 * h;
    if (r <= 1e-15 || r >= h_sup || sigma_n_m <= 0.0) return {0.0, 0.0, 0.0};

    const double w_coh = cohesion_spline(r, h_sup);
    const double k_sym = (2.0 * rest_density) / std::max(1.0, rho_i + rho_j);
    // Attractive along -r_ij (towards j) normalized per particle spacing
    const double norm_scale = 32.0 / (kPi * std::pow(h_sup, 6));
    const double mag = -sigma_n_m * mass_j * k_sym * w_coh * norm_scale * (h * h * h) / r;
    return r_ij * mag;
}

Vector3<double> SphSurfaceTensionModule::compute_wall_adhesion_accel(
    const Vector3<double>& r_ib,
    double boundary_mass,
    double rho_0,
    double h,
    double sigma_n_m,
    double contact_angle_deg) noexcept {
    const double r = r_ib.length();
    const double h_sup = 2.0 * h;
    if (r <= 1e-15 || r >= h_sup || sigma_n_m <= 0.0) return {0.0, 0.0, 0.0};

    // Wetting strength scales with cos(theta): hydrophilic (< 90) attracts strongly, hydrophobic (> 90) repels
    const double theta_rad = contact_angle_deg * (kPi / 180.0);
    const double beta = 0.5 * (1.0 + std::cos(theta_rad));
    const double w = SphKernels::wendland_c2_3d(r, h);
    const double mag = -beta * sigma_n_m * (boundary_mass / std::max(1.0, rho_0)) * w * h / r;
    return r_ib * mag;
}

double SphSurfaceTensionModule::rayleigh_oscillation_omega(
    double surface_tension_n_m,
    double density_kg_m3,
    double radius_m,
    int mode_l) noexcept {
    if (surface_tension_n_m <= 0.0 || density_kg_m3 <= 0.0 || radius_m <= 0.0 || mode_l < 2) {
        return 0.0;
    }
    const double l = static_cast<double>(mode_l);
    return std::sqrt(l * (l - 1.0) * (l + 2.0) * surface_tension_n_m /
                     (density_kg_m3 * radius_m * radius_m * radius_m));
}

double SphSurfaceTensionModule::jurin_capillary_height_m(
    double surface_tension_n_m,
    double contact_angle_deg,
    double density_kg_m3,
    double tube_radius_m,
    double gravity_m_s2) noexcept {
    if (density_kg_m3 <= 0.0 || tube_radius_m <= 0.0 || gravity_m_s2 <= 0.0) return 0.0;
    const double theta_rad = contact_angle_deg * (kPi / 180.0);
    return (2.0 * surface_tension_n_m * std::cos(theta_rad)) /
           (density_kg_m3 * gravity_m_s2 * tube_radius_m);
}

} // namespace shunya::phy
