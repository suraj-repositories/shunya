#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/env/standard_atmosphere_1976.hpp"

namespace shunya::env {

struct AtmosphericState {
    double geopotential_alt_m{0.0};
    double geometric_alt_m{0.0};
    double temperature_k{288.15};
    double pressure_pa{101325.0};
    double density_kg_m3{1.225};
    double speed_of_sound_m_s{340.294};
    double dynamic_viscosity_pa_s{1.7894e-5};
    double thermal_conductivity_w_m_k{0.0253};
    int layer_index{0};
};

/**
 * @brief US Standard Atmosphere 1976 evaluator from sea level to mesopause (0 to 86 km).
 */
class AtmosphericModel {
public:
    [[nodiscard]] static core::base::Result<AtmosphericState> evaluate_geopotential(double geopotential_alt_m);
    [[nodiscard]] static core::base::Result<AtmosphericState> evaluate_geometric(double geometric_alt_m);

    [[nodiscard]] static double sutherland_viscosity(double temp_k) noexcept;
    [[nodiscard]] static double speed_of_sound(double temp_k) noexcept;
};

} // namespace shunya::env
