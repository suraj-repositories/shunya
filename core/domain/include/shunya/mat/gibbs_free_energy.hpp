#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace shunya::mat {

struct MixingThermodynamicsResult {
    bool is_spontaneous{true};
    double delta_g_j_mol{0.0};          // Delta G_mix in J/mol
    double delta_h_j_mol{0.0};          // Delta H_mix in J/mol
    double delta_s_j_mol_k{0.0};        // Delta S_mix in J/(mol*K)
    double temperature_k{298.15};
    double critical_temperature_k{0.0}; // Spinodal critical temp T_c (if Omega > 0)
    bool has_miscibility_gap{false};    // True if T < T_c and Omega > 0
    std::string thermodynamic_regime;   // "Spontaneous (Exergonic)", "Equilibrium", "Non-Spontaneous (Endergonic)"
    std::vector<std::string> thermodynamic_notes;
};

/**
 * @brief Thermodynamic engine evaluating Gibbs free energy of mixing:
 *        Delta G_mix = Delta H_mix - T * Delta S_mix
 *        Validates spontaneity and miscibility gaps for compounds and solutions.
 * (Roadmap task PH4-03.01.003)
 */
class GibbsFreeEnergyEngine {
public:
    static constexpr double GAS_CONSTANT_R = 8.31446261815324; // J/(mol*K)

    /**
     * @brief Computes molar entropy of ideal mixing:
     *        Delta S_mix = -R * sum(x_i * ln(x_i))
     */
    [[nodiscard]] static double compute_entropy_of_mixing(const std::vector<double>& mole_fractions) noexcept;

    /**
     * @brief Computes molar enthalpy of regular solution mixing:
     *        Delta H_mix = sum_{i < j} (Omega_{ij} * x_i * x_j)
     */
    [[nodiscard]] static double compute_enthalpy_of_mixing(
        const std::vector<double>& mole_fractions,
        double interaction_parameter_omega_j_mol = 0.0) noexcept;

    /**
     * @brief Evaluates Gibbs free energy of mixing Delta G_mix = Delta H_mix - T * Delta S_mix
     *        and checks spontaneity (Delta G_mix < 0) and miscibility gap.
     */
    [[nodiscard]] static MixingThermodynamicsResult evaluate_mixing_thermodynamics(
        const std::vector<double>& mole_fractions,
        double temperature_k = 298.15,
        double interaction_parameter_omega_j_mol = 0.0);

    /**
     * @brief Estimates binary interaction parameter Omega from Pauling electronegativity delta & average molar volume:
     *        Omega ~ -k_e * (Delta Chi)^2 + k_v * (Delta V)^2
     */
    [[nodiscard]] static double estimate_binary_interaction_parameter(
        double chi_a, double chi_b, double molar_vol_a_m3 = 1e-5, double molar_vol_b_m3 = 1e-5) noexcept;
};

} // namespace shunya::mat
