#include "shunya/mat/porous_material.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

namespace {

core::base::Error make_porous_error(std::string msg) {
    return core::base::Error{
        .code = 4240,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

core::base::Result<EffectivePorousProperties> PorousMaterialModel::compute(const PorousSolidInput& in) {
    if (in.relative_density <= 0.0 || in.relative_density > 1.0) {
        return make_porous_error("Relative density must lie in (0.0, 1.0]");
    }
    if (in.solid_density_kg_m3 <= 0.0 || in.solid_youngs_gpa <= 0.0 || in.solid_yield_mpa <= 0.0) {
        return make_porous_error("Solid matrix properties must be strictly positive");
    }

    const double r = in.relative_density;
    EffectivePorousProperties out;
    out.effective_density_kg_m3 = in.solid_density_kg_m3 * r;
    out.porosity = 1.0 - r;
    out.densification_strain = std::max(0.05, 1.0 - 1.4 * r);

    if (in.morphology == CellularMorphology::OpenCell) {
        out.effective_youngs_gpa = in.solid_youngs_gpa * in.constants.c1_modulus * (r * r);
        out.plateau_yield_mpa = in.solid_yield_mpa * in.constants.c2_yield * std::pow(r, 1.5);
    } else {
        const double phi = std::clamp(in.constants.solid_strut_fraction, 0.0, 1.0);
        const double strut_e = in.constants.c1_modulus * (phi * r) * (phi * r);
        const double face_e = in.constants.c1_membrane * (1.0 - phi) * r;
        out.effective_youngs_gpa = in.solid_youngs_gpa * (strut_e + face_e);

        const double strut_y = in.constants.c2_yield * std::pow(phi * r, 1.5);
        const double face_y = (1.0 - phi) * r;
        out.plateau_yield_mpa = in.solid_yield_mpa * (strut_y + face_y);
    }

    // Porous conductivity: solid conduction path scaled by power law + gas pore conduction
    out.effective_thermal_k_w_mk =
        in.solid_thermal_k_w_mk * std::pow(r, 1.65) + in.pore_fluid_k_w_mk * (1.0 - r);
    return out;
}

double PorousMaterialModel::compressive_stress_mpa(const PorousSolidInput& in,
                                                   const EffectivePorousProperties& eff,
                                                   double compressive_strain) noexcept {
    if (compressive_strain <= 0.0) return 0.0;
    const double e_eff_mpa = eff.effective_youngs_gpa * 1000.0;
    const double eps_yield = eff.plateau_yield_mpa / std::max(1.0, e_eff_mpa);

    if (compressive_strain <= eps_yield) {
        return e_eff_mpa * compressive_strain;
    }
    if (compressive_strain < eff.densification_strain) {
        // Gentle positive work-hardening slope in plastic collapse plateau
        const double span = eff.densification_strain - eps_yield;
        const double progress = (span > 1e-6) ? ((compressive_strain - eps_yield) / span) : 0.0;
        return eff.plateau_yield_mpa * (1.0 + 0.08 * progress);
    }
    // Rapid densification locking approaching solid stiffness
    const double over = compressive_strain - eff.densification_strain;
    return eff.plateau_yield_mpa * 1.08 + in.solid_youngs_gpa * 100.0 * over * over;
}

} // namespace shunya::mat
