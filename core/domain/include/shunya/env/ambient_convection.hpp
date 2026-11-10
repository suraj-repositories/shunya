#pragma once

#include "shunya/core/base/result.hpp"

#include <vector>

namespace shunya::env {

/**
 * @brief Far-field wind-driven ambient convective boundary using McAdams correlation (h = 5.7 + 3.8 * v).
 */
class AmbientConvectionBoundary {
public:
    [[nodiscard]] static double mcadams_h_w_m2_k(double wind_speed_m_s) noexcept;

    [[nodiscard]] static double evaluate_heat_loss_w(
        double surface_temp_k,
        double ambient_temp_k,
        double surface_area_m2,
        double wind_speed_m_s) noexcept;

    [[nodiscard]] static double analytical_lumped_temp_k(
        double initial_temp_k,
        double ambient_temp_k,
        double surface_area_m2,
        double thermal_mass_j_k,
        double wind_speed_m_s,
        double time_s) noexcept;

    [[nodiscard]] static core::base::Result<std::vector<double>> simulate_lumped_cooling(
        double initial_temp_k,
        double ambient_temp_k,
        double surface_area_m2,
        double thermal_mass_j_k,
        double wind_speed_m_s,
        double total_time_s,
        double dt_s);
};

} // namespace shunya::env
