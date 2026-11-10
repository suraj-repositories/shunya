#include "shunya/chem/combustion_model.hpp"
#include "shunya/chem/arrhenius_evaluator.hpp"
#include "shunya/chem/stiff_ode_solver.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

core::base::Result<CombustionMixtureState> CombustionKineticsModel::simulate_combustion(
    FuelKind fuel,
    double fuel_mol_m3,
    double o2_mol_m3,
    double n2_mol_m3,
    double temperature_k,
    double duration_s,
    double dt_s) {

    if (fuel_mol_m3 < 0.0 || o2_mol_m3 < 0.0 || n2_mol_m3 < 0.0 || temperature_k <= 0.0 || duration_s <= 0.0) {
        return core::base::Result<CombustionMixtureState>::err(
            core::base::Error{105, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Invalid combustion mixture parameters", ""});
    }

    const auto spec = FlammabilityLimits::spec_for(fuel);
    const double total_moles = fuel_mol_m3 + o2_mol_m3 + n2_mol_m3;
    const double fuel_frac = (total_moles > 0.0) ? (fuel_mol_m3 / total_moles) : 0.0;

    CombustionMixtureState out;
    out.fuel_mol_m3 = fuel_mol_m3;
    out.o2_mol_m3 = o2_mol_m3;
    out.n2_mol_m3 = n2_mol_m3;
    out.temperature_k = temperature_k;

    // Require BOTH flammable mixture [LFL, UFL] AND temperature >= auto-ignition threshold
    if (!FlammabilityLimits::is_flammable(spec, fuel_frac) ||
        temperature_k < spec.auto_ignition_temp_k ||
        o2_mol_m3 <= 1e-12) {
        out.ignited = false;
        return core::base::Result<CombustionMixtureState>::ok(out);
    }

    out.ignited = true;

    // Stoichiometry per mole of fuel:
    //   Methane (C1H4): nC = 1, nH2O = 2, O2 for step 1 (CO + H2O) = 1.5
    //   Propane (C3H8): nC = 3, nH2O = 4, O2 for step 1 (3 CO + 4 H2O) = 3.5
    //   Hydrogen (H2):  nC = 0, nH2O = 1, O2 for step 1 = 0.5
    double n_c = 1.0, n_h2o = 2.0, o2_step1 = 1.5;
    double dh1_j = 519000.0, dh2_j = 283000.0; // Positive heat release magnitudes
    if (fuel == FuelKind::Propane) {
        n_c = 3.0; n_h2o = 4.0; o2_step1 = 3.5;
        dh1_j = 1195000.0;
    } else if (fuel == FuelKind::Hydrogen) {
        n_c = 0.0; n_h2o = 1.0; o2_step1 = 0.5;
        dh1_j = 241800.0; dh2_j = 0.0;
    }

    const ModifiedArrheniusParams step1_arr{spec.pre_exponential, 0.0, spec.activation_j_mol};
    const ModifiedArrheniusParams step2_arr{2.0e6, 0.0, 75000.0};
    const double mixture_rho_cp = std::max(500.0, total_moles * 35.0);

    // State vector y: [Fuel, O2, CO, CO2, H2O, T]
    std::vector<double> y{fuel_mol_m3, o2_mol_m3, 0.0, 0.0, 0.0, temperature_k};

    auto rhs = [&](double, const std::vector<double>& s, std::vector<double>& dy) {
        const double cf = std::max(0.0, s[0]);
        const double co2_o = std::max(0.0, s[1]);
        const double cco = std::max(0.0, s[2]);
        const double T = std::clamp(s[5], 200.0, 4000.0);

        const double k1 = ArrheniusRateEvaluator::modified_arrhenius(step1_arr, T);
        const double k2 = ArrheniusRateEvaluator::modified_arrhenius(step2_arr, T);

        const double r1 = k1 * cf * co2_o;
        const double r2 = (n_c > 0.0) ? (k2 * cco * co2_o) : 0.0;

        dy[0] = -r1;
        dy[1] = -o2_step1 * r1 - 0.5 * r2;
        dy[2] = n_c * r1 - r2;
        dy[3] = r2;
        dy[4] = n_h2o * r1;
        dy[5] = (dh1_j * r1 + dh2_j * r2) / mixture_rho_cp;
    };

    const double safe_dt = std::min(dt_s, 1e-4);
    StiffOdeSolver::Options opts;
    opts.clamp_non_negative = false; // Preserve exact C/H/O stoichiometric linearity
    auto sim = StiffOdeSolver::integrate(duration_s, safe_dt, y, rhs, opts);
    if (!sim.is_ok()) {
        return core::base::Result<CombustionMixtureState>::err(sim.error());
    }

    out.fuel_mol_m3 = std::max(0.0, y[0]);
    out.o2_mol_m3   = std::max(0.0, y[1]);
    out.co_mol_m3   = std::max(0.0, y[2]);
    out.co2_mol_m3  = std::max(0.0, y[3]);
    out.h2o_mol_m3  = std::max(0.0, y[4]);
    out.temperature_k = y[5];
    return core::base::Result<CombustionMixtureState>::ok(out);
}

} // namespace shunya::chem
