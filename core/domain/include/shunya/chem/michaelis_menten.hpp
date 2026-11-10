#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace shunya::chem {

enum class EnzymeInhibitionMode : uint8_t {
    None,
    Competitive,
    NonCompetitive,
    Uncompetitive
};

struct MichaelisMentenParams {
    double v_max_mol_l_s{1.0};
    double k_m_mol_l{0.05};
    double k_i_mol_l{0.02};
    EnzymeInhibitionMode mode{EnzymeInhibitionMode::None};
};

/**
 * @brief Michaelis-Menten enzyme kinetics with competitive/non-competitive/uncompetitive inhibition & Lineweaver-Burk.
 */
class MichaelisMentenModel {
public:
    [[nodiscard]] static double velocity(
        const MichaelisMentenParams& p,
        double substrate_s,
        double inhibitor_i = 0.0) noexcept {
        if (substrate_s <= 0.0 || p.v_max_mol_l_s <= 0.0 || p.k_m_mol_l <= 0.0) return 0.0;
        const double ki = std::max(1e-18, p.k_i_mol_l);
        const double inh = std::max(0.0, inhibitor_i);
        const double alpha = (p.mode == EnzymeInhibitionMode::Competitive ||
                              p.mode == EnzymeInhibitionMode::NonCompetitive)
                                 ? (1.0 + inh / ki)
                                 : 1.0;
        const double alpha_p = (p.mode == EnzymeInhibitionMode::NonCompetitive ||
                                p.mode == EnzymeInhibitionMode::Uncompetitive)
                                   ? (1.0 + inh / ki)
                                   : 1.0;
        const double v_max_eff = p.v_max_mol_l_s / alpha_p;
        const double k_m_eff = p.k_m_mol_l * (alpha / alpha_p);
        return (v_max_eff * substrate_s) / (k_m_eff + substrate_s);
    }

    static void lineweaver_burk_coefficients(
        const MichaelisMentenParams& p,
        double inhibitor_i,
        double& out_slope,
        double& out_y_intercept) noexcept {
        const double ki = std::max(1e-18, p.k_i_mol_l);
        const double inh = std::max(0.0, inhibitor_i);
        const double alpha = (p.mode == EnzymeInhibitionMode::Competitive ||
                              p.mode == EnzymeInhibitionMode::NonCompetitive)
                                 ? (1.0 + inh / ki)
                                 : 1.0;
        const double alpha_p = (p.mode == EnzymeInhibitionMode::NonCompetitive ||
                                p.mode == EnzymeInhibitionMode::Uncompetitive)
                                   ? (1.0 + inh / ki)
                                   : 1.0;
        // 1 / v = (alpha * Km / Vmax) * (1/[S]) + (alpha' / Vmax)
        out_slope = (alpha * p.k_m_mol_l) / p.v_max_mol_l_s;
        out_y_intercept = alpha_p / p.v_max_mol_l_s;
    }
};

} // namespace shunya::chem
