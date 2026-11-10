#pragma once

#include "shunya/chem/gibbs_minimizer.hpp"
#include "shunya/core/base/result.hpp"

#include <string>
#include <vector>

namespace shunya::chem {

struct LeChatelierComparison {
    GibbsEquilibriumResult baseline;
    GibbsEquilibriumResult perturbed;
    double van_t_hoff_slope_observed{0.0}; // d(ln K) / d(1/T) = -Delta_H / R
    double implied_delta_h_j_mol{0.0};
};

/**
 * @brief Le Chatelier equilibrium shift evaluator verifying temperature, pressure, and van 't Hoff sensitivity.
 */
class EquilibriumShiftController {
public:
    [[nodiscard]] static core::base::Result<LeChatelierComparison> evaluate_shift(
        const std::vector<ThermoSpeciesEntry>& species,
        const std::vector<double>& initial_moles,
        double t1_k, double p1_bar,
        double t2_k, double p2_bar,
        const std::string& num_species,
        int num_power,
        const std::string& den_species,
        int den_power);
};

} // namespace shunya::chem
