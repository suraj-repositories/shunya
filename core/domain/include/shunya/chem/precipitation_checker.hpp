#pragma once

#include "shunya/core/base/result.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace shunya::chem {

struct SparinglySolubleSalt {
    std::string name;
    int stoich_cation_a{1};
    int stoich_anion_b{1};
    double k_sp{1.8e-10}; // e.g. AgCl = 1.8e-10, BaSO4 = 1.1e-10, CaCO3 = 3.3e-9
};

struct PrecipitationReport {
    bool precipitated{false};
    double reaction_quotient_qsp{0.0};
    double k_sp{0.0};
    double solid_precipitated_mol_l{0.0};
    double remaining_cation_mol_l{0.0};
    double remaining_anion_mol_l{0.0};
};

/**
 * @brief Solubility product (Q_sp > K_sp) precipitation detector and equilibrium ion concentration solver.
 */
class PrecipitationChecker {
public:
    [[nodiscard]] static SparinglySolubleSalt agcl() { return {"AgCl", 1, 1, 1.8e-10}; }
    [[nodiscard]] static SparinglySolubleSalt baso4() { return {"BaSO4", 1, 1, 1.1e-10}; }
    [[nodiscard]] static SparinglySolubleSalt caco3() { return {"CaCO3", 1, 1, 3.3e-9}; }

    [[nodiscard]] static PrecipitationReport evaluate(
        const SparinglySolubleSalt& salt,
        double initial_cation_mol_l,
        double initial_anion_mol_l) noexcept {

        const double c_m = std::max(0.0, initial_cation_mol_l);
        const double c_x = std::max(0.0, initial_anion_mol_l);
        const int a = std::max(1, salt.stoich_cation_a);
        const int b = std::max(1, salt.stoich_anion_b);

        const double q_sp = std::pow(c_m, a) * std::pow(c_x, b);
        PrecipitationReport rep;
        rep.reaction_quotient_qsp = q_sp;
        rep.k_sp = salt.k_sp;
        rep.remaining_cation_mol_l = c_m;
        rep.remaining_anion_mol_l = c_x;

        if (q_sp <= salt.k_sp) {
            rep.precipitated = false;
            return rep;
        }

        rep.precipitated = true;
        // Max possible reaction extent before limiting reactant vanishes
        double hi = std::min(c_m / static_cast<double>(a), c_x / static_cast<double>(b));
        double lo = 0.0;
        for (int iter = 0; iter < 80; ++iter) {
            const double mid = 0.5 * (lo + hi);
            const double rem_m = c_m - static_cast<double>(a) * mid;
            const double rem_x = c_x - static_cast<double>(b) * mid;
            const double q = std::pow(std::max(0.0, rem_m), a) * std::pow(std::max(0.0, rem_x), b);
            if (q > salt.k_sp) lo = mid;
            else hi = mid;
        }
        const double x_eq = 0.5 * (lo + hi);
        rep.solid_precipitated_mol_l = x_eq;
        rep.remaining_cation_mol_l = std::max(0.0, c_m - static_cast<double>(a) * x_eq);
        rep.remaining_anion_mol_l = std::max(0.0, c_x - static_cast<double>(b) * x_eq);
        return rep;
    }
};

} // namespace shunya::chem
