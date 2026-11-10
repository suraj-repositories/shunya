#pragma once

#include "shunya/core/base/result.hpp"

#include <cmath>

namespace shunya::chem {

struct ModifiedArrheniusParams {
    double pre_exponential_A{1.0}; // s^-1 or (L/mol)^(order-1)/s
    double temp_exponent_n{0.0};   // T^n exponent
    double activation_ea_j_mol{0.0};
};

struct EyringTransitionParams {
    double delta_h_act_j_mol{50000.0};
    double delta_s_act_j_mol_k{-50.0};
    double transmission_kappa{1.0};
};

/**
 * @brief Evaluates temperature-dependent Modified Arrhenius, Eyring TST, and equilibrium K_eq reverse rate constants.
 */
class ArrheniusRateEvaluator {
public:
    static constexpr double kGasConstantR = 8.31446261815324;       // J / (mol * K)
    static constexpr double kBoltzmannKb = 1.380649e-23;            // J / K
    static constexpr double kPlanckH = 6.62607015e-34;              // J * s

    [[nodiscard]] static double modified_arrhenius(
        const ModifiedArrheniusParams& params,
        double temp_k) noexcept;

    [[nodiscard]] static double eyring_rate(
        const EyringTransitionParams& params,
        double temp_k) noexcept;

    /**
     * @brief Computes equilibrium constant K_eq = exp(-(dH - T*dS) / (R * T)) and reverse rate k_r = k_f / K_c.
     */
    [[nodiscard]] static double equilibrium_constant_keq(
        double delta_h_rxn_j_mol,
        double delta_s_rxn_j_mol_k,
        double temp_k) noexcept;

    [[nodiscard]] static double reverse_rate_from_equilibrium(
        double k_forward,
        double delta_h_rxn_j_mol,
        double delta_s_rxn_j_mol_k,
        double temp_k,
        int delta_n_gas = 0) noexcept;
};

} // namespace shunya::chem
