#pragma once

#include "shunya/mat/calculation_classification.hpp"

#include <cmath>
#include <string>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

/**
 * @brief Represents a physical property scalar value paired with its epistemic status,
 *        calculation classification tier, and numerical confidence bounds (+/- delta%).
 * (Roadmap task PH4-03.03.002)
 */
struct EpistemicScalar {
    double value{0.0};
    CalculationClassification classification;

    constexpr EpistemicScalar() = default;

    constexpr EpistemicScalar(double val, CalculationTier tier, double uncertainty_pct = -1.0)
        : value(val),
          classification{tier, "", "", (uncertainty_pct >= 0.0 ? uncertainty_pct : default_tier_uncertainty_pct(tier)), true} {}

    EpistemicScalar(double val, CalculationClassification meta)
        : value(val), classification(std::move(meta)) {}

    [[nodiscard]] constexpr double lower_bound() const noexcept {
        return value - std::abs(value) * (classification.relative_uncertainty_pct / 100.0);
    }

    [[nodiscard]] constexpr double upper_bound() const noexcept {
        return value + std::abs(value) * (classification.relative_uncertainty_pct / 100.0);
    }

    [[nodiscard]] constexpr double absolute_uncertainty() const noexcept {
        return std::abs(value) * (classification.relative_uncertainty_pct / 100.0);
    }

    [[nodiscard]] constexpr bool contains(double test_val) const noexcept {
        return test_val >= lower_bound() && test_val <= upper_bound();
    }

    [[nodiscard]] bool is_scientifically_certified() const noexcept {
        return classification.is_high_fidelity();
    }

    [[nodiscard]] EpistemicStatus epistemic_status() const noexcept {
        return classification.epistemic_status();
    }

    [[nodiscard]] std::string to_string(int precision = 3, std::string_view unit = "") const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        if (!unit.empty()) {
            oss << " " << unit;
        }
        oss << " +/- " << std::fixed << std::setprecision(1) << classification.relative_uncertainty_pct << "%"
            << " [" << calculation_tier_to_string(classification.tier) << "]"
            << " (" << lower_bound() << " .. " << upper_bound() << ")";
        return oss.str();
    }

    /**
     * @brief Gaussian quadrature error propagation for addition: z = a + b,
     *        dz = sqrt(da^2 + db^2).
     */
    [[nodiscard]] static EpistemicScalar add_propagated(
        const EpistemicScalar& a, const EpistemicScalar& b,
        CalculationTier combined_tier = CalculationTier::AnalyticalFormulation,
        std::string_view formula_desc = "Summation") {
        const double val = a.value + b.value;
        const double da = a.absolute_uncertainty();
        const double db = b.absolute_uncertainty();
        const double dz = std::sqrt(da * da + db * db);
        const double rel_pct = (std::abs(val) > 1e-15) ? (dz / std::abs(val) * 100.0) : 0.0;

        CalculationClassification meta;
        meta.tier = combined_tier;
        meta.source_model_or_formula = std::string(formula_desc);
        meta.relative_uncertainty_pct = rel_pct;
        meta.is_empirically_validated = a.classification.is_empirically_validated && b.classification.is_empirically_validated;
        return EpistemicScalar(val, meta);
    }

    /**
     * @brief Gaussian error propagation for multiplication: z = a * b,
     *        (dz / z) = sqrt((da / a)^2 + (db / b)^2).
     */
    [[nodiscard]] static EpistemicScalar multiply_propagated(
        const EpistemicScalar& a, const EpistemicScalar& b,
        CalculationTier combined_tier = CalculationTier::AnalyticalFormulation,
        std::string_view formula_desc = "Product") {
        const double val = a.value * b.value;
        const double rel_a = a.classification.relative_uncertainty_pct;
        const double rel_b = b.classification.relative_uncertainty_pct;
        const double rel_z = std::sqrt(rel_a * rel_a + rel_b * rel_b);

        CalculationClassification meta;
        meta.tier = combined_tier;
        meta.source_model_or_formula = std::string(formula_desc);
        meta.relative_uncertainty_pct = rel_z;
        meta.is_empirically_validated = a.classification.is_empirically_validated && b.classification.is_empirically_validated;
        return EpistemicScalar(val, meta);
    }
};

} // namespace shunya::mat
