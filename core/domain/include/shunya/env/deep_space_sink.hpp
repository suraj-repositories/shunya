#pragma once

#include "shunya/core/base/result.hpp"

#include <vector>

namespace shunya::env {

enum class SkySinkMode {
    ClearNightSkyBerdahlMartin,
    OrbitalDeepSpaceVacuum
};

struct RadiativeCoolingResult {
    double effective_sky_temp_k{270.0};
    double sky_emissivity{0.75};
    double initial_net_cooling_w{0.0};
    std::vector<double> temperature_history_k;
};

/**
 * @brief Clear night-sky (Berdahl-Martin) and deep-space (2.7 K) infrared radiative cooling sink.
 */
class DeepSpaceRadiationSink {
public:
    static constexpr double kDeepSpaceTempK = 2.7;

    [[nodiscard]] static double clear_sky_emissivity(double dew_point_temp_k) noexcept;

    [[nodiscard]] static double effective_sky_temp_k(
        double ambient_temp_k,
        double dew_point_temp_k,
        SkySinkMode mode = SkySinkMode::ClearNightSkyBerdahlMartin) noexcept;

    [[nodiscard]] static double net_radiative_loss_w(
        double surface_temp_k,
        double sink_temp_k,
        double surface_area_m2,
        double emissivity = 0.95) noexcept;

    [[nodiscard]] static core::base::Result<RadiativeCoolingResult> simulate_nocturnal_cooling(
        double initial_temp_k,
        double ambient_temp_k,
        double dew_point_temp_k,
        double surface_area_m2,
        double thermal_mass_j_k,
        double ir_emissivity,
        double parasitic_conv_h_w_m2_k,
        double total_time_s,
        double dt_s,
        SkySinkMode mode = SkySinkMode::ClearNightSkyBerdahlMartin);
};

} // namespace shunya::env
