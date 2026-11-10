#include "shunya/chem/battery_cell_model.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

namespace {
constexpr double kGasConstantR = 8.31446261815324;
constexpr double kRefTempK = 298.15;
constexpr double kRunawayOnsetK = 393.15; // 120 C SEI exothermic decomposition threshold
} // namespace

BatteryCellRuntime::BatteryCellRuntime(BatteryCellParams params, double initial_soc)
    : params_(params),
      soc_(std::clamp(initial_soc, 0.0, 1.0)),
      temp_k_(std::max(100.0, params.ambient_temp_k)) {}

double BatteryCellRuntime::internal_r0_ohm() const noexcept {
    const double t_safe = std::max(150.0, temp_k_);
    const double exponent = (params_.r_arrhenius_ea_j_mol / kGasConstantR) *
                            ((1.0 / t_safe) - (1.0 / kRefTempK));
    return params_.r0_25c_ohm * std::exp(std::clamp(exponent, -10.0, 10.0));
}

core::base::Result<BatteryStepResult> BatteryCellRuntime::step_constant_current(
    double discharge_current_a,
    double dt_s) {
    if (dt_s <= 0.0 || !std::isfinite(dt_s)) {
        return core::base::Error{
            201,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Battery time step dt_s must be strictly positive",
            ""
        };
    }
    if (!std::isfinite(discharge_current_a)) {
        return core::base::Error{
            202,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Discharge current must be finite",
            ""
        };
    }
    if (params_.nominal_capacity_ah <= 0.0 || params_.thermal_mass_j_k <= 0.0) {
        return core::base::Error{
            203,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Battery capacity and thermal mass must be positive",
            ""
        };
    }

    const double q_cap_c = params_.nominal_capacity_ah * 3600.0;
    soc_ = std::clamp(soc_ - (discharge_current_a * dt_s) / q_cap_c, 0.0, 1.0);

    const double tau1 = std::max(1e-6, params_.r1_ohm * params_.c1_farad);
    const double tau2 = std::max(1e-6, params_.r2_ohm * params_.c2_farad);
    const double alpha1 = std::exp(-dt_s / tau1);
    const double alpha2 = std::exp(-dt_s / tau2);

    v1_rc_ = v1_rc_ * alpha1 + (discharge_current_a * params_.r1_ohm) * (1.0 - alpha1);
    v2_rc_ = v2_rc_ * alpha2 + (discharge_current_a * params_.r2_ohm) * (1.0 - alpha2);

    const double r0 = internal_r0_ohm();
    const double ocv_v = LiIonSocTables::lookup_ocv_v(soc_);
    const double terminal_v = std::max(0.0, ocv_v - discharge_current_a * r0 - v1_rc_ - v2_rc_);

    const double q_ohmic = discharge_current_a * discharge_current_a * r0;
    const double q_rc1 = (params_.r1_ohm > 0.0) ? (v1_rc_ * v1_rc_) / params_.r1_ohm : 0.0;
    const double q_rc2 = (params_.r2_ohm > 0.0) ? (v2_rc_ * v2_rc_) / params_.r2_ohm : 0.0;
    double q_total = q_ohmic + q_rc1 + q_rc2;

    if (params_.enable_runaway_heating && temp_k_ >= kRunawayOnsetK) {
        const double delta_t = std::min(temp_k_ - kRunawayOnsetK, 80.0);
        q_total += 150.0 * std::exp(0.045 * delta_t);
    }

    const double q_cool = params_.cooling_h_area_w_k * (temp_k_ - params_.ambient_temp_k);
    temp_k_ = std::max(100.0, temp_k_ + ((q_total - q_cool) / params_.thermal_mass_j_k) * dt_s);
    last_heat_w_ = q_total;
    time_s_ += dt_s;

    BatteryStepResult res;
    res.time_s = time_s_;
    res.soc = soc_;
    res.ocv_v = ocv_v;
    res.terminal_voltage_v = terminal_v;
    res.current_a = discharge_current_a;
    res.temperature_k = temp_k_;
    res.heat_generation_w = q_total;
    return res;
}

core::base::Result<BatteryStepResult> BatteryCellRuntime::step_resistive_load(
    double external_load_ohm,
    double dt_s) {
    if (external_load_ohm <= 0.0 || !std::isfinite(external_load_ohm)) {
        return core::base::Error{
            204,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "External load resistance must be strictly positive",
            ""
        };
    }
    const double ocv_v = LiIonSocTables::lookup_ocv_v(soc_);
    const double r0 = internal_r0_ohm();
    const double effective_v = std::max(0.0, ocv_v - v1_rc_ - v2_rc_);
    const double current_a = effective_v / (r0 + external_load_ohm);
    return step_constant_current(current_a, dt_s);
}

core::base::Result<std::vector<BatteryStepResult>> BatteryCellRuntime::simulate_cc_discharge(
    double c_rate,
    double cutoff_voltage_v,
    double dt_s) {
    if (c_rate <= 0.0 || !std::isfinite(c_rate)) {
        return core::base::Error{
            205,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Discharge C-rate must be strictly positive",
            ""
        };
    }
    if (dt_s <= 0.0 || !std::isfinite(dt_s)) {
        return core::base::Error{
            206,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Simulation step dt_s must be strictly positive",
            ""
        };
    }

    const double discharge_a = c_rate * params_.nominal_capacity_ah;
    std::vector<BatteryStepResult> trajectory;
    constexpr size_t kMaxSteps = 200000;
    for (size_t step = 0; step < kMaxSteps; ++step) {
        auto res = step_constant_current(discharge_a, dt_s);
        if (!res.is_ok()) {
            return res.error();
        }
        const BatteryStepResult& r = res.value();
        trajectory.push_back(r);
        if (r.terminal_voltage_v <= cutoff_voltage_v || r.soc <= 0.0) {
            break;
        }
    }
    return trajectory;
}

void BatteryCellRuntime::inject_heat_to_thermal_cell(
    phy::ThermalSolver3D& solver,
    size_t ix, size_t iy, size_t iz,
    double cell_volume_m3) const {
    if (cell_volume_m3 <= 0.0) {
        return;
    }
    solver.set_volumetric_source(ix, iy, iz, last_heat_w_ / cell_volume_m3);
}

} // namespace shunya::chem
