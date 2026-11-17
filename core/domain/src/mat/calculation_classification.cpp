#include "shunya/mat/calculation_classification.hpp"

#include <sstream>
#include <iomanip>

namespace shunya::mat {

bool CalculationClassification::is_high_fidelity() const noexcept {
    return tier == CalculationTier::AnalyticalFormulation ||
           tier == CalculationTier::ReferenceDatasetLookup ||
           (tier == CalculationTier::EmpiricalFit && is_empirically_validated);
}

EpistemicStatus CalculationClassification::epistemic_status() const noexcept {
    return map_tier_to_epistemic_status(tier);
}

std::string CalculationClassification::format_citation() const {
    std::ostringstream oss;
    oss << "[" << calculation_tier_to_string(tier) << "] "
        << (source_model_or_formula.empty() ? "Unspecified Model" : source_model_or_formula);
    if (!reference_standard.empty()) {
        oss << " (ref: " << reference_standard << ")";
    }
    oss << " +/- " << std::fixed << std::setprecision(1) << relative_uncertainty_pct << "%";
    return oss.str();
}

} // namespace shunya::mat
