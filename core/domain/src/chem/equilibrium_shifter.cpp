#include "shunya/chem/equilibrium_shifter.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

namespace {

double compute_qp(
    const GibbsEquilibriumResult& eq,
    const std::string& num_sp, int num_pow,
    const std::string& den_sp, int den_pow) {
    auto p_of = [&](const std::string& name) -> double {
        auto it = eq.fraction_map.find(name);
        return (it != eq.fraction_map.end()) ? (it->second * eq.pressure_bar) : 1e-30;
    };
    return std::pow(p_of(num_sp), num_pow) / std::max(1e-30, std::pow(p_of(den_sp), den_pow));
}

} // namespace

core::base::Result<LeChatelierComparison> EquilibriumShiftController::evaluate_shift(
    const std::vector<ThermoSpeciesEntry>& species,
    const std::vector<double>& initial_moles,
    double t1_k, double p1_bar,
    double t2_k, double p2_bar,
    const std::string& num_species, int num_power,
    const std::string& den_species, int den_power) {

    auto r1 = GibbsMinimizer::minimize(species, initial_moles, t1_k, p1_bar);
    if (!r1.is_ok()) return core::base::Result<LeChatelierComparison>::err(r1.error());

    auto r2 = GibbsMinimizer::minimize(species, initial_moles, t2_k, p2_bar);
    if (!r2.is_ok()) return core::base::Result<LeChatelierComparison>::err(r2.error());

    LeChatelierComparison out;
    out.baseline = std::move(r1.value());
    out.perturbed = std::move(r2.value());

    const double d_inv_t = (1.0 / t2_k) - (1.0 / t1_k);
    if (std::abs(d_inv_t) > 1e-12) {
        const double k1 = compute_qp(out.baseline, num_species, num_power, den_species, den_power);
        const double k2 = compute_qp(out.perturbed, num_species, num_power, den_species, den_power);
        if (k1 > 0.0 && k2 > 0.0) {
            out.van_t_hoff_slope_observed = std::log(k2 / k1) / d_inv_t;
            out.implied_delta_h_j_mol = -GibbsMinimizer::kGasConstantR * out.van_t_hoff_slope_observed;
        }
    }
    return core::base::Result<LeChatelierComparison>::ok(std::move(out));
}

} // namespace shunya::chem
