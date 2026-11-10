#pragma once

#include "shunya/core/base/result.hpp"

namespace shunya::env {

struct PsychrometricState {
    double dry_bulb_temp_k{293.15};
    double relative_humidity_pct{50.0};
    double pressure_pa{101325.0};
    double saturation_vapor_pressure_pa{2338.8};
    double vapor_pressure_pa{1169.4};
    double dew_point_k{282.43};
    double wet_bulb_k{286.85};
    double humidity_ratio_kg_kg{0.00726};
    double specific_gas_constant_moist{288.3};
    double moist_air_density_kg_m3{1.199};
};

/**
 * @brief Psychrometric moist-air thermodynamics using Magnus-Tetens saturation vapor pressure.
 */
class PsychrometricModel {
public:
    static constexpr double kDryAirGasR = 287.058;
    static constexpr double kWaterVaporGasR = 461.495;
    static constexpr double kMolecularWeightRatio = kDryAirGasR / kWaterVaporGasR; // 0.621995

    [[nodiscard]] static double saturation_vapor_pressure_pa(double temp_k) noexcept;

    [[nodiscard]] static core::base::Result<double> dew_point_k(
        double dry_bulb_temp_k,
        double relative_humidity_pct);

    [[nodiscard]] static core::base::Result<double> wet_bulb_k(
        double dry_bulb_temp_k,
        double relative_humidity_pct,
        double pressure_pa = 101325.0);

    [[nodiscard]] static core::base::Result<PsychrometricState> evaluate(
        double dry_bulb_temp_k,
        double relative_humidity_pct,
        double pressure_pa = 101325.0);
};

} // namespace shunya::env
