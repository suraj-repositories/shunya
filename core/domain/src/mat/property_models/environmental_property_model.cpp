#include "shunya/mat/property_models/environmental_property_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

bool EnvironmentalPropertyModel::is_oxide_protective() const noexcept {
    // Pilling-Bedworth criterion: 1.0 <= PBR <= 2.0 provides continuous passivating protective layer
    return (pilling_bedworth_ratio >= 1.0 && pilling_bedworth_ratio <= 2.0);
}

double EnvironmentalPropertyModel::compute_hygroscopic_strain(double moisture_content_pct) const noexcept {
    const double m = std::clamp(moisture_content_pct, 0.0, saturation_moisture_content_pct);
    return hygroscopic_expansion_coefficient * (m / 100.0);
}

double EnvironmentalPropertyModel::compute_oxide_mass_gain(
    double time_seconds, double temperature_k) const noexcept {
    const double t = std::max(0.0, time_seconds);
    const double temp = std::max(1.0, temperature_k);

    // Arrhenius rate: kp(T) = kp_0 * exp(-Ea / (R * T))
    const double kp_t = oxidation_parabolic_rate_kp *
                        std::exp(-(oxidation_activation_energy_j_mol / GAS_CONSTANT_R) *
                                 ((1.0 / temp) - (1.0 / 293.15)));

    // Parabolic law: (Delta m / A)^2 = kp * t -> Delta m / A = sqrt(kp * t)
    return std::sqrt(std::max(0.0, kp_t * t));
}

std::pair<double, GalvanicRiskLevel> EnvironmentalPropertyModel::evaluate_galvanic_coupling(
    double potential_v1, double potential_v2) noexcept {
    const double delta_v = std::abs(potential_v1 - potential_v2);

    if (delta_v <= 0.15) {
        return {delta_v, GalvanicRiskLevel::Compatible};
    } else if (delta_v <= 0.25) {
        return {delta_v, GalvanicRiskLevel::ControlledAcceptable};
    } else if (delta_v <= 0.50) {
        return {delta_v, GalvanicRiskLevel::ModerateRisk};
    } else {
        return {delta_v, GalvanicRiskLevel::SevereCorrosionHazard};
    }
}

std::string EnvironmentalPropertyModel::to_summary_string() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "Environmental: M_inf=" << saturation_moisture_content_pct << "%, "
       << "UV Index=" << uv_resistance_index << ", "
       << "PBR=" << pilling_bedworth_ratio << " (" << (is_oxide_protective() ? "Passivating/Protective" : "Non-Protective") << "), "
       << "E0=" << standard_reduction_potential_v << " V vs SHE";
    return ss.str();
}

} // namespace shunya::mat
