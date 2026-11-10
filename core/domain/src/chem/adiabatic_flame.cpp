#include "shunya/chem/adiabatic_flame.hpp"
#include "shunya/matdb/shomate_evaluator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

namespace {

double species_sensible_h_kj_mol(const char* shomate_id, double fallback_cp_j, double T) noexcept {
    if (shomate_id && shomate_id[0] != '\0') {
        auto res = matdb::ShomateEvaluator::evaluate_delta_h_kj_mol(shomate_id, std::clamp(T, 298.0, 5500.0));
        if (res.is_ok()) return res.value();
    }
    return fallback_cp_j * (T - 298.15) * 1e-3;
}

} // namespace

core::base::Result<AdiabaticFlameResult> AdiabaticFlameCalculator::compute_fuel_air_flame(
    FuelKind fuel,
    double equivalence_ratio_phi,
    double initial_temp_k,
    bool include_high_t_dissociation) {

    if (equivalence_ratio_phi <= 0.0 || initial_temp_k <= 0.0) {
        return core::base::Result<AdiabaticFlameResult>::err(
            core::base::Error{106, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Equivalence ratio and initial temperature must be positive", ""});
    }

    // Standard reactants: phi moles of fuel + o2_stoich moles O2 + 3.76 * o2_stoich moles N2
    double o2_stoich = 2.0, n_co2_max = 1.0, n_h2o_max = 2.0, hf_fuel_kj = -74.87;
    const char* fuel_shomate = "CH4";
    double fuel_cp = 35.69;

    if (fuel == FuelKind::Propane) {
        o2_stoich = 5.0; n_co2_max = 3.0; n_h2o_max = 4.0; hf_fuel_kj = -104.68;
        fuel_shomate = ""; fuel_cp = 73.60;
    } else if (fuel == FuelKind::Hydrogen) {
        o2_stoich = 0.5; n_co2_max = 0.0; n_h2o_max = 1.0; hf_fuel_kj = 0.0;
        fuel_shomate = "H2"; fuel_cp = 28.84;
    }

    const double n_fuel = equivalence_ratio_phi;
    const double n_o2_in = o2_stoich;
    const double n_n2 = 3.76 * o2_stoich;
    const double n_react = n_fuel + n_o2_in + n_n2;

    const double h_react_kj =
        n_fuel * (hf_fuel_kj + species_sensible_h_kj_mol(fuel_shomate, fuel_cp, initial_temp_k)) +
        n_o2_in * species_sensible_h_kj_mol("O2", 29.38, initial_temp_k) +
        n_n2 * species_sensible_h_kj_mol("N2", 29.12, initial_temp_k);

    const double burn_frac = std::min(1.0, equivalence_ratio_phi);
    const double unburned_fuel = std::max(0.0, n_fuel - burn_frac);
    const double excess_o2_base = std::max(0.0, n_o2_in - burn_frac * o2_stoich);
    const double n_co2_base = burn_frac * n_co2_max;
    const double n_h2o_base = burn_frac * n_h2o_max;

    auto eval_products = [&](double T, double& out_n_prod) -> double {
        double alpha_co2 = 0.0;
        if (include_high_t_dissociation && n_co2_base > 0.0 && T > 1500.0) {
            // Equilibrium CO2 <=> CO + 0.5 O2 thermal dissociation (Kp = exp(-dG / RT))
            const double dG_co2 = 282980.0 - T * 86.8;
            const double kp = std::exp(-dG_co2 / (8.3144626 * T));
            // Approximate dissociation fraction x solving x / (1-x) * sqrt(0.5 * x / n_tot) ~ kp
            const double n_approx = unburned_fuel + excess_o2_base + n_co2_base + n_h2o_base + n_n2;
            const double rhs = kp * std::sqrt(2.0 * n_approx);
            alpha_co2 = std::clamp(std::pow(rhs, 2.0 / 3.0), 0.0, 0.45);
        }

        const double n_co2 = n_co2_base * (1.0 - alpha_co2);
        const double n_co = n_co2_base * alpha_co2;
        const double n_o2 = excess_o2_base + 0.5 * n_co;
        out_n_prod = unburned_fuel + n_o2 + n_co2 + n_co + n_h2o_base + n_n2;

        double H_kj = 0.0;
        H_kj += n_co2 * (-393.51 + species_sensible_h_kj_mol("CO2", 54.0, T));
        H_kj += n_co  * (-110.53 + species_sensible_h_kj_mol("", 34.0, T));
        H_kj += n_h2o_base * (-241.826 + species_sensible_h_kj_mol("H2O", 45.0, T));
        H_kj += n_o2  * species_sensible_h_kj_mol("O2", 34.5, T);
        H_kj += n_n2  * species_sensible_h_kj_mol("N2", 33.0, T);
        H_kj += unburned_fuel * (hf_fuel_kj + species_sensible_h_kj_mol(fuel_shomate, fuel_cp, T));
        return H_kj;
    };

    // 1) Solve constant-pressure H_prod(T_ad) == H_react(T_0) via bisection
    double lo = initial_temp_k, hi = 4500.0, n_prod_cp = n_react;
    for (int iter = 0; iter < 60; ++iter) {
        const double mid = 0.5 * (lo + hi);
        if (eval_products(mid, n_prod_cp) < h_react_kj) lo = mid;
        else hi = mid;
    }
    const double t_ad = 0.5 * (lo + hi);

    // 2) Solve constant-volume U_prod(T_v) == U_react(T_0) -> H_prod(T_v) - n_prod*R*T_v = H_react - n_react*R*T_0
    constexpr double kR_kj = 8.3144626e-3;
    const double u_react_kj = h_react_kj - n_react * kR_kj * initial_temp_k;
    lo = initial_temp_k; hi = 5000.0;
    double n_prod_cv = n_react;
    for (int iter = 0; iter < 60; ++iter) {
        const double mid = 0.5 * (lo + hi);
        const double h_m = eval_products(mid, n_prod_cv);
        const double u_m = h_m - n_prod_cv * kR_kj * mid;
        if (u_m < u_react_kj) lo = mid;
        else hi = mid;
    }
    const double t_cv = 0.5 * (lo + hi);

    AdiabaticFlameResult res;
    res.adiabatic_flame_temp_k = t_ad;
    res.constant_volume_temp_k = t_cv;
    res.moles_reactants = n_react;
    res.moles_products = n_prod_cv;
    return core::base::Result<AdiabaticFlameResult>::ok(res);
}

} // namespace shunya::chem
