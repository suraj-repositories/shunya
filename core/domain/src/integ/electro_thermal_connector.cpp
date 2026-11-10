#include "shunya/integ/electro_thermal_connector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shunya::integ {

ElectroThermalConnector::ElectroThermalConnector(FuseElementSpec spec)
    : spec_(spec), temp_k_(spec.ref_temp_k) {}

double ElectroThermalConnector::current_resistance_ohm() const noexcept {
    if (melted_) {
        return std::numeric_limits<double>::infinity();
    }
    const double factor = 1.0 + spec_.temp_coeff_alpha_per_k * (temp_k_ - spec_.ref_temp_k);
    return std::max(1e-6, spec_.cold_resistance_ohm * factor);
}

core::base::Result<ElectroThermalStepResult> ElectroThermalConnector::step_current(
    double applied_current_a,
    double dt_s,
    int coupling_iterations) {
    if (dt_s <= 0.0 || !std::isfinite(dt_s) || !std::isfinite(applied_current_a) ||
        spec_.thermal_mass_j_k <= 0.0) {
        return core::base::Error{
            421, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid ElectroThermalConnector parameters", ""
        };
    }

    if (melted_) {
        time_s_ += dt_s;
        last_power_w_ = 0.0;
        return ElectroThermalStepResult{
            time_s_, std::numeric_limits<double>::infinity(), 0.0, temp_k_, i2t_accum_, true
        };
    }

    const double t_start = temp_k_;
    double t_end = temp_k_;
    double r_eff = current_resistance_ohm();
    double power_w = 0.0;

    // Bidirectional Gauss-Seidel electro-thermal iteration over step dt_s
    const int n_iter = std::max(1, coupling_iterations);
    for (int it = 0; it < n_iter; ++it) {
        const double t_mid = 0.5 * (t_start + t_end);
        r_eff = std::max(1e-6, spec_.cold_resistance_ohm *
                               (1.0 + spec_.temp_coeff_alpha_per_k * (t_mid - spec_.ref_temp_k)));
        power_w = applied_current_a * applied_current_a * r_eff;
        const double q_cool = spec_.ambient_h_area_w_k * (t_mid - spec_.ambient_temp_k);
        t_end = t_start + ((power_w - q_cool) / spec_.thermal_mass_j_k) * dt_s;
    }

    temp_k_ = t_end;
    last_power_w_ = power_w;
    i2t_accum_ += applied_current_a * applied_current_a * dt_s;
    time_s_ += dt_s;

    if (temp_k_ >= spec_.melting_temp_k) {
        melted_ = true;
        last_power_w_ = 0.0;
    }

    return ElectroThermalStepResult{
        time_s_,
        current_resistance_ohm(),
        last_power_w_,
        temp_k_,
        i2t_accum_,
        melted_
    };
}

core::base::Result<ElectroThermalStepResult> ElectroThermalConnector::step_voltage(
    double supply_voltage_v,
    double series_load_ohm,
    double dt_s) {
    if (series_load_ohm < 0.0 || !std::isfinite(supply_voltage_v)) {
        return core::base::Error{
            422, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid supply voltage or series load", ""
        };
    }
    const double current_a = melted_
                                 ? 0.0
                                 : supply_voltage_v / std::max(1e-9, series_load_ohm + current_resistance_ohm());
    return step_current(current_a, dt_s, 2);
}

void ElectroThermalConnector::inject_heat_to_thermal_node(
    phy::ThermalSolver3D& solver,
    size_t ix, size_t iy, size_t iz,
    double element_volume_m3) {
    if (element_volume_m3 <= 0.0) {
        return;
    }
    solver.set_volumetric_source(ix, iy, iz, last_power_w_ / element_volume_m3);
}

} // namespace shunya::integ
