#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <vector>

namespace shunya::env {

struct ChamberProfileSegment {
    double duration_s{60.0};
    double target_temp_k{298.15};
    double target_pressure_pa{101325.0};
    double max_ramp_rate_k_per_s{5.0};
};

struct ChamberDeviceUnderTest {
    double temperature_k{298.15};
    double thermal_mass_j_k{50.0};
    double surface_area_m2{0.02};
    double emissivity{0.85};
    double internal_power_w{0.0};
};

struct ChamberStepSample {
    double time_s{0.0};
    double chamber_temp_k{298.15};
    double chamber_pressure_pa{101325.0};
    double dut_temp_k{298.15};
    double convective_w{0.0};
    double radiative_w{0.0};
};

/**
 * @brief Hermetic environmental climatic/vacuum test chamber (-40 C to +125 C, down to 1e-4 Pa) with MIL-STD-810 cycling.
 */
class ClimaticChamberModel {
public:
    static constexpr double kMinChamberTempK = 233.15;    // -40 C
    static constexpr double kMaxChamberTempK = 398.15;    // +125 C
    static constexpr double kMinVacuumPressurePa = 1e-4;  // High vacuum floor

    ClimaticChamberModel(
        double initial_temp_k = 298.15,
        double initial_pressure_pa = 101325.0,
        double nominal_h_1atm_w_m2_k = 25.0);

    [[nodiscard]] core::base::Result<void> set_setpoint(
        double temp_k,
        double pressure_pa,
        double ramp_rate_k_per_s = 10.0);

    [[nodiscard]] core::base::Result<void> add_profile_segment(const ChamberProfileSegment& seg);
    void clear_profile() noexcept;

    [[nodiscard]] core::base::Result<ChamberStepSample> step(
        double dt_s,
        ChamberDeviceUnderTest& dut);

    [[nodiscard]] core::base::Result<std::vector<ChamberStepSample>> run_profile(
        double dt_s,
        ChamberDeviceUnderTest& dut);

    void apply_boundary_to_solver(phy::ThermalSolver3D& solver) const;

    [[nodiscard]] double chamber_temp_k() const noexcept { return chamber_temp_k_; }
    [[nodiscard]] double chamber_pressure_pa() const noexcept { return chamber_pressure_pa_; }
    [[nodiscard]] double effective_h_w_m2_k() const noexcept;

private:
    double time_s_{0.0};
    double chamber_temp_k_{298.15};
    double target_temp_k_{298.15};
    double chamber_pressure_pa_{101325.0};
    double target_pressure_pa_{101325.0};
    double ramp_rate_k_per_s_{10.0};
    double nominal_h_1atm_{25.0};
    std::vector<ChamberProfileSegment> segments_;
    size_t active_seg_idx_{0};
    double seg_elapsed_s_{0.0};
};

} // namespace shunya::env
