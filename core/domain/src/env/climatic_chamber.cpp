#include "shunya/env/climatic_chamber.hpp"

#include "shunya/core/domain/thermal.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

ClimaticChamberModel::ClimaticChamberModel(
    double initial_temp_k,
    double initial_pressure_pa,
    double nominal_h_1atm_w_m2_k)
    : chamber_temp_k_(std::clamp(initial_temp_k, kMinChamberTempK, kMaxChamberTempK)),
      target_temp_k_(chamber_temp_k_),
      chamber_pressure_pa_(std::max(kMinVacuumPressurePa, initial_pressure_pa)),
      target_pressure_pa_(chamber_pressure_pa_),
      nominal_h_1atm_(std::max(0.0, nominal_h_1atm_w_m2_k)) {}

core::base::Result<void> ClimaticChamberModel::set_setpoint(
    double temp_k,
    double pressure_pa,
    double ramp_rate_k_per_s) {
    if (!std::isfinite(temp_k) || temp_k < kMinChamberTempK || temp_k > kMaxChamberTempK) {
        return core::base::Error{
            331, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Chamber temperature setpoint must be within [-40 C, +125 C] (233.15 - 398.15 K)", ""
        };
    }
    if (!std::isfinite(pressure_pa) || pressure_pa < kMinVacuumPressurePa) {
        return core::base::Error{
            332, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Chamber pressure must be >= 1e-4 Pa", ""
        };
    }
    if (ramp_rate_k_per_s <= 0.0 || !std::isfinite(ramp_rate_k_per_s)) {
        return core::base::Error{
            333, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Chamber temperature ramp rate must be positive", ""
        };
    }
    target_temp_k_ = temp_k;
    target_pressure_pa_ = pressure_pa;
    ramp_rate_k_per_s_ = ramp_rate_k_per_s;
    return core::base::Result<void>::ok();
}

core::base::Result<void> ClimaticChamberModel::add_profile_segment(const ChamberProfileSegment& seg) {
    if (seg.duration_s <= 0.0 ||
        seg.target_temp_k < kMinChamberTempK || seg.target_temp_k > kMaxChamberTempK ||
        seg.target_pressure_pa < kMinVacuumPressurePa ||
        seg.max_ramp_rate_k_per_s <= 0.0) {
        return core::base::Error{
            334, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid chamber thermal profile segment parameters", ""
        };
    }
    segments_.push_back(seg);
    return core::base::Result<void>::ok();
}

void ClimaticChamberModel::clear_profile() noexcept {
    segments_.clear();
    active_seg_idx_ = 0;
    seg_elapsed_s_ = 0.0;
}

double ClimaticChamberModel::effective_h_w_m2_k() const noexcept {
    // Rarefied gas convection scales down smoothly toward 0 under high vacuum
    const double p_ratio = std::clamp(chamber_pressure_pa_ / 101325.0, 0.0, 2.0);
    return nominal_h_1atm_ * std::sqrt(p_ratio);
}

core::base::Result<ChamberStepSample> ClimaticChamberModel::step(
    double dt_s,
    ChamberDeviceUnderTest& dut) {
    if (dt_s <= 0.0 || !std::isfinite(dt_s) ||
        dut.thermal_mass_j_k <= 0.0 || dut.surface_area_m2 < 0.0) {
        return core::base::Error{
            335, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid climatic chamber step or DUT thermal parameters", ""
        };
    }

    if (active_seg_idx_ < segments_.size()) {
        const auto& seg = segments_[active_seg_idx_];
        target_temp_k_ = seg.target_temp_k;
        target_pressure_pa_ = seg.target_pressure_pa;
        ramp_rate_k_per_s_ = seg.max_ramp_rate_k_per_s;

        seg_elapsed_s_ += dt_s;
        if (seg_elapsed_s_ >= seg.duration_s && active_seg_idx_ + 1 < segments_.size()) {
            seg_elapsed_s_ -= seg.duration_s;
            ++active_seg_idx_;
        }
    }

    const double max_dt = ramp_rate_k_per_s_ * dt_s;
    const double diff_t = target_temp_k_ - chamber_temp_k_;
    if (std::abs(diff_t) <= max_dt) {
        chamber_temp_k_ = target_temp_k_;
    } else {
        chamber_temp_k_ += (diff_t > 0.0 ? max_dt : -max_dt);
    }

    // Fast pump-down / repressurization relaxation (tau_p = 2 s)
    const double alpha_p = 1.0 - std::exp(-dt_s / 2.0);
    chamber_pressure_pa_ += alpha_p * (target_pressure_pa_ - chamber_pressure_pa_);

    const double h_eff = effective_h_w_m2_k();
    const double q_conv = h_eff * dut.surface_area_m2 * (chamber_temp_k_ - dut.temperature_k);
    const double t_c4 = std::pow(chamber_temp_k_, 4);
    const double t_d4 = std::pow(dut.temperature_k, 4);
    const double q_rad = dut.emissivity * core::domain::thermal::STEFAN_BOLTZMANN *
                         dut.surface_area_m2 * (t_c4 - t_d4);

    const double q_net = q_conv + q_rad + dut.internal_power_w;
    dut.temperature_k = std::max(50.0, dut.temperature_k + (q_net / dut.thermal_mass_j_k) * dt_s);
    time_s_ += dt_s;

    ChamberStepSample sample;
    sample.time_s = time_s_;
    sample.chamber_temp_k = chamber_temp_k_;
    sample.chamber_pressure_pa = chamber_pressure_pa_;
    sample.dut_temp_k = dut.temperature_k;
    sample.convective_w = q_conv;
    sample.radiative_w = q_rad;
    return sample;
}

core::base::Result<std::vector<ChamberStepSample>> ClimaticChamberModel::run_profile(
    double dt_s,
    ChamberDeviceUnderTest& dut) {
    if (segments_.empty() || dt_s <= 0.0) {
        return core::base::Error{
            336, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Chamber profile is empty or dt_s <= 0", ""
        };
    }

    double total_duration = 0.0;
    for (const auto& s : segments_) {
        total_duration += s.duration_s;
    }

    active_seg_idx_ = 0;
    seg_elapsed_s_ = 0.0;
    std::vector<ChamberStepSample> history;
    const size_t steps = static_cast<size_t>(std::ceil(total_duration / dt_s));
    history.reserve(steps);
    for (size_t i = 0; i < steps; ++i) {
        auto s = step(dt_s, dut);
        if (!s.is_ok()) {
            return s.error();
        }
        history.push_back(s.value());
    }
    return history;
}

void ClimaticChamberModel::apply_boundary_to_solver(phy::ThermalSolver3D& solver) const {
    solver.set_dirichlet_x_min_plane(chamber_temp_k_);
    solver.set_dirichlet_x_max_plane(chamber_temp_k_);
}

} // namespace shunya::env
