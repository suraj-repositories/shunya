#pragma once

#include "shunya/chem/adiabatic_flame.hpp"
#include "shunya/core/base/result.hpp"

namespace shunya::chem {

struct DeflagrationResult {
    double peak_pressure_bar{1.0};
    double pressure_ratio{1.0};
    double flame_temp_cv_k{298.15};
    double adiabatic_flame_cp_k{298.15};
};

/**
 * @brief Computes closed-vessel constant-volume explosion / deflagration pressure rise P_max.
 */
class ExplosionPressureCalculator {
public:
    [[nodiscard]] static core::base::Result<DeflagrationResult> compute_closed_vessel_explosion(
        FuelKind fuel,
        double equivalence_ratio_phi = 1.0,
        double initial_temp_k = 298.15,
        double initial_pressure_bar = 1.01325) {
        auto flame_res = AdiabaticFlameCalculator::compute_fuel_air_flame(
            fuel, equivalence_ratio_phi, initial_temp_k, true);
        if (!flame_res.is_ok()) {
            return core::base::Result<DeflagrationResult>::err(flame_res.error());
        }
        const auto& f = flame_res.value();
        const double n_ratio = (f.moles_reactants > 0.0) ? (f.moles_products / f.moles_reactants) : 1.0;
        const double p_ratio = n_ratio * (f.constant_volume_temp_k / initial_temp_k);

        DeflagrationResult out;
        out.pressure_ratio = p_ratio;
        out.peak_pressure_bar = initial_pressure_bar * p_ratio;
        out.flame_temp_cv_k = f.constant_volume_temp_k;
        out.adiabatic_flame_cp_k = f.adiabatic_flame_temp_k;
        return core::base::Result<DeflagrationResult>::ok(out);
    }
};

} // namespace shunya::chem
