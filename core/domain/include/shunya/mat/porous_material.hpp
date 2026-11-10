#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/gibson_ashby.hpp"

namespace shunya::mat {

struct PorousSolidInput {
    double solid_density_kg_m3{2700.0};
    double solid_youngs_gpa{70.0};
    double solid_yield_mpa{150.0};
    double solid_thermal_k_w_mk{200.0};
    double pore_fluid_k_w_mk{0.026}; // Air
    double relative_density{0.15};     // rho_eff / rho_solid in (0, 1]
    CellularMorphology morphology{CellularMorphology::OpenCell};
    GibsonAshbyConstants constants{};
};

struct EffectivePorousProperties {
    double effective_density_kg_m3{0.0};
    double porosity{0.0};
    double effective_youngs_gpa{0.0};
    double plateau_yield_mpa{0.0};
    double effective_thermal_k_w_mk{0.0};
    double densification_strain{0.79};
};

/**
 * @brief Models porous and cellular solids via Gibson-Ashby scaling laws.
 */
class PorousMaterialModel {
public:
    [[nodiscard]] static core::base::Result<EffectivePorousProperties> compute(const PorousSolidInput& in);

    /**
     * @brief Evaluates uniaxial compressive stress (MPa) across elastic, plateau, and densification regimes.
     */
    [[nodiscard]] static double compressive_stress_mpa(const PorousSolidInput& in,
                                                       const EffectivePorousProperties& eff,
                                                       double compressive_strain) noexcept;
};

} // namespace shunya::mat
