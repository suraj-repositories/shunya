#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/epistemic_value.hpp"

#include <string>
#include <string_view>
#include <type_traits>

namespace shunya::mat {

enum class ScientificFidelityBadge : uint8_t {
    CertifiedScientific,  ///< Strict experimental or first-principles derivation with low uncertainty
    ApproximatePreview    ///< Preliminary preview / Rule of Mixtures / simulation estimate
};

[[nodiscard]] constexpr std::string_view fidelity_badge_to_string(ScientificFidelityBadge b) noexcept {
    switch (b) {
    case ScientificFidelityBadge::CertifiedScientific: return "CERTIFIED SCIENTIFIC";
    case ScientificFidelityBadge::ApproximatePreview:   return "APPROXIMATE PREVIEW";
    }
    return "UNKNOWN";
}

struct UiScientificPresentation {
    ScientificFidelityBadge badge{ScientificFidelityBadge::ApproximatePreview};
    std::string badge_text;         // e.g. "[CERTIFIED]" or "[PREVIEW]"
    std::string css_class;          // "badge-certified-scientific" or "badge-approximate-preview"
    std::string formatted_value;    // e.g. "401.0 W/(m*K) +/- 1.0%"
    std::string tooltip_citation;   // e.g. "Validated via ASTM E1461. Analytical Formulation."
    bool requires_disclaimer{true}; // True for preview estimates to prevent design sign-off
};

/**
 * @brief Distinct C++ strong type representing a verified, scientifically certified property value.
 *        Guarantees that uncertified approximate previews cannot be silently substituted
 *        into certified engineering pipelines.
 */
template <typename T>
class CertifiedScientificResult {
public:
    CertifiedScientificResult(T val, CalculationClassification meta)
        : value_(std::move(val)), metadata_(std::move(meta)) {}

    [[nodiscard]] const T& value() const noexcept { return value_; }
    [[nodiscard]] const CalculationClassification& metadata() const noexcept { return metadata_; }
    [[nodiscard]] static constexpr bool is_certified() noexcept { return true; }

    [[nodiscard]] UiScientificPresentation presentation(std::string_view unit = "") const {
        UiScientificPresentation ui;
        ui.badge = ScientificFidelityBadge::CertifiedScientific;
        ui.badge_text = "[CERTIFIED SCIENTIFIC]";
        ui.css_class = "badge-certified-scientific";
        std::ostringstream oss;
        oss << value_;
        if (!unit.empty()) oss << " " << unit;
        oss << " +/- " << std::fixed << std::setprecision(1) << metadata_.relative_uncertainty_pct << "%";
        ui.formatted_value = oss.str();
        ui.tooltip_citation = metadata_.format_citation();
        ui.requires_disclaimer = false;
        return ui;
    }

private:
    T value_;
    CalculationClassification metadata_;
};

/**
 * @brief Distinct C++ strong type representing an uncertified, approximate preview value.
 */
template <typename T>
class ApproximatePreviewResult {
public:
    ApproximatePreviewResult(T val, CalculationClassification meta, std::string preview_reason)
        : value_(std::move(val)), metadata_(std::move(meta)), preview_reason_(std::move(preview_reason)) {}

    [[nodiscard]] const T& value() const noexcept { return value_; }
    [[nodiscard]] const CalculationClassification& metadata() const noexcept { return metadata_; }
    [[nodiscard]] const std::string& preview_reason() const noexcept { return preview_reason_; }
    [[nodiscard]] static constexpr bool is_certified() noexcept { return false; }

    [[nodiscard]] UiScientificPresentation presentation(std::string_view unit = "") const {
        UiScientificPresentation ui;
        ui.badge = ScientificFidelityBadge::ApproximatePreview;
        ui.badge_text = "[APPROXIMATE PREVIEW]";
        ui.css_class = "badge-approximate-preview";
        std::ostringstream oss;
        oss << value_;
        if (!unit.empty()) oss << " " << unit;
        oss << " (approx +/- " << std::fixed << std::setprecision(1) << metadata_.relative_uncertainty_pct << "%)";
        ui.formatted_value = oss.str();
        ui.tooltip_citation = "PREVIEW: " + preview_reason_ + " [" + metadata_.format_citation() + "]";
        ui.requires_disclaimer = true;
        return ui;
    }

private:
    T value_;
    CalculationClassification metadata_;
    std::string preview_reason_;
};

/**
 * @brief Promotion engine: strictly validates criteria before converting
 *        an ApproximatePreviewResult into a CertifiedScientificResult.
 */
class ScientificCertificationEngine {
public:
    [[nodiscard]] static UiScientificPresentation format_presentation(
        const EpistemicScalar& scalar, std::string_view unit = "");

    template <typename T>
    [[nodiscard]] static shunya::core::base::Result<CertifiedScientificResult<T>> promote_to_certified(
        const ApproximatePreviewResult<T>& preview,
        std::string_view standard_citation,
        double verified_uncertainty_pct) {
        if (standard_citation.empty()) {
            return shunya::core::base::Result<CertifiedScientificResult<T>>::err(
                shunya::core::base::Error{1, shunya::core::base::ErrorDomain::Materials, shunya::core::base::Severity::Error,
                    "Certification rejected: authoritative standard citation (e.g. ASTM/ISO/NIST) is required.", ""});
        }
        if (verified_uncertainty_pct > 5.0) {
            return shunya::core::base::Result<CertifiedScientificResult<T>>::err(
                shunya::core::base::Error{2, shunya::core::base::ErrorDomain::Materials, shunya::core::base::Severity::Error,
                    "Certification rejected: uncertainty (" + std::to_string(verified_uncertainty_pct) +
                    "%) exceeds certified threshold of 5.0%.", ""});
        }

        CalculationClassification cert_meta = preview.metadata();
        cert_meta.reference_standard = std::string(standard_citation);
        cert_meta.relative_uncertainty_pct = verified_uncertainty_pct;
        cert_meta.is_empirically_validated = true;
        // Upgrade calculation tier if it was heuristic rule-of-mixtures
        if (cert_meta.tier == CalculationTier::RuleOfMixturesApproximation) {
            cert_meta.tier = CalculationTier::EmpiricalFit;
        }

        return shunya::core::base::Result<CertifiedScientificResult<T>>::ok(
            CertifiedScientificResult<T>(preview.value(), std::move(cert_meta)));
    }
};

} // namespace shunya::mat
