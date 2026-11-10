#include "shunya/chem/aqueous_solution.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

namespace {

std::vector<double> compute_alpha_fractions(const PolyproticAcidSpec& acid, double pH) {
    const size_t n_pka = acid.pka_values.size();
    std::vector<double> log10_t(n_pka + 1, 0.0);
    double max_log = 0.0;
    for (size_t m = 1; m <= n_pka; ++m) {
        log10_t[m] = log10_t[m - 1] + (pH - acid.pka_values[m - 1]);
        max_log = std::max(max_log, log10_t[m]);
    }
    std::vector<double> alpha(n_pka + 1, 0.0);
    double sum = 0.0;
    for (size_t m = 0; m <= n_pka; ++m) {
        const double ex = std::clamp(log10_t[m] - max_log, -300.0, 300.0);
        alpha[m] = std::pow(10.0, ex);
        sum += alpha[m];
    }
    for (double& a : alpha) a /= sum;
    return alpha;
}

} // namespace

void AqueousSolutionEngine::add_strong_acid(double conc_mol_l) {
    // Dissociates 100% into H+ and Cl- (spectator anion charge -1)
    spectator_charge_ -= std::max(0.0, conc_mol_l);
}

void AqueousSolutionEngine::add_strong_base(double conc_mol_l) {
    // Dissociates 100% into Na+ and OH- (spectator cation charge +1)
    spectator_charge_ += std::max(0.0, conc_mol_l);
}

void AqueousSolutionEngine::add_polyprotic_acid(PolyproticAcidSpec acid) {
    acids_.push_back(std::move(acid));
}

void AqueousSolutionEngine::reset() {
    spectator_charge_ = 0.0;
    acids_.clear();
}

core::base::Result<AqueousEquilibriumResult> AqueousSolutionEngine::solve_equilibrium(double kw) const {
    if (kw <= 0.0) {
        return core::base::Result<AqueousEquilibriumResult>::err(
            core::base::Error{110, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Water auto-ionization Kw must be positive", ""});
    }

    auto charge_balance = [&](double pH) -> double {
        const double h = PhCalculator::h_from_pH(pH);
        const double oh = kw / std::max(1e-30, h);
        double q = h - oh + spectator_charge_;
        for (const auto& acid : acids_) {
            if (acid.total_conc_mol_l <= 0.0) continue;
            const auto alpha = compute_alpha_fractions(acid, pH);
            for (size_t m = 0; m < alpha.size(); ++m) {
                const int sp_charge = acid.neutral_charge - static_cast<int>(m);
                q += acid.total_conc_mol_l * alpha[m] * static_cast<double>(sp_charge);
            }
        }
        return q;
    };

    // f(pH) strictly decreases as pH increases -> guaranteed unique root in [-2, 16]
    double lo = -2.0;
    double hi = 16.0;
    for (int iter = 0; iter < 100; ++iter) {
        const double mid = 0.5 * (lo + hi);
        if (charge_balance(mid) > 0.0) lo = mid;
        else hi = mid;
    }

    const double pH = 0.5 * (lo + hi);
    const double h = PhCalculator::h_from_pH(pH);
    const double oh = kw / std::max(1e-30, h);
    const double pKw = -std::log10(kw);

    AqueousEquilibriumResult out;
    out.pH = pH;
    out.pOH = pKw - pH;
    out.h_plus_mol_l = h;
    out.oh_minus_mol_l = oh;
    for (const auto& acid : acids_) {
        out.acid_species_fractions.push_back(compute_alpha_fractions(acid, pH));
    }
    return core::base::Result<AqueousEquilibriumResult>::ok(std::move(out));
}

std::vector<std::pair<double, double>> AqueousSolutionEngine::titrate_strong_acid_base(
    double acid_conc_mol_l,
    double acid_vol_l,
    double base_conc_mol_l,
    const std::vector<double>& added_base_vols_l) {

    std::vector<std::pair<double, double>> curve;
    curve.reserve(added_base_vols_l.size());
    for (double vb : added_base_vols_l) {
        const double v_tot = std::max(1e-12, acid_vol_l + std::max(0.0, vb));
        AqueousSolutionEngine sol;
        sol.add_strong_acid((acid_conc_mol_l * acid_vol_l) / v_tot);
        sol.add_strong_base((base_conc_mol_l * std::max(0.0, vb)) / v_tot);
        const auto res = sol.solve_equilibrium();
        curve.emplace_back(vb, res.is_ok() ? res.value().pH : 7.0);
    }
    return curve;
}

} // namespace shunya::chem
