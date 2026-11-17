#include "shunya/mat/gibbs_free_energy.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

double GibbsFreeEnergyEngine::compute_entropy_of_mixing(const std::vector<double>& mole_fractions) noexcept {
    if (mole_fractions.size() < 2) return 0.0;

    double sum = 0.0;
    double total_x = 0.0;
    for (double x : mole_fractions) {
        if (x > 0.0) total_x += x;
    }
    if (total_x <= 0.0) return 0.0;

    for (double x_raw : mole_fractions) {
        if (x_raw <= 1e-15) continue;
        const double x = x_raw / total_x;
        sum += x * std::log(x);
    }

    return -GAS_CONSTANT_R * sum; // Always >= 0
}

double GibbsFreeEnergyEngine::compute_enthalpy_of_mixing(
    const std::vector<double>& mole_fractions,
    double interaction_parameter_omega_j_mol) noexcept {
    if (mole_fractions.size() < 2) return 0.0;

    double total_x = 0.0;
    for (double x : mole_fractions) {
        if (x > 0.0) total_x += x;
    }
    if (total_x <= 0.0) return 0.0;

    double pair_sum = 0.0;
    const size_t n = mole_fractions.size();
    for (size_t i = 0; i < n; ++i) {
        const double xi = std::max(0.0, mole_fractions[i]) / total_x;
        for (size_t j = i + 1; j < n; ++j) {
            const double xj = std::max(0.0, mole_fractions[j]) / total_x;
            pair_sum += xi * xj;
        }
    }

    return interaction_parameter_omega_j_mol * pair_sum;
}

double GibbsFreeEnergyEngine::estimate_binary_interaction_parameter(
    double chi_a, double chi_b, double molar_vol_a_m3, double molar_vol_b_m3) noexcept {
    const double delta_chi = chi_a - chi_b;
    // Exothermic chemical bonding energy proportional to (Delta Chi)^2
    const double chemical_bonding = -40000.0 * (delta_chi * delta_chi); // J/mol

    // Elastic strain energy from atomic volume mismatch
    const double v_avg = 0.5 * (std::max(1e-7, molar_vol_a_m3) + std::max(1e-7, molar_vol_b_m3));
    const double v_diff = molar_vol_a_m3 - molar_vol_b_m3;
    const double size_strain = 20000.0 * (v_diff * v_diff) / (v_avg * v_avg); // J/mol

    return chemical_bonding + size_strain;
}

MixingThermodynamicsResult GibbsFreeEnergyEngine::evaluate_mixing_thermodynamics(
    const std::vector<double>& mole_fractions,
    double temperature_k,
    double interaction_parameter_omega_j_mol) {
    MixingThermodynamicsResult res;
    res.temperature_k = std::max(0.001, temperature_k);

    res.delta_s_j_mol_k = compute_entropy_of_mixing(mole_fractions);
    res.delta_h_j_mol = compute_enthalpy_of_mixing(mole_fractions, interaction_parameter_omega_j_mol);

    const double t_delta_s = res.temperature_k * res.delta_s_j_mol_k;
    res.delta_g_j_mol = res.delta_h_j_mol - t_delta_s;

    if (res.delta_g_j_mol < -1e-5) {
        res.is_spontaneous = true;
        res.thermodynamic_regime = "Spontaneous (Exergonic)";
    } else if (res.delta_g_j_mol > 1e-5) {
        res.is_spontaneous = false;
        res.thermodynamic_regime = "Non-Spontaneous (Endergonic)";
    } else {
        res.is_spontaneous = true;
        res.thermodynamic_regime = "Equilibrium";
    }

    if (interaction_parameter_omega_j_mol > 0.0 && mole_fractions.size() == 2) {
        res.critical_temperature_k = interaction_parameter_omega_j_mol / (2.0 * GAS_CONSTANT_R);
        if (res.temperature_k < res.critical_temperature_k) {
            res.has_miscibility_gap = true;
        }
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "Thermodynamic state at T = " << res.temperature_k << " K: "
       << "Delta G_mix = " << res.delta_g_j_mol << " J/mol ("
       << "Delta H_mix = " << res.delta_h_j_mol << " J/mol, "
       << "-T*Delta S_mix = " << -t_delta_s << " J/mol). "
       << "Regime: " << res.thermodynamic_regime << ".";
    res.thermodynamic_notes.push_back(ss.str());

    if (res.has_miscibility_gap) {
        std::ostringstream s_gap;
        s_gap << std::fixed << std::setprecision(1);
        s_gap << "Miscibility gap detected: T (" << res.temperature_k << " K) is below spinodal critical temperature T_c ("
              << res.critical_temperature_k << " K). Phase separation into constituent-rich phases predicted.";
        res.thermodynamic_notes.push_back(s_gap.str());
    }

    return res;
}

} // namespace shunya::mat
