#include "shunya/env/ambient_convection.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

double AmbientConvectionBoundary::mcadams_h_w_m2_k(double wind_speed_m_s) noexcept {
    return 5.7 + 3.8 * std::max(0.0, wind_speed_m_s);
}

double AmbientConvectionBoundary::evaluate_heat_loss_w(
    double surface_temp_k,
    double ambient_temp_k,
    double surface_area_m2,
    double wind_speed_m_s) noexcept {
    const double h = mcadams_h_w_m2_k(wind_speed_m_s);
    return h * std::max(0.0, surface_area_m2) * (surface_temp_k - ambient_temp_k);
}

double AmbientConvectionBoundary::analytical_lumped_temp_k(
    double initial_temp_k,
    double ambient_temp_k,
    double surface_area_m2,
    double thermal_mass_j_k,
    double wind_speed_m_s,
    double time_s) noexcept {
    if (thermal_mass_j_k <= 0.0 || time_s <= 0.0) {
        return initial_temp_k;
    }
    const double h = mcadams_h_w_m2_k(wind_speed_m_s);
    const double tau = thermal_mass_j_k / std::max(1e-12, h * surface_area_m2);
    return ambient_temp_k + (initial_temp_k - ambient_temp_k) * std::exp(-time_s / tau);
}

core::base::Result<std::vector<double>> AmbientConvectionBoundary::simulate_lumped_cooling(
    double initial_temp_k,
    double ambient_temp_k,
    double surface_area_m2,
    double thermal_mass_j_k,
    double wind_speed_m_s,
    double total_time_s,
    double dt_s) {
    if (dt_s <= 0.0 || total_time_s <= 0.0 || thermal_mass_j_k <= 0.0 || surface_area_m2 <= 0.0) {
        return core::base::Error{
            351, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid parameters for lumped convective cooling simulation", ""
        };
    }

    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_s));
    std::vector<double> temps;
    temps.reserve(steps + 1);
    double t = initial_temp_k;
    temps.push_back(t);

    const double h = mcadams_h_w_m2_k(wind_speed_m_s);
    const double k_step = (h * surface_area_m2 * dt_s) / thermal_mass_j_k;
    // Semi-implicit Crank-Nicolson / backward Euler hybrid for unconditional stability
    for (size_t i = 0; i < steps; ++i) {
        t = (t + k_step * ambient_temp_k) / (1.0 + k_step);
        temps.push_back(t);
    }
    return temps;
}

} // namespace shunya::env
