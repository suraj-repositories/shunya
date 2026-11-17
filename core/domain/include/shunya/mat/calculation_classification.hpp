#pragma once

#include "shunya/mat/element_behavior.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace shunya::mat {

/**
 * @brief Explicit 5-tier calculation classification system for scientific accuracy
 *        and provenance tracking.
 * (Roadmap task PH4-03.03.001)
 */
enum class CalculationTier : uint8_t {
    AnalyticalFormulation = 1,       ///< Tier 1: First-principles closed-form equations (e.g. CLT ABD matrix, Wiedemann-Franz)
    EmpiricalFit = 2,                ///< Tier 2: Calibrated polynomial/Arrhenius curves (e.g. NIST/JANAF, TCR)
    ReferenceDatasetLookup = 3,      ///< Tier 3: Measured tabular values from elements.csv or authoritative handbooks
    RuleOfMixturesApproximation = 4, ///< Tier 4: Voigt-Reuss bounds, Halpin-Tsai models, Vegard's law
    NumericalSimulationEstimate = 5  ///< Tier 5: Iterative solver approximation, numerical integration, relaxation
};

[[nodiscard]] constexpr std::string_view calculation_tier_to_string(CalculationTier tier) noexcept {
    switch (tier) {
    case CalculationTier::AnalyticalFormulation:       return "Analytical Formulation";
    case CalculationTier::EmpiricalFit:                return "Empirical Fit";
    case CalculationTier::ReferenceDatasetLookup:      return "Reference Dataset Lookup";
    case CalculationTier::RuleOfMixturesApproximation: return "Rule-of-Mixtures Approximation";
    case CalculationTier::NumericalSimulationEstimate: return "Numerical Simulation Estimate";
    }
    return "Unknown Calculation Tier";
}

[[nodiscard]] constexpr std::string_view calculation_tier_description(CalculationTier tier) noexcept {
    switch (tier) {
    case CalculationTier::AnalyticalFormulation:
        return "Derived from first-principles governing physics equations with closed-form solution.";
    case CalculationTier::EmpiricalFit:
        return "Calibrated empirical regression or polynomial fit against experimental standards (e.g. NIST, JANAF).";
    case CalculationTier::ReferenceDatasetLookup:
        return "Direct tabular extraction from authoritative measured reference databases (e.g. elements.csv, CRC).";
    case CalculationTier::RuleOfMixturesApproximation:
        return "Homogenization approximation based on constituent volume/mass fractions (e.g. Voigt-Reuss, Halpin-Tsai).";
    case CalculationTier::NumericalSimulationEstimate:
        return "Approximate solution obtained via discrete numerical simulation or non-linear iterative relaxation.";
    }
    return "";
}

/**
 * @brief Returns the canonical baseline relative uncertainty percentage (+/- delta%)
 *        associated with each calculation tier.
 */
[[nodiscard]] constexpr double default_tier_uncertainty_pct(CalculationTier tier) noexcept {
    switch (tier) {
    case CalculationTier::ReferenceDatasetLookup:      return 0.5;   // +/- 0.5%
    case CalculationTier::AnalyticalFormulation:       return 2.0;   // +/- 2.0%
    case CalculationTier::EmpiricalFit:                return 5.0;   // +/- 5.0%
    case CalculationTier::RuleOfMixturesApproximation: return 12.0;  // +/- 12.0%
    case CalculationTier::NumericalSimulationEstimate: return 20.0;  // +/- 20.0%
    }
    return 25.0;
}

/**
 * @brief Maps calculation classification tier to domain EpistemicStatus.
 */
[[nodiscard]] constexpr EpistemicStatus map_tier_to_epistemic_status(CalculationTier tier) noexcept {
    switch (tier) {
    case CalculationTier::AnalyticalFormulation:
    case CalculationTier::ReferenceDatasetLookup:
        return EpistemicStatus::ConfirmedScientific;
    case CalculationTier::EmpiricalFit:
    case CalculationTier::RuleOfMixturesApproximation:
    case CalculationTier::NumericalSimulationEstimate:
        return EpistemicStatus::ApproximateSimulation;
    }
    return EpistemicStatus::Unsupported;
}

/**
 * @brief Structured metadata representing the scientific provenance, classification,
 *        and uncertainty bounds of a material calculation.
 */
struct CalculationClassification {
    CalculationTier tier{CalculationTier::AnalyticalFormulation};
    std::string source_model_or_formula;
    std::string reference_standard;
    double relative_uncertainty_pct{2.0};
    bool is_empirically_validated{true};

    [[nodiscard]] bool is_high_fidelity() const noexcept;
    [[nodiscard]] EpistemicStatus epistemic_status() const noexcept;
    [[nodiscard]] std::string format_citation() const;
};

} // namespace shunya::mat
