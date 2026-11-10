#include "shunya/chem/chemo_thermal_coupler.hpp"
#include "shunya/chem/stiff_ode_solver.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

core::base::Result<std::vector<ThermalRunawaySample>> ChemoThermalCoupler::simulate_reactor(
    const AdiabaticReactorConfig& cfg,
    double total_time_s,
    double dt_s) {
    if (cfg.density_kg_m3 <= 0.0 || cfg.cp_j_kg_k <= 0.0 || total_time_s <= 0.0 || dt_s <= 0.0) {
        return core::base::Result<std::vector<ThermalRunawaySample>>::err(
            core::base::Error{102, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Invalid ChemoThermalCoupler parameters", ""});
    }

    const double rho_cp = cfg.density_kg_m3 * cfg.cp_j_kg_k;
    auto rhs = [&](double, const std::vector<double>& y, std::vector<double>& dydt) {
        const double C = std::max(0.0, y[0]);
        const double T = std::max(1.0, y[1]);
        const double k = ArrheniusRateEvaluator::modified_arrhenius(cfg.rate_params, T);
        const double rate = k * C;
        const double q_gen = (-cfg.delta_h_rxn_j_mol) * rate;
        dydt[0] = -rate;
        dydt[1] = (q_gen - cfg.heat_loss_u_w_m3_k * (T - cfg.ambient_temp_k)) / rho_cp;
    };

    std::vector<double> state{cfg.initial_conc_mol_m3, cfg.initial_temp_k};
    StiffOdeSolver::Options opts;
    opts.clamp_non_negative = true;
    auto sim = StiffOdeSolver::integrate(total_time_s, dt_s, state, rhs, opts);
    if (!sim.is_ok()) {
        return core::base::Result<std::vector<ThermalRunawaySample>>::err(sim.error());
    }

    const auto& hist = sim.value();
    const double actual_dt = total_time_s / static_cast<double>(std::max<size_t>(1, hist.size() - 1));
    std::vector<ThermalRunawaySample> samples;
    samples.reserve(hist.size());

    for (size_t i = 0; i < hist.size(); ++i) {
        const double C = hist[i][0];
        const double T = hist[i][1];
        const double k = ArrheniusRateEvaluator::modified_arrhenius(cfg.rate_params, T);
        const double q = (-cfg.delta_h_rxn_j_mol) * k * C;
        samples.push_back({static_cast<double>(i) * actual_dt, T, C, q});
    }
    return core::base::Result<std::vector<ThermalRunawaySample>>::ok(std::move(samples));
}

double ChemoThermalCoupler::couple_to_thermal_cell(
    phy::ThermalSolver3D& thermal_solver,
    size_t ix, size_t iy, size_t iz,
    double conc_mol_m3,
    const ModifiedArrheniusParams& rate,
    double delta_h_rxn_j_mol) {
    const double T = thermal_solver.temperature(ix, iy, iz);
    const double k = ArrheniusRateEvaluator::modified_arrhenius(rate, T);
    const double q_gen = (-delta_h_rxn_j_mol) * k * std::max(0.0, conc_mol_m3);
    thermal_solver.set_volumetric_source(ix, iy, iz, q_gen);
    return q_gen;
}

} // namespace shunya::chem
