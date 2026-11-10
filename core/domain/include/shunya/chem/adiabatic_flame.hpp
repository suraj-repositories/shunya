#pragma once

#include "shunya/chem/flammability_limits.hpp"
#include "shunya/core/base/result.hpp"

namespace shunya::chem {

struct AdiabaticFlameResult {
    double adiabatic_flame_temp_k{298.15};   // Constant-pressure enthalpy-balanced T_ad
    double constant_volume_temp_k{298.15};   // Constant-volume internal-energy T_v
    double moles_reactants{0.0};
    double moles_products{0.0};
};

/**
 * @brief Calculates constant-pressure adiabatic flame temperature H_prod(T_ad) = H_react(T_0) using Shomate data.
 */
class AdiabaticFlameCalculator {
public:
    [[nodiscard]] static core::base::Result<AdiabaticFlameResult> compute_fuel_air_flame(
        FuelKind fuel,
        double equivalence_ratio_phi = 1.0,
        double initial_temp_k = 298.15,
        bool include_high_t_dissociation = true);
};

} // namespace shunya::chem
