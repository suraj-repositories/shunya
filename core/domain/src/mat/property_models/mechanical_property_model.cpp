#include "shunya/mat/property_models/mechanical_property_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

double MechanicalPropertyModel::derived_shear_modulus_pa() const noexcept {
    const double e = std::max(0.0, young_modulus_pa);
    const double nu = std::clamp(poisson_ratio, -0.99, 0.499);
    return e / (2.0 * (1.0 + nu));
}

double MechanicalPropertyModel::derived_bulk_modulus_pa() const noexcept {
    const double e = std::max(0.0, young_modulus_pa);
    const double nu = std::clamp(poisson_ratio, -0.99, 0.499);
    return e / (3.0 * (1.0 - 2.0 * nu));
}

double MechanicalPropertyModel::longitudinal_sound_speed_m_s() const noexcept {
    const double rho = std::max(1e-3, density_kg_m3);
    const double k = (bulk_modulus_pa > 0.0) ? bulk_modulus_pa : derived_bulk_modulus_pa();
    const double g = (shear_modulus_pa > 0.0) ? shear_modulus_pa : derived_shear_modulus_pa();
    const double m_modulus = k + (4.0 / 3.0) * g;
    return std::sqrt(std::max(0.0, m_modulus / rho));
}

double MechanicalPropertyModel::shear_sound_speed_m_s() const noexcept {
    const double rho = std::max(1e-3, density_kg_m3);
    const double g = (shear_modulus_pa > 0.0) ? shear_modulus_pa : derived_shear_modulus_pa();
    return std::sqrt(std::max(0.0, g / rho));
}

double MechanicalPropertyModel::critical_energy_release_rate_j_m2() const noexcept {
    const double e = std::max(1e6, young_modulus_pa);
    // K_1c is in MPa * sqrt(m) = 1e6 Pa * sqrt(m)
    const double k1c_pa_m05 = fracture_toughness_k1c_mpa_m05 * 1.0e6;
    const double nu = std::clamp(poisson_ratio, -0.99, 0.499);
    // Plane strain condition: G_1c = K_1c^2 * (1 - nu^2) / E
    const double e_prime = e / (1.0 - nu * nu);
    return (k1c_pa_m05 * k1c_pa_m05) / e_prime;
}

bool MechanicalPropertyModel::check_elastic_stability() const noexcept {
    if (density_kg_m3 <= 0.0) return false;
    if (young_modulus_pa <= 0.0) return false;
    if (poisson_ratio <= -1.0 || poisson_ratio >= 0.5) return false;
    if (shear_modulus_pa <= 0.0) return false;
    if (bulk_modulus_pa <= 0.0) return false;
    return true;
}

std::string MechanicalPropertyModel::to_summary_string() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "Mechanical: rho=" << density_kg_m3 << " kg/m^3, "
       << "E=" << (young_modulus_pa / 1e9) << " GPa, "
       << "nu=" << std::setprecision(2) << poisson_ratio << ", "
       << "G=" << std::setprecision(1) << (shear_modulus_pa / 1e9) << " GPa, "
       << "K=" << (bulk_modulus_pa / 1e9) << " GPa, "
       << "sigma_y=" << (yield_strength_pa / 1e6) << " MPa, "
       << "UTS=" << (ultimate_tensile_strength_pa / 1e6) << " MPa, "
       << "HV=" << hardness_vickers_hv << ", "
       << "K1c=" << fracture_toughness_k1c_mpa_m05 << " MPa*m^0.5, "
       << "c_L=" << longitudinal_sound_speed_m_s() << " m/s";
    return ss.str();
}

} // namespace shunya::mat
