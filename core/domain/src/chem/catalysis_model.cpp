#include "shunya/chem/catalysis_model.hpp"

#include <algorithm>

namespace shunya::chem {

double CatalysisModel::catalyzed_rate_constant(
    const ModifiedArrheniusParams& uncatalyzed,
    double activation_lowering_j_mol,
    double temp_k) noexcept {
    ModifiedArrheniusParams cat = uncatalyzed;
    cat.activation_ea_j_mol = std::max(0.0, uncatalyzed.activation_ea_j_mol - activation_lowering_j_mol);
    return ArrheniusRateEvaluator::modified_arrhenius(cat, temp_k);
}

double CatalysisModel::langmuir_coverage(
    const AdsorbateSpec& target,
    const std::vector<AdsorbateSpec>& all_adsorbates) noexcept {
    double denom = 1.0;
    for (const auto& a : all_adsorbates) {
        denom += std::max(0.0, a.equilibrium_K * a.partial_pressure_or_conc);
    }
    return std::max(0.0, target.equilibrium_K * target.partial_pressure_or_conc) / denom;
}

double CatalysisModel::langmuir_hinshelwood_rate(
    double k_surface,
    const AdsorbateSpec& reactant_a,
    const AdsorbateSpec& reactant_b,
    const std::vector<AdsorbateSpec>& all_adsorbates) noexcept {
    const double theta_a = langmuir_coverage(reactant_a, all_adsorbates);
    const double theta_b = langmuir_coverage(reactant_b, all_adsorbates);
    return k_surface * theta_a * theta_b;
}

} // namespace shunya::chem
