#include "shunya/mat/material_boundary_checker.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

std::string BoundaryCheckReport::format_report() const {
    std::ostringstream oss;
    oss << "=== Material Operating Boundary Report ===\n";
    oss << "Status: " << (is_operating_safe ? "COMPLIANT" : "NON-COMPLIANT")
        << " (Max Severity: " << boundary_hazard_to_string(max_severity) << ")\n";
    if (warnings.empty()) {
        oss << "All operating conditions are within safe physical limits.\n";
    } else {
        oss << "Diagnostics (" << warnings.size() << " generated):\n";
        for (const auto& w : warnings) {
            oss << "  [" << boundary_hazard_to_string(w.severity) << "][" << w.domain << "] ("
                << w.code << "): " << w.message;
            if (w.safety_margin > 0.0 && w.safety_margin < 100.0) {
                oss << " [SF: " << std::fixed << std::setprecision(2) << w.safety_margin << "]";
            }
            oss << "\n";
        }
    }
    return oss.str();
}

BoundaryCheckReport MaterialBoundaryChecker::check_boundaries(
    const MaterialPropertyBundle& material,
    const OperatingConditions& conditions) noexcept {
    BoundaryCheckReport report;

    auto record_warning = [&](BoundaryHazardLevel sev, std::string_view domain,
                              std::string_view code, std::string_view msg, double sf = 0.0) {
        if (sev > report.max_severity) {
            report.max_severity = sev;
        }
        if (sev == BoundaryHazardLevel::CriticalFailure) {
            report.is_operating_safe = false;
        }
        report.warnings.push_back(BoundaryWarning{sev, std::string(domain), std::string(code), std::string(msg), sf});
    };

    // 1. Thermal Boundary Checks
    const double t_op = conditions.operating_temperature_k;
    const double t_melt = material.thermal.melting_point_k;
    if (t_melt > 0.0) {
        if (t_op >= t_melt) {
            record_warning(
                BoundaryHazardLevel::CriticalFailure, "Thermal", "THERMAL-MELTING",
                "Operating temperature (" + std::to_string(t_op) + " K) exceeds melting point (" +
                    std::to_string(t_melt) + " K). Phase transition to liquid loses structural load capacity.");
        } else if (t_op >= 0.40 * t_melt) {
            const double homologous_t = t_op / t_melt;
            record_warning(
                BoundaryHazardLevel::Warning, "Thermal", "THERMAL-CREEP",
                "Homologous temperature (" + std::to_string(homologous_t) + " Tm) exceeds 0.4 Tm. High risk of time-dependent creep deformation.",
                1.0 / homologous_t);
        }
    }

    if (t_op < 100.0) {
        record_warning(
            BoundaryHazardLevel::Caution, "Thermal", "THERMAL-CRYO",
            "Operating temperature is in cryogenic regime (<100 K). Evaluate potential ductile-to-brittle transition (DBTT).");
    }

    // 2. Mechanical Boundary Checks (von Mises yield & ultimate strength)
    const double sigma_vm = conditions.von_mises_stress_pa;
    const double uts = material.mechanical.ultimate_tensile_strength_pa;
    const double yield = material.mechanical.yield_strength_pa;

    if (sigma_vm > 0.0) {
        if (uts > 0.0 && sigma_vm >= uts) {
            const double sf_uts = uts / sigma_vm;
            record_warning(
                BoundaryHazardLevel::CriticalFailure, "Mechanical", "STRESS-RUPTURE",
                "Applied von Mises stress (" + std::to_string(sigma_vm / 1e6) + " MPa) exceeds ultimate tensile strength (" +
                    std::to_string(uts / 1e6) + " MPa). Tensile rupture / necking failure imminent.",
                sf_uts);
        } else if (yield > 0.0 && sigma_vm >= yield) {
            const double sf_yield = yield / sigma_vm;
            record_warning(
                BoundaryHazardLevel::Warning, "Mechanical", "STRESS-PLASTIC-YIELD",
                "Applied von Mises stress (" + std::to_string(sigma_vm / 1e6) + " MPa) exceeds yield strength (" +
                    std::to_string(yield / 1e6) + " MPa). Permanent plastic deformation occurs.",
                sf_yield);
        } else if (yield > 0.0) {
            const double sf = yield / sigma_vm;
            if (sf < 1.5) {
                record_warning(
                    BoundaryHazardLevel::Caution, "Mechanical", "LOW-SAFETY-MARGIN",
                    "Safety factor (" + std::to_string(sf) + ") is below standard aerospace/engineering margin of 1.5.",
                    sf);
            }
        }
    }

    // 3. Electrical Dielectric Breakdown Checks
    const double e_field = conditions.applied_electric_field_v_m;
    const double e_break = material.electrical.breakdown_field_v_m;
    if (e_field > 0.0 && e_break > 0.0) {
        if (e_field >= e_break) {
            const double sf_elec = e_break / e_field;
            record_warning(
                BoundaryHazardLevel::CriticalFailure, "Electrical", "DIELECTRIC-BREAKDOWN",
                "Applied electric field (" + std::to_string(e_field) + " V/m) exceeds dielectric breakdown strength (" +
                    std::to_string(e_break) + " V/m). Catastrophic dielectric puncture occurs.",
                sf_elec);
        } else {
            const double sf_elec = e_break / e_field;
            if (sf_elec < 1.3) {
                record_warning(
                    BoundaryHazardLevel::Caution, "Electrical", "HIGH-ELECTRIC-FIELD",
                    "Electric field safety margin (" + std::to_string(sf_elec) + ") is near dielectric breakdown limit.",
                    sf_elec);
            }
        }
    }

    return report;
}

} // namespace shunya::mat
