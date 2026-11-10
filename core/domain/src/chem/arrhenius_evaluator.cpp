#include "shunya/chem/arrhenius_evaluator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

double ArrheniusRateEvaluator::modified_arrhenius(
    const ModifiedArrheniusParams& params,
    double temp_k) noexcept {
    if (temp_k <= 0.0 || params.pre_exponential_A <= 0.0) return 0.0;
    const double expo = std::clamp(-params.activation_ea_j_mol / (kGasConstantR * temp_k), -500.0, 500.0);
    return params.pre_exponential_A * std::pow(temp_k, params.temp_exponent_n) * std::exp(expo);
}

double ArrheniusRateEvaluator::eyring_rate(
    const EyringTransitionParams& params,
    double temp_k) noexcept {
    if (temp_k <= 0.0 || params.transmission_kappa <= 0.0) return 0.0;
    const double dg_act = params.delta_h_act_j_mol - temp_k * params.delta_s_act_j_mol_k;
    const double prefactor = params.transmission_kappa * (kBoltzmannKb * temp_k / kPlanckH);
    const double expo = std::clamp(-dg_act / (kGasConstantR * temp_k), -500.0, 500.0);
    return prefactor * std::exp(expo);
}

double ArrheniusRateEvaluator::equilibrium_constant_keq(
    double delta_h_rxn_j_mol,
    double delta_s_rxn_j_mol_k,
    double temp_k) noexcept {
    if (temp_k <= 0.0) return 0.0;
    const double dg = delta_h_rxn_j_mol - temp_k * delta_s_rxn_j_mol_k;
    const double expo = std::clamp(-dg / (kGasConstantR * temp_k), -500.0, 500.0);
    return std::exp(expo);
}

double ArrheniusRateEvaluator::reverse_rate_from_equilibrium(
    double k_forward,
    double delta_h_rxn_j_mol,
    double delta_s_rxn_j_mol_k,
    double temp_k,
    int delta_n_gas) noexcept {
    const double kp = equilibrium_constant_keq(delta_h_rxn_j_mol, delta_s_rxn_j_mol_k, temp_k);
    // For ideal gases: K_c = K_p * (R*T / P_std)^(-delta_n); using standard Molar reference RT in kPa*L/(mol*K) -> 1 bar = 100 kPa
    const double rt_ratio = (kGasConstantR * temp_k) / 100.0; // in L*bar/(mol*K) ~ 0.08314 * T / 100? Wait: R_Lbar = 0.0831446
    (void)rt_ratio;
    constexpr double kRLiterBar = 0.08314462618; // L * bar / (mol * K)
    const double kc = kp * std::pow(kRLiterBar * temp_k, -delta_n_gas);
    return (kc > 1e-30) ? (k_forward / kc) : 0.0;
}

} // namespace shunya::chem
