#pragma once

#include "shunya/mat/property_models/material_property_bundle.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::mat {

enum class BoundaryHazardLevel : uint8_t {
    Safe = 0,
    Caution = 1,
    Warning = 2,
    CriticalFailure = 3
};

[[nodiscard]] constexpr std::string_view boundary_hazard_to_string(BoundaryHazardLevel lvl) noexcept {
    switch (lvl) {
    case BoundaryHazardLevel::Safe:            return "SAFE";
    case BoundaryHazardLevel::Caution:         return "CAUTION";
    case BoundaryHazardLevel::Warning:         return "WARNING";
    case BoundaryHazardLevel::CriticalFailure: return "CRITICAL FAILURE";
    }
    return "UNKNOWN";
}

struct BoundaryWarning {
    BoundaryHazardLevel severity{BoundaryHazardLevel::Safe};
    std::string domain; // "Thermal", "Mechanical", "Electrical"
    std::string code;
    std::string message;
    double safety_margin{1.0}; // Safety Factor (e.g. sigma_y / sigma_applied)
};

struct BoundaryCheckReport {
    bool is_operating_safe{true};
    BoundaryHazardLevel max_severity{BoundaryHazardLevel::Safe};
    std::vector<BoundaryWarning> warnings;

    [[nodiscard]] bool has_failures() const noexcept {
        return max_severity == BoundaryHazardLevel::CriticalFailure;
    }

    [[nodiscard]] std::string format_report() const;
};

struct OperatingConditions {
    double operating_temperature_k{293.15};
    double von_mises_stress_pa{0.0};
    double applied_electric_field_v_m{0.0};
    double hydrostatic_pressure_pa{101325.0};
};

/**
 * @brief Engineering validation boundary checker:
 *        Evaluates operating thermal, mechanical, and electrical conditions against
 *        physical material capacity limits, generating structured engineering diagnostics.
 * (Roadmap task PH4-03.03.004)
 */
class MaterialBoundaryChecker {
public:
    [[nodiscard]] static BoundaryCheckReport check_boundaries(
        const MaterialPropertyBundle& material,
        const OperatingConditions& conditions) noexcept;
};

} // namespace shunya::mat
