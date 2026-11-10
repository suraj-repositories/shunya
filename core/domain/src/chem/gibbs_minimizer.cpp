#include "shunya/chem/gibbs_minimizer.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace shunya::chem {

double ThermoSpeciesEntry::standard_enthalpy_j_mol(double temp_k) const noexcept {
    if (temp_k <= 0.0) return hf_298_j_mol;
    return hf_298_j_mol + cp_const_j_mol_k * (temp_k - 298.15);
}

double ThermoSpeciesEntry::standard_gibbs_j_mol(double temp_k) const noexcept {
    if (explicit_g0_t_j_mol < 1e29) {
        return explicit_g0_t_j_mol;
    }
    const double t = std::max(1.0, temp_k);
    const double h_t = hf_298_j_mol + cp_const_j_mol_k * (t - 298.15);
    const double s_t = s_298_j_mol_k + cp_const_j_mol_k * std::log(t / 298.15);
    return h_t - t * s_t;
}

core::base::Result<GibbsEquilibriumResult> GibbsMinimizer::minimize(
    const std::vector<ThermoSpeciesEntry>& species,
    const std::vector<double>& initial_moles,
    double temperature_k,
    double pressure_bar,
    int max_iterations,
    double tolerance) {

    const size_t ns = species.size();
    if (ns == 0 || initial_moles.size() != ns || temperature_k <= 0.0 || pressure_bar <= 0.0) {
        return core::base::Result<GibbsEquilibriumResult>::err(
            core::base::Error{95, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "GibbsMinimizer requires non-empty species matching initial_moles and positive T, P", ""});
    }

    // Collect distinct element names
    std::set<std::string> elem_set;
    for (const auto& sp : species) {
        for (const auto& [el, cnt] : sp.elements) {
            if (cnt != 0) elem_set.insert(el);
        }
    }
    if (elem_set.empty()) {
        return core::base::Result<GibbsEquilibriumResult>::err(
            core::base::Error{96, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Species must declare non-empty elemental compositions", ""});
    }

    std::vector<std::string> all_elems(elem_set.begin(), elem_set.end());
    std::vector<double> all_b(all_elems.size(), 0.0);
    for (size_t e = 0; e < all_elems.size(); ++e) {
        for (size_t i = 0; i < ns; ++i) {
            auto it = species[i].elements.find(all_elems[e]);
            if (it != species[i].elements.end()) {
                all_b[e] += static_cast<double>(it->second) * std::max(0.0, initial_moles[i]);
            }
        }
    }

    // Extract linearly independent element rows via Modified Gram-Schmidt to prevent singular constraint systems
    // (e.g. N2O4 <-> 2 NO2 where N and O rows are collinear)
    std::vector<std::vector<double>> A_indep;
    std::vector<double> b_indep;
    std::vector<std::vector<double>> ortho_basis;

    for (size_t e = 0; e < all_elems.size(); ++e) {
        if (all_b[e] <= 0.0) continue;
        std::vector<double> row(ns, 0.0);
        for (size_t i = 0; i < ns; ++i) {
            auto it = species[i].elements.find(all_elems[e]);
            if (it != species[i].elements.end()) row[i] = static_cast<double>(it->second);
        }
        std::vector<double> v = row;
        for (const auto& q : ortho_basis) {
            double dot = 0.0;
            for (size_t i = 0; i < ns; ++i) dot += v[i] * q[i];
            for (size_t i = 0; i < ns; ++i) v[i] -= dot * q[i];
        }
        double norm2 = 0.0;
        for (double x : v) norm2 += x * x;
        if (norm2 > 1e-12) {
            const double inv = 1.0 / std::sqrt(norm2);
            for (double& x : v) x *= inv;
            ortho_basis.push_back(std::move(v));
            A_indep.push_back(std::move(row));
            b_indep.push_back(all_b[e]);
        }
    }

    const size_t ne = A_indep.size();
    if (ne == 0) {
        return core::base::Result<GibbsEquilibriumResult>::err(
            core::base::Error{97, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Initial moles correspond to zero conserved elements", ""});
    }

    // Positive feasible-scale starting moles (avoid starting near zero for missing product species)
    double scale_moles = 0.0;
    for (double m : initial_moles) scale_moles += std::max(0.0, m);
    if (scale_moles <= 0.0) scale_moles = 1.0;

    std::vector<double> n(ns, 0.0);
    double n_tot = 0.0;
    for (size_t i = 0; i < ns; ++i) {
        n[i] = std::max(initial_moles[i], 0.05 * scale_moles / static_cast<double>(ns));
        n_tot += n[i];
    }

    const double rt = kGasConstantR * temperature_k;
    const double ln_p = std::log(pressure_bar);
    std::vector<double> g0_rt(ns, 0.0);
    for (size_t i = 0; i < ns; ++i) {
        g0_rt[i] = species[i].standard_gibbs_j_mol(temperature_k) / rt;
    }

    std::vector<double> pi(ne, 0.0);
    bool converged = false;
    int iters_used = 0;

    for (int iter = 1; iter <= max_iterations; ++iter) {
        iters_used = iter;
        std::vector<double> mu_rt(ns, 0.0);
        for (size_t i = 0; i < ns; ++i) {
            mu_rt[i] = g0_rt[i] + std::log(std::max(1e-30, n[i]) / n_tot) + ln_p;
        }

        // Assemble (ne + 1) x (ne + 1) NASA CEA Newton matrix
        core::domain::DenseMatrix M(ne + 1, ne + 1);
        std::vector<double> rhs(ne + 1, 0.0);

        for (size_t e = 0; e < ne; ++e) {
            double elem_sum = 0.0;
            double elem_mu = 0.0;
            for (size_t i = 0; i < ns; ++i) {
                elem_sum += A_indep[e][i] * n[i];
                elem_mu += A_indep[e][i] * n[i] * mu_rt[i];
            }
            for (size_t f = 0; f < ne; ++f) {
                double g_ef = 0.0;
                for (size_t i = 0; i < ns; ++i) {
                    g_ef += A_indep[e][i] * A_indep[f][i] * n[i];
                }
                M(e, f) = g_ef;
            }
            M(e, ne) = elem_sum;
            M(ne, e) = elem_sum;
            rhs[e] = (b_indep[e] - elem_sum) + elem_mu;
        }

        double sum_n = 0.0;
        double sum_n_mu = 0.0;
        for (size_t i = 0; i < ns; ++i) {
            sum_n += n[i];
            sum_n_mu += n[i] * mu_rt[i];
        }
        M(ne, ne) = sum_n - n_tot;
        rhs[ne] = (n_tot - sum_n) + sum_n_mu;

        auto sol = M.solve_lu(rhs);
        if (!sol.is_ok()) {
            return core::base::Result<GibbsEquilibriumResult>::err(sol.error());
        }
        const auto& x = sol.value();
        for (size_t e = 0; e < ne; ++e) pi[e] = x[e];
        const double dln_ntot = x[ne];

        std::vector<double> dln_n(ns, 0.0);
        double max_dln = 0.0;
        double max_weighted = 0.0;
        for (size_t i = 0; i < ns; ++i) {
            double pi_dot = 0.0;
            for (size_t e = 0; e < ne; ++e) pi_dot += A_indep[e][i] * pi[e];
            dln_n[i] = -mu_rt[i] + dln_ntot + pi_dot;
            max_dln = std::max(max_dln, std::abs(dln_n[i]));
            max_weighted = std::max(max_weighted, std::abs(dln_n[i]) * (n[i] / n_tot));
        }

        // NASA CEA step size control ensuring smooth convergence without overshoot
        double lam = 1.0;
        if (max_dln > 2.0) lam = std::min(lam, 2.0 / max_dln);
        if (std::abs(dln_ntot) > 0.5) lam = std::min(lam, 0.5 / std::abs(dln_ntot));

        for (size_t i = 0; i < ns; ++i) {
            n[i] = std::clamp(n[i] * std::exp(lam * dln_n[i]), 1e-30, 1e12 * scale_moles);
        }
        n_tot = 0.0;
        for (double ni : n) n_tot += ni;

        double max_elem_err = 0.0;
        for (size_t e = 0; e < ne; ++e) {
            double cur = 0.0;
            for (size_t i = 0; i < ns; ++i) cur += A_indep[e][i] * n[i];
            max_elem_err = std::max(max_elem_err, std::abs(cur - b_indep[e]) / std::max(1.0, b_indep[e]));
        }

        if (max_weighted < tolerance && max_elem_err < tolerance && std::abs(dln_ntot) < tolerance) {
            converged = true;
            break;
        }
    }

    if (!converged) {
        return core::base::Result<GibbsEquilibriumResult>::err(
            core::base::Error{98, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
                              "GibbsMinimizer failed to converge within max_iterations", ""});
    }

    GibbsEquilibriumResult res;
    res.converged = true;
    res.iterations = iters_used;
    res.temperature_k = temperature_k;
    res.pressure_bar = pressure_bar;
    res.total_moles = n_tot;
    res.species_moles = n;
    res.mole_fractions.resize(ns, 0.0);

    double total_g = 0.0;
    for (size_t i = 0; i < ns; ++i) {
        const double yi = n[i] / n_tot;
        res.mole_fractions[i] = yi;
        res.species_mole_map[species[i].formula] = n[i];
        res.fraction_map[species[i].formula] = yi;
        const double mu_i = species[i].standard_gibbs_j_mol(temperature_k) +
                            rt * std::log(std::max(1e-30, yi) * pressure_bar);
        total_g += n[i] * mu_i;
    }
    res.total_gibbs_j = total_g;
    return core::base::Result<GibbsEquilibriumResult>::ok(std::move(res));
}

} // namespace shunya::chem
