#pragma once

#include "shunya/chem/li_ion_soc_tables.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <vector>

namespace shunya::chem {

struct BatteryCellParams {
    double nominal_capacity_ah{2.5}; // 2.5 Ah 18650 cell
    double r0_25c_ohm{0.045};
    double r1_ohm{0.015};
    double c1_farad{800.0};
    double r2_ohm{0.025};
    double c2_farad{4000.0};
    double r_arrhenius_ea_j_mol{15000.0};

    // Lumped thermal + optional SEI exothermic runaway onset > 393 K (120 C)
    double thermal_mass_j_k{42.0};
    double cooling_h_area_w_k{0.35};
    double ambient_temp_k{298.15};
    bool enable_runaway_heating{false};
};

struct BatteryStepResult {
    double time_s{0.0};
    double soc{1.0};
    double ocv_v{4.2};
    double terminal_voltage_v{4.2};
    double current_a{0.0};
    double temperature_k{298.15};
    double heat_generation_w{0.0};
};

/**
 * @brief Dual-RC Thevenin Li-ion battery Equivalent Circuit Model coupled to thermal Joule/runaway dynamics.
 */
class BatteryCellRuntime {
public:
    explicit BatteryCellRuntime(BatteryCellParams params = {}, double initial_soc = 1.0);

    [[nodiscard]] core::base::Result<BatteryStepResult> step_constant_current(
        double discharge_current_a,
        double dt_s);

    [[nodiscard]] core::base::Result<BatteryStepResult> step_resistive_load(
        double external_load_ohm,
        double dt_s);

    [[nodiscard]] core::base::Result<std::vector<BatteryStepResult>> simulate_cc_discharge(
        double c_rate,
        double cutoff_voltage_v = 2.8,
        double dt_s = 1.0);

    void inject_heat_to_thermal_cell(
        phy::ThermalSolver3D& solver,
        size_t ix, size_t iy, size_t iz,
        double cell_volume_m3) const;

    [[nodiscard]] double soc() const noexcept { return soc_; }
    [[nodiscard]] double temperature_k() const noexcept { return temp_k_; }
    [[nodiscard]] double internal_r0_ohm() const noexcept;
    [[nodiscard]] double last_heat_watts() const noexcept { return last_heat_w_; }

private:
    BatteryCellParams params_;
    double time_s_{0.0};
    double soc_{1.0};
    double v1_rc_{0.0};
    double v2_rc_{0.0};
    double temp_k_{298.15};
    double last_heat_w_{0.0};
};

} // namespace shunya::chem
