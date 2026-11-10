#include "shunya/integ/thermo_mechanical_connector.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::integ {

double ThermoMechanicalConnector::bimetallic_tip_deflection_m(
    const BimaterialBeamSpec& spec,
    double delta_temp_k) noexcept {
    // Classic Timoshenko bimetallic strip curvature formula
    const double h1 = spec.layer1_thickness_m;
    const double h2 = spec.layer2_thickness_m;
    const double h = h1 + h2;
    if (h <= 0.0 || h1 <= 0.0 || h2 <= 0.0 || spec.e1_pa <= 0.0 || spec.e2_pa <= 0.0) {
        return 0.0;
    }
    const double m = h1 / h2;
    const double n = spec.e1_pa / spec.e2_pa;
    const double denom = 3.0 * std::pow(1.0 + m, 2) +
                         (1.0 + m * n) * (m * m + 1.0 / (m * n));
    const double curvature = (6.0 * (spec.alpha2_per_k - spec.alpha1_per_k) *
                              delta_temp_k * std::pow(1.0 + m, 2)) / (h * denom);
    // Cantilever tip deflection delta = 0.5 * kappa * L^2
    return 0.5 * curvature * spec.length_m * spec.length_m;
}

double ThermoMechanicalConnector::euler_critical_buckling_delta_t_k(
    const SlenderColumnBucklingSpec& spec) noexcept {
    constexpr double kPi = 3.14159265358979323846;
    const double l_eff = spec.end_factor_k * spec.length_m;
    if (l_eff <= 0.0 || spec.cross_section_area_m2 <= 0.0 || spec.cte_alpha_per_k <= 0.0) {
        return 0.0;
    }
    return (kPi * kPi * spec.second_moment_i_m4) /
           (spec.cte_alpha_per_k * spec.cross_section_area_m2 * l_eff * l_eff);
}

core::base::Result<double> ThermoMechanicalConnector::axial_thermal_stress_pa(
    const SlenderColumnBucklingSpec& spec,
    double delta_temp_k,
    bool& out_buckled) {
    if (spec.length_m <= 0.0 || spec.youngs_modulus_pa <= 0.0) {
        return core::base::Error{
            425, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid column geometry or modulus for thermal stress", ""
        };
    }
    const double delta_t_crit = euler_critical_buckling_delta_t_k(spec);
    out_buckled = (delta_temp_k >= delta_t_crit && delta_t_crit > 0.0);
    // Constrained compressive thermal stress capped post-buckling at Euler critical stress
    const double eff_dt = out_buckled ? delta_t_crit : delta_temp_k;
    return spec.youngs_modulus_pa * spec.cte_alpha_per_k * eff_dt;
}

double ThermoMechanicalConnector::taylor_quinney_plastic_temp_rise_k(
    double yield_stress_pa,
    double plastic_strain_increment,
    double density_kg_m3,
    double specific_heat_j_kg_k,
    double taylor_quinney_beta) noexcept {
    if (density_kg_m3 <= 0.0 || specific_heat_j_kg_k <= 0.0 || plastic_strain_increment <= 0.0) {
        return 0.0;
    }
    const double beta = std::clamp(taylor_quinney_beta, 0.0, 1.0);
    return (beta * yield_stress_pa * plastic_strain_increment) /
           (density_kg_m3 * specific_heat_j_kg_k);
}

} // namespace shunya::integ
