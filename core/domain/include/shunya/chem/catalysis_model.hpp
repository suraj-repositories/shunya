#pragma once

#include "shunya/chem/arrhenius_evaluator.hpp"

#include <vector>

namespace shunya::chem {

struct AdsorbateSpec {
    double equilibrium_K{1.0}; // Adsorption equilibrium constant K_i
    double partial_pressure_or_conc{0.0};
};

/**
 * @brief Heterogeneous surface catalysis (activation barrier lowering + Langmuir-Hinshelwood adsorption kinetics).
 */
class CatalysisModel {
public:
    [[nodiscard]] static double catalyzed_rate_constant(
        const ModifiedArrheniusParams& uncatalyzed,
        double activation_lowering_j_mol,
        double temp_k) noexcept;

    /**
     * @brief Competitive Langmuir fractional surface coverage theta_target = K_t * P_t / (1 + sum K_j * P_j).
     */
    [[nodiscard]] static double langmuir_coverage(
        const AdsorbateSpec& target,
        const std::vector<AdsorbateSpec>& all_adsorbates) noexcept;

    [[nodiscard]] static double langmuir_hinshelwood_rate(
        double k_surface,
        const AdsorbateSpec& reactant_a,
        const AdsorbateSpec& reactant_b,
        const std::vector<AdsorbateSpec>& all_adsorbates) noexcept;
};

} // namespace shunya::chem
