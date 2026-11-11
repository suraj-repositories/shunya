#pragma once

#include "shunya/matdb/shomate_evaluator.hpp"

#include <cmath>
#include <vector>

namespace shunya::bench {

struct JanafSpeciesRef {
    double delta_hf_298_kj_mol{0.0};
    matdb::ShomateSegment shomate;
};

/**
 * @brief NIST-JANAF Shomate thermochemical database equilibrium validator (300 K - 2000 K).
 */
class BenchmarkChemicalEquilibrium {
public:
    [[nodiscard]] static double gibbs_j_mol(const JanafSpeciesRef& sp, double temp_k) noexcept {
        const double h_j_mol = (sp.delta_hf_298_kj_mol + sp.shomate.eval_delta_h_kj_mol(temp_k)) * 1000.0;
        const double s_j_mol_k = sp.shomate.eval_s_j_mol_k(temp_k);
        return h_j_mol - temp_k * s_j_mol_k;
    }

    [[nodiscard]] static double eval_delta_g0_from_shomate(
        const std::vector<std::pair<const JanafSpeciesRef*, double>>& stoich,
        double temp_k) noexcept {
        double dg_j_mol = 0.0;
        for (const auto& [sp, nu] : stoich) {
            dg_j_mol += nu * gibbs_j_mol(*sp, temp_k);
        }
        return dg_j_mol;
    }

    [[nodiscard]] static double log10_kp_from_shomate(
        const std::vector<std::pair<const JanafSpeciesRef*, double>>& stoich,
        double temp_k) noexcept {
        const double dg = eval_delta_g0_from_shomate(stoich, temp_k);
        constexpr double kR = 8.31446261815324;
        const double ln_k = -dg / (kR * temp_k);
        return ln_k / std::log(10.0);
    }
};

} // namespace shunya::bench
