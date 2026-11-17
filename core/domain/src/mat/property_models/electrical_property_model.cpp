#include "shunya/mat/property_models/electrical_property_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

double ElectricalPropertyModel::compute_drude_conductivity() const noexcept {
    return std::max(0.0, carrier_density_m3 * ELEMENTARY_CHARGE * carrier_mobility_m2_vs);
}

double ElectricalPropertyModel::resistivity_at(double temperature_k) const noexcept {
    if (classification == ElectricalClassification::Superconductor) {
        return 0.0;
    }
    const double t = std::max(0.1, temperature_k);

    if (classification == ElectricalClassification::Semiconductor && bandgap_ev > 0.0) {
        // Arrhenius intrinsic thermal generation: sigma ~ exp(-Eg / (2 * kB * T))
        const double exponent = -(bandgap_ev / (2.0 * BOLTZMANN_K_EV)) * ((1.0 / t) - (1.0 / reference_temp_k));
        const double sigma = base_conductivity_s_m * std::exp(std::clamp(exponent, -50.0, 50.0));
        return (sigma > 1e-18) ? (1.0 / sigma) : 1e18;
    }

    // Metallic TCR model: rho(T) = rho_0 * (1 + TCR * (T - T_0))
    const double delta_t = t - reference_temp_k;
    const double factor = 1.0 + tcr_per_k * delta_t;
    return std::max(1e-18, base_resistivity_ohm_m * std::max(0.01, factor));
}

double ElectricalPropertyModel::conductivity_at(double temperature_k) const noexcept {
    const double rho = resistivity_at(temperature_k);
    if (rho <= 1e-18) return 1e18; // Superconductor / extreme limit
    return 1.0 / rho;
}

double ElectricalPropertyModel::dielectric_breakdown_voltage(double thickness_m) const noexcept {
    return std::max(0.0, breakdown_field_v_m * std::max(0.0, thickness_m));
}

std::string ElectricalPropertyModel::to_summary_string() const {
    std::ostringstream ss;
    ss << electrical_classification_to_string(classification) << ": "
       << "sigma_0=" << base_conductivity_s_m << " S/m, "
       << "rho_0=" << base_resistivity_ohm_m << " Ohm*m, "
       << "TCR=" << tcr_per_k << "/K, "
       << "n=" << carrier_density_m3 << " m^-3, "
       << "mu=" << carrier_mobility_m2_vs << " m^2/(V*s), "
       << "eps_r=" << relative_permittivity << ", "
       << "E_break=" << breakdown_field_v_m << " V/m";
    return ss.str();
}

} // namespace shunya::mat
