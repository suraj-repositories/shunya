#include "shunya/mat/solution_solubility_engine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

double SolutionSolubilityEngine::compute_solubility_limit(
    const SolutionThermodynamicModel& model,
    double temperature_k) noexcept {
    const double t = std::max(1.0, temperature_k);
    const double t0 = std::max(1.0, model.reference_temp_k);

    const double inv_t_diff = (1.0 / t) - (1.0 / t0);
    const double exponent = -(model.delta_h_dissolution_j_mol / GAS_CONSTANT_R) * inv_t_diff;
    const double clamped_exponent = std::clamp(exponent, -50.0, 50.0);

    return std::max(0.0, model.base_solubility_g_per_100g * std::exp(clamped_exponent));
}

std::pair<double, double> SolutionSolubilityEngine::compute_electrolytic_dissociation(
    ElectrolyteType type,
    int ion_count,
    double solute_moles_per_liter,
    double temperature_k) noexcept {
    const double t = std::max(1.0, temperature_k);
    const double c = std::max(0.0, solute_moles_per_liter);
    const int nu = std::max(2, ion_count);

    if (type == ElectrolyteType::NonElectrolyte) {
        return {0.0, 1.0};
    }

    if (type == ElectrolyteType::StrongElectrolyte) {
        // Strong electrolytes completely dissociate (alpha ~ 1.0)
        const double alpha = 1.0;
        const double i_factor = 1.0 + (static_cast<double>(nu) - 1.0) * alpha;
        return {alpha, i_factor};
    }

    // Weak Electrolyte: Ostwald Dilution Law
    // K_d(T) = K_d0 * exp(-Delta H_ion / (R*T))
    const double kd_ref = 1.8e-5; // Acetic acid baseline
    const double delta_h_ion = -1200.0; // J/mol
    const double kd = kd_ref * std::exp(-(delta_h_ion / GAS_CONSTANT_R) * ((1.0 / t) - (1.0 / 298.15)));

    double alpha = 1.0;
    if (c > 1e-12) {
        const double disc = kd * kd + 4.0 * c * kd;
        alpha = (-kd + std::sqrt(std::max(0.0, disc))) / (2.0 * c);
    }
    alpha = std::clamp(alpha, 0.0, 1.0);
    const double i_factor = 1.0 + (static_cast<double>(nu) - 1.0) * alpha;

    return {alpha, i_factor};
}

double SolutionSolubilityEngine::compute_ionic_conductivity(
    const SolutionThermodynamicModel& model,
    double solute_moles_per_liter,
    double alpha,
    double temperature_k) noexcept {
    const double c = std::max(0.0, solute_moles_per_liter);
    if (c <= 1e-12 || model.electrolyte_type == ElectrolyteType::NonElectrolyte) {
        return 0.0;
    }

    // Kohlrausch's Law for molar conductivity: Lambda_m = Lambda_0 - K * sqrt(C)
    const double k_kohlrausch = 0.002;
    const double lambda_m = std::max(0.001, model.limiting_molar_conductivity_s_m2_mol - k_kohlrausch * std::sqrt(c));

    // Temperature coefficient ~2% per K relative to 298.15 K
    const double t_factor = 1.0 + 0.02 * (temperature_k - 298.15);

    // Moles per m^3 = c * 1000.0
    const double c_m3 = c * 1000.0;
    const double sigma = alpha * lambda_m * c_m3 * std::max(0.1, t_factor);

    return std::max(0.0, sigma);
}

SolutionSaturationResult SolutionSolubilityEngine::evaluate_solution(
    const SolutionThermodynamicModel& model,
    double solute_mass_g,
    double solvent_mass_g,
    double temperature_k,
    double solute_molar_mass_g_mol) {
    SolutionSaturationResult res;
    res.temperature_k = std::max(1.0, temperature_k);

    const double solvent_g = std::max(1e-6, solvent_mass_g);
    res.solubility_limit_g_per_100g = compute_solubility_limit(model, res.temperature_k);
    res.current_solute_concentration_g_per_100g = (solute_mass_g / solvent_g) * 100.0;

    const double max_dissolved_g = (res.solubility_limit_g_per_100g / 100.0) * solvent_g;

    if (solute_mass_g > max_dissolved_g + 1e-7) {
        res.state = SaturationState::Supersaturated;
        res.dissolved_solute_mass_g = max_dissolved_g;
        res.precipitated_mass_g = solute_mass_g - max_dissolved_g;
    } else if (std::abs(solute_mass_g - max_dissolved_g) <= 1e-7) {
        res.state = SaturationState::Saturated;
        res.dissolved_solute_mass_g = solute_mass_g;
        res.precipitated_mass_g = 0.0;
    } else {
        res.state = SaturationState::Unsaturated;
        res.dissolved_solute_mass_g = solute_mass_g;
        res.precipitated_mass_g = 0.0;
    }

    // Solution volume in liters (approximate density 1.0 g/mL)
    const double total_solution_mass_g = solvent_g + res.dissolved_solute_mass_g;
    const double solution_volume_liters = std::max(1e-6, total_solution_mass_g / 1000.0);

    const double solute_mm = std::max(1.0, solute_molar_mass_g_mol);
    const double solute_moles = res.dissolved_solute_mass_g / solute_mm;
    const double molarity = solute_moles / solution_volume_liters;

    const auto [alpha, i_factor] = compute_electrolytic_dissociation(
        model.electrolyte_type,
        model.ion_count_per_formula,
        molarity,
        res.temperature_k);

    res.degree_of_dissociation = alpha;
    res.vant_hoff_factor = i_factor;
    res.ionic_conductivity_s_m = compute_ionic_conductivity(model, molarity, alpha, res.temperature_k);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "Solution State: " << saturation_state_to_string(res.state) << " at T=" << res.temperature_k << " K. "
       << "Solubility limit: " << res.solubility_limit_g_per_100g << " g/100g. "
       << "Dissolved: " << res.dissolved_solute_mass_g << " g";
    if (res.precipitated_mass_g > 0.0) {
        ss << ", Precipitated: " << res.precipitated_mass_g << " g";
    }
    ss << ". van 't Hoff i=" << res.vant_hoff_factor << ", Conductivity=" << res.ionic_conductivity_s_m << " S/m.";
    res.summary_description = ss.str();

    return res;
}

} // namespace shunya::mat
