#include "shunya/env/deep_space_sink.hpp"

#include "shunya/core/domain/thermal.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

double DeepSpaceRadiationSink::clear_sky_emissivity(double dew_point_temp_k) noexcept {
    const double t_dp_c = dew_point_temp_k - 273.15;
    const double eps = 0.711 + 0.0056 * t_dp_c + 0.000073 * t_dp_c * t_dp_c;
    return std::clamp(eps, 0.35, 1.0);
}

double DeepSpaceRadiationSink::effective_sky_temp_k(
    double ambient_temp_k,
    double dew_point_temp_k,
    SkySinkMode mode) noexcept {
    if (mode == SkySinkMode::OrbitalDeepSpaceVacuum) {
        return kDeepSpaceTempK;
    }
    const double eps_sky = clear_sky_emissivity(dew_point_temp_k);
    return std::max(kDeepSpaceTempK, ambient_temp_k * std::pow(eps_sky, 0.25));
}

double DeepSpaceRadiationSink::net_radiative_loss_w(
    double surface_temp_k,
    double sink_temp_k,
    double surface_area_m2,
    double emissivity) noexcept {
    const double t_s4 = std::pow(std::max(0.0, surface_temp_k), 4);
    const double t_k4 = std::pow(std::max(0.0, sink_temp_k), 4);
    return std::clamp(emissivity, 0.0, 1.0) *
           core::domain::thermal::STEFAN_BOLTZMANN *
           std::max(0.0, surface_area_m2) * (t_s4 - t_k4);
}

core::base::Result<RadiativeCoolingResult> DeepSpaceRadiationSink::simulate_nocturnal_cooling(
    double initial_temp_k,
    double ambient_temp_k,
    double dew_point_temp_k,
    double surface_area_m2,
    double thermal_mass_j_k,
    double ir_emissivity,
    double parasitic_conv_h_w_m2_k,
    double total_time_s,
    double dt_s,
    SkySinkMode mode) {
    if (dt_s <= 0.0 || total_time_s <= 0.0 || thermal_mass_j_k <= 0.0 ||
        surface_area_m2 <= 0.0 || initial_temp_k <= 0.0 || ambient_temp_k <= 0.0) {
        return core::base::Error{
            361, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid parameters for nocturnal/deep-space radiative cooling simulation", ""
        };
    }

    RadiativeCoolingResult res;
    res.sky_emissivity = (mode == SkySinkMode::OrbitalDeepSpaceVacuum)
                             ? 1.0
                             : clear_sky_emissivity(dew_point_temp_k);
    res.effective_sky_temp_k = effective_sky_temp_k(ambient_temp_k, dew_point_temp_k, mode);

    const double q_rad0 = net_radiative_loss_w(
        initial_temp_k, res.effective_sky_temp_k, surface_area_m2, ir_emissivity);
    const double q_conv0 = parasitic_conv_h_w_m2_k * surface_area_m2 * (initial_temp_k - ambient_temp_k);
    res.initial_net_cooling_w = q_rad0 + q_conv0;

    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_s));
    res.temperature_history_k.reserve(steps + 1);
    double t = initial_temp_k;
    res.temperature_history_k.push_back(t);

    for (size_t i = 0; i < steps; ++i) {
        const double q_rad = net_radiative_loss_w(
            t, res.effective_sky_temp_k, surface_area_m2, ir_emissivity);
        const double q_conv = parasitic_conv_h_w_m2_k * surface_area_m2 * (t - ambient_temp_k);
        t = std::max(kDeepSpaceTempK, t - ((q_rad + q_conv) / thermal_mass_j_k) * dt_s);
        res.temperature_history_k.push_back(t);
    }
    return res;
}

} // namespace shunya::env
