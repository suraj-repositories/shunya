#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <vector>

namespace shunya::integ {

struct FuseElementSpec {
    double cold_resistance_ohm{0.02};
    double temp_coeff_alpha_per_k{0.00393}; // Copper TCR
    double ref_temp_k{293.15};
    double melting_temp_k{1357.77};         // Copper melt (1084.6 C) or SnPb 505 K
    double thermal_mass_j_k{0.25};
    double ambient_h_area_w_k{0.02};
    double ambient_temp_k{293.15};
};

struct ElectroThermalStepResult {
    double time_s{0.0};
    double resistance_ohm{0.02};
    double joule_power_w{0.0};
    double temperature_k{293.15};
    double integral_i2t_a2s{0.0};
    bool fuse_melted{false};
};

/**
 * @brief Bidirectional electro-thermal coupling connector (I^2*R(T) heating <-> rho(T) & fuse blowout).
 */
class ElectroThermalConnector {
public:
    explicit ElectroThermalConnector(FuseElementSpec spec = {});

    [[nodiscard]] core::base::Result<ElectroThermalStepResult> step_current(
        double applied_current_a,
        double dt_s,
        int coupling_iterations = 2);

    [[nodiscard]] core::base::Result<ElectroThermalStepResult> step_voltage(
        double supply_voltage_v,
        double series_load_ohm,
        double dt_s);

    void inject_heat_to_thermal_node(
        phy::ThermalSolver3D& solver,
        size_t ix, size_t iy, size_t iz,
        double element_volume_m3);

    [[nodiscard]] double current_resistance_ohm() const noexcept;
    [[nodiscard]] double temperature_k() const noexcept { return temp_k_; }
    [[nodiscard]] double integral_i2t() const noexcept { return i2t_accum_; }
    [[nodiscard]] bool is_melted() const noexcept { return melted_; }

private:
    FuseElementSpec spec_;
    double time_s_{0.0};
    double temp_k_{293.15};
    double last_power_w_{0.0};
    double i2t_accum_{0.0};
    bool melted_{false};
};

} // namespace shunya::integ
