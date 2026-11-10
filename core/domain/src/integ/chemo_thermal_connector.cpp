#include "shunya/integ/chemo_thermal_connector.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::integ {

ChemoThermalConnector::ChemoThermalConnector(
    CstrReactorSpec spec,
    double init_conc,
    double init_temp_k)
    : spec_(spec) {
    state_.conc_mol_m3 = std::max(0.0, init_conc);
    state_.temp_k = std::max(150.0, init_temp_k);
    state_.rate_constant_per_s = arrhenius_k(state_.temp_k);
}

double ChemoThermalConnector::arrhenius_k(double temp_k) const noexcept {
    const double t = std::max(150.0, temp_k);
    const double exponent = -spec_.activation_energy_j_mol / (kGasConstantR * t);
    return spec_.pre_exponential_a_per_s * std::exp(std::clamp(exponent, -200.0, 100.0));
}

double ChemoThermalConnector::steady_conc_at_temp(double temp_k) const noexcept {
    const double k = arrhenius_k(temp_k);
    return spec_.feed_conc_mol_m3 / (1.0 + k * spec_.residence_time_tau_s);
}

double ChemoThermalConnector::heat_generation_w_m3(double temp_k) const noexcept {
    const double k = arrhenius_k(temp_k);
    const double c_ss = steady_conc_at_temp(temp_k);
    return spec_.heat_of_reaction_exothermic_j_mol * k * c_ss;
}

double ChemoThermalConnector::heat_removal_w_m3(double temp_k) const noexcept {
    const double q_flow = (spec_.rho_cp_j_m3_k / spec_.residence_time_tau_s) *
                          (temp_k - spec_.feed_temp_k);
    const double q_cool = spec_.ua_per_volume_w_m3_k * (temp_k - spec_.coolant_temp_k);
    return q_flow + q_cool;
}

std::vector<double> ChemoThermalConnector::find_steady_state_temps_k(
    double t_min_k,
    double t_max_k,
    int samples) const {
    std::vector<double> roots;
    if (t_max_k <= t_min_k || samples < 10) {
        return roots;
    }
    const double dt = (t_max_k - t_min_k) / static_cast<double>(samples);
    auto balance = [&](double t) { return heat_generation_w_m3(t) - heat_removal_w_m3(t); };

    double prev_t = t_min_k;
    double prev_f = balance(prev_t);
    for (int i = 1; i <= samples; ++i) {
        const double cur_t = t_min_k + static_cast<double>(i) * dt;
        const double cur_f = balance(cur_t);
        if ((prev_f <= 0.0 && cur_f >= 0.0) || (prev_f >= 0.0 && cur_f <= 0.0)) {
            double lo = prev_t;
            double hi = cur_t;
            double f_lo = prev_f;
            for (int iter = 0; iter < 40; ++iter) {
                const double mid = 0.5 * (lo + hi);
                const double f_mid = balance(mid);
                if ((f_lo <= 0.0 && f_mid <= 0.0) || (f_lo >= 0.0 && f_mid >= 0.0)) {
                    lo = mid;
                    f_lo = f_mid;
                } else {
                    hi = mid;
                }
            }
            roots.push_back(0.5 * (lo + hi));
        }
        prev_t = cur_t;
        prev_f = cur_f;
    }
    return roots;
}

core::base::Result<CstrState> ChemoThermalConnector::step(double dt_s) {
    if (dt_s <= 0.0 || spec_.residence_time_tau_s <= 0.0 || spec_.rho_cp_j_m3_k <= 0.0) {
        return core::base::Error{
            441, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid CSTR step parameters", ""
        };
    }

    const int substeps = std::max(1, static_cast<int>(std::ceil(dt_s / 0.1)));
    const double sdt = dt_s / static_cast<double>(substeps);

    for (int s = 0; s < substeps; ++s) {
        const double k = arrhenius_k(state_.temp_k);
        // Semi-implicit concentration update (strictly positive for arbitrarily fast k)
        const double inv_tau = 1.0 / spec_.residence_time_tau_s;
        const double c_new = (state_.conc_mol_m3 + sdt * inv_tau * spec_.feed_conc_mol_m3) /
                             (1.0 + sdt * (inv_tau + k));

        const double q_rxn = spec_.heat_of_reaction_exothermic_j_mol * k * c_new;
        const double q_flow = (spec_.rho_cp_j_m3_k * inv_tau) * (spec_.feed_temp_k - state_.temp_k);
        const double q_cool = spec_.ua_per_volume_w_m3_k * (spec_.coolant_temp_k - state_.temp_k);

        state_.conc_mol_m3 = c_new;
        state_.temp_k = std::max(150.0, state_.temp_k + ((q_rxn + q_flow + q_cool) / spec_.rho_cp_j_m3_k) * sdt);
        state_.rate_constant_per_s = arrhenius_k(state_.temp_k);
        state_.time_s += sdt;
    }
    return state_;
}

core::base::Result<void> ChemoThermalConnector::advance(double total_time_s, double dt_s) {
    if (total_time_s <= 0.0 || dt_s <= 0.0) {
        return core::base::Error{
            442, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "CSTR advance duration and dt_s must be positive", ""
        };
    }
    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_s));
    for (size_t i = 0; i < steps; ++i) {
        auto s = step(dt_s);
        if (!s.is_ok()) return s.error();
    }
    return core::base::Result<void>::ok();
}

} // namespace shunya::integ
