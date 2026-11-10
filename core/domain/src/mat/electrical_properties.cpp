#include "shunya/mat/electrical_properties.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

namespace {
inline constexpr double kBoltzmannEvPerK = 8.617333262e-5;
}

double ElectricalPropertyEvaluator::metal_resistivity_ohm_m(const MetalResistivityParams& p,
                                                            double temp_k) noexcept {
    const double rho = p.rho_0_ohm_m * (1.0 + p.alpha_per_k * (temp_k - p.ref_temp_k));
    return std::max(1e-12, rho);
}

double ElectricalPropertyEvaluator::step_resistor_temperature_k(
    const MetalResistivityParams& p,
    double current_temp_k,
    double current_a,
    double length_m,
    double area_m2,
    double thermal_mass_j_k,
    double ambient_conductance_w_k,
    double ambient_temp_k,
    double dt_s) noexcept {
    const double rho = metal_resistivity_ohm_m(p, current_temp_k);
    const double r_ohm = rho * (length_m / std::max(1e-18, area_m2));
    const double p_joule = current_a * current_a * r_ohm;
    const double p_loss = ambient_conductance_w_k * (current_temp_k - ambient_temp_k);
    return current_temp_k + ((p_joule - p_loss) / std::max(1e-12, thermal_mass_j_k)) * dt_s;
}

double ElectricalPropertyEvaluator::varshni_bandgap_ev(const SemiconductorVarshniParams& p,
                                                         double temp_k) noexcept {
    const double T = std::max(0.0, temp_k);
    const double drop = (p.alpha_ev_k * T * T) / (T + p.beta_k);
    return std::max(0.0, p.eg0_ev - drop);
}

double ElectricalPropertyEvaluator::intrinsic_carrier_cm3(const SemiconductorVarshniParams& p,
                                                          double temp_k) noexcept {
    if (temp_k <= 1.0) return 0.0;
    const double eg = varshni_bandgap_ev(p, temp_k);
    const double t_ratio = temp_k / 300.0;
    const double eff_dos = std::sqrt(p.nc_300_cm3 * p.nv_300_cm3) * std::pow(t_ratio, 1.5);
    return eff_dos * std::exp(-eg / (2.0 * kBoltzmannEvPerK * temp_k));
}

MetalResistivityParams ElectricalPropertyEvaluator::copper() {
    return {1.68e-8, 293.15, 0.00393};
}

SemiconductorVarshniParams ElectricalPropertyEvaluator::silicon() {
    return {1.1695, 4.73e-4, 636.0, 2.86e19, 3.10e19};
}

SemiconductorVarshniParams ElectricalPropertyEvaluator::germanium() {
    return {0.7437, 4.774e-4, 235.0, 1.04e19, 6.00e18};
}

SemiconductorVarshniParams ElectricalPropertyEvaluator::gallium_arsenide() {
    return {1.519, 5.405e-4, 204.0, 4.35e17, 9.00e18};
}

} // namespace shunya::mat
