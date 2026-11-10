#include "shunya/env/atmospheric_model.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

core::base::Result<AtmosphericState> AtmosphericModel::evaluate_geopotential(double geopotential_alt_m) {
    if (!std::isfinite(geopotential_alt_m) ||
        geopotential_alt_m < -5000.0 ||
        geopotential_alt_m > StandardAtmosphere1976::kMaxGeopotentialM) {
        return core::base::Error{
            301,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Geopotential altitude out of US Standard Atmosphere 1976 bounds [-5000, 84852] m",
            ""
        };
    }

    int layer_idx = 0;
    const auto& layers = StandardAtmosphere1976::kLayers;
    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        if (geopotential_alt_m >= layers[static_cast<size_t>(i)].base_geopotential_m) {
            layer_idx = i;
            break;
        }
    }

    const AtmosphereLayerSpec& base = layers[static_cast<size_t>(layer_idx)];
    const double dh = geopotential_alt_m - base.base_geopotential_m;
    const double temp_k = base.base_temperature_k + base.lapse_rate_k_per_m * dh;

    double pressure_pa = base.base_pressure_pa;
    constexpr double g0_over_r = StandardAtmosphere1976::kStandardGravity /
                                 StandardAtmosphere1976::kSpecificGasAirR;

    if (std::abs(base.lapse_rate_k_per_m) < 1e-12) {
        pressure_pa = base.base_pressure_pa *
                      std::exp(-g0_over_r * dh / base.base_temperature_k);
    } else {
        const double exponent = -g0_over_r / base.lapse_rate_k_per_m;
        pressure_pa = base.base_pressure_pa *
                      std::pow(temp_k / base.base_temperature_k, exponent);
    }

    const double rho = pressure_pa / (StandardAtmosphere1976::kSpecificGasAirR * temp_k);
    const double t_c = temp_k - 273.15;
    const double k_air = 0.0241 + 7.5e-5 * t_c;

    AtmosphericState state;
    state.geopotential_alt_m = geopotential_alt_m;
    state.geometric_alt_m = StandardAtmosphere1976::geopotential_to_geometric(geopotential_alt_m);
    state.temperature_k = temp_k;
    state.pressure_pa = pressure_pa;
    state.density_kg_m3 = rho;
    state.speed_of_sound_m_s = speed_of_sound(temp_k);
    state.dynamic_viscosity_pa_s = sutherland_viscosity(temp_k);
    state.thermal_conductivity_w_m_k = std::max(0.005, k_air);
    state.layer_index = layer_idx;
    return state;
}

core::base::Result<AtmosphericState> AtmosphericModel::evaluate_geometric(double geometric_alt_m) {
    if (!std::isfinite(geometric_alt_m) || geometric_alt_m < -5000.0 || geometric_alt_m > 86000.0) {
        return core::base::Error{
            302,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Geometric altitude out of US Standard Atmosphere 1976 bounds [-5000, 86000] m",
            ""
        };
    }
    const double h_m = StandardAtmosphere1976::geometric_to_geopotential(geometric_alt_m);
    auto res = evaluate_geopotential(h_m);
    if (res.is_ok()) {
        res.value().geometric_alt_m = geometric_alt_m;
    }
    return res;
}

double AtmosphericModel::sutherland_viscosity(double temp_k) noexcept {
    const double t = std::max(50.0, temp_k);
    return (1.458e-6 * t * std::sqrt(t)) / (t + 110.4);
}

double AtmosphericModel::speed_of_sound(double temp_k) noexcept {
    const double t = std::max(50.0, temp_k);
    return std::sqrt(StandardAtmosphere1976::kGammaAir *
                     StandardAtmosphere1976::kSpecificGasAirR * t);
}

} // namespace shunya::env
