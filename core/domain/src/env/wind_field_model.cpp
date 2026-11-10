#include "shunya/env/wind_field_model.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

WindFieldModel::WindFieldModel(WindFieldConfig config)
    : config_(config),
      turb_(config.enable_turbulence
                ? std::max(0.0, config.turbulence_intensity * config.reference_speed_m_s)
                : 0.0,
            config.integral_length_scale_m,
            std::max(0.5, config.reference_speed_m_s),
            config.seed) {}

double WindFieldModel::friction_velocity_u_star() const noexcept {
    const double z0 = std::max(1e-5, config_.roughness_length_z0_m);
    const double z_ref = std::max(z0 * 1.1, config_.reference_height_m);
    return (kVonKarmanKappa * std::max(0.0, config_.reference_speed_m_s)) /
           std::log(z_ref / z0);
}

double WindFieldModel::mean_speed_at_height(double height_m) const noexcept {
    const double z0 = std::max(1e-5, config_.roughness_length_z0_m);
    if (height_m <= z0) {
        return 0.0;
    }
    const double u_star = friction_velocity_u_star();
    return (u_star / kVonKarmanKappa) * std::log(height_m / z0);
}

core::base::math::Vector3<double> WindFieldModel::velocity_at(
    double height_m,
    double time_s,
    const core::base::math::Vector3<double>& pos_m) const noexcept {
    const double mean_spd = mean_speed_at_height(height_m);
    const double cx = std::cos(config_.azimuth_rad);
    const double cz = std::sin(config_.azimuth_rad);

    core::base::math::Vector3<double> v{mean_spd * cx, 0.0, mean_spd * cz};
    if (config_.enable_turbulence && height_m > config_.roughness_length_z0_m) {
        const auto gust = turb_.sample_fluctuation(time_s, pos_m);
        // Scale gust intensity near surface proportionally to local log-law speed
        const double ratio = (config_.reference_speed_m_s > 1e-6)
                                 ? std::min(1.5, mean_spd / config_.reference_speed_m_s)
                                 : 1.0;
        v.x += gust.x * ratio;
        v.y += gust.y * ratio;
        v.z += gust.z * ratio;
    }
    return v;
}

} // namespace shunya::env
