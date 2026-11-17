#include "shunya/mat/scientific_certification.hpp"

#include <iomanip>
#include <sstream>

namespace shunya::mat {

UiScientificPresentation ScientificCertificationEngine::format_presentation(
    const EpistemicScalar& scalar, std::string_view unit) {
    UiScientificPresentation ui;
    const bool certified = scalar.is_scientifically_certified();
    ui.badge = certified ? ScientificFidelityBadge::CertifiedScientific
                         : ScientificFidelityBadge::ApproximatePreview;
    ui.badge_text = certified ? "[CERTIFIED SCIENTIFIC]" : "[APPROXIMATE PREVIEW]";
    ui.css_class = certified ? "badge-certified-scientific" : "badge-approximate-preview";

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << scalar.value;
    if (!unit.empty()) {
        oss << " " << unit;
    }
    oss << " +/- " << std::fixed << std::setprecision(1) << scalar.classification.relative_uncertainty_pct << "%";
    ui.formatted_value = oss.str();
    ui.tooltip_citation = scalar.classification.format_citation();
    ui.requires_disclaimer = !certified;
    return ui;
}

} // namespace shunya::mat
