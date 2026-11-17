#include "shunya/mat/property_models/thermal_property_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

double ThermalPropertyModel::thermal_conductivity_at(double temperature_k) const noexcept {
    const double t = std::max(0.1, temperature_k);
    const double delta_t = t - reference_temp_k;
    const double factor = 1.0 + thermal_conductivity_temp_coeff * delta_t;
    return std::max(0.001, base_thermal_conductivity_w_mk * std::max(0.05, factor));
}

double ThermalPropertyModel::specific_heat_at(double temperature_k) const noexcept {
    const double t = std::max(0.1, temperature_k);
    const double delta_t = t - reference_temp_k;
    const double factor = 1.0 + cp_temp_coeff * delta_t;
    return std::max(1.0, base_specific_heat_j_kgk * std::max(0.1, factor));
}

double ThermalPropertyModel::thermal_expansion_at(double temperature_k) const noexcept {
    const double t = std::max(0.1, temperature_k);
    const double delta_t = t - reference_temp_k;
    // Slight linear increase of thermal expansion at elevated temperatures
    const double factor = 1.0 + 0.0005 * delta_t;
    return linear_thermal_expansion_1_k * std::max(0.1, factor);
}

double ThermalPropertyModel::compute_thermal_strain(double t_start_k, double t_end_k) const noexcept {
    const double t_avg = 0.5 * (t_start_k + t_end_k);
    const double alpha = thermal_expansion_at(t_avg);
    return alpha * (t_end_k - t_start_k);
}

double ThermalPropertyModel::compute_thermal_diffusivity(
    double density_kg_m3, double temperature_k) const noexcept {
    const double rho = std::max(1e-3, density_kg_m3);
    const double k = thermal_conductivity_at(temperature_k);
    const double cp = specific_heat_at(temperature_k);
    return k / (rho * cp);
}

double ThermalPropertyModel::estimate_electronic_thermal_conductivity(
    double electrical_conductivity_s_m, double temperature_k) noexcept {
    const double t = std::max(0.0, temperature_k);
    const double sigma = std::max(0.0, electrical_conductivity_s_m);
    // Wiedemann-Franz Law: k_e = L * sigma * T
    return LORENZ_NUMBER * sigma * t;
}

std::string ThermalPropertyModel::to_summary_string() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "Thermal Model: k_0=" << base_thermal_conductivity_w_mk << " W/(m*K), "
       << "Cp_0=" << base_specific_heat_j_kgk << " J/(kg*K), "
       << "alpha=" << (linear_thermal_expansion_1_k * 1e6) << " um/(m*K), "
       << "Tm=" << melting_point_k << " K, Tb=" << boiling_point_k << " K, "
       << "H_fus=" << (heat_of_fusion_j_kg / 1e3) << " kJ/kg, "
       << "H_vap=" << (heat_of_vaporization_j_kg / 1e6) << " MJ/kg";
    return ss.str();
}

} // namespace shunya::mat
