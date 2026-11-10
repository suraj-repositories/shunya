#pragma once

#include "shunya/chem/nasa_cea_algorithm.hpp"
#include "shunya/core/base/result.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::chem {

/**
 * @brief NASA CEA (Gordon & McBride) Lagrange-multiplier Gibbs free energy minimizer G(T, P, n).
 */
class GibbsMinimizer {
public:
    static constexpr double kGasConstantR = 8.31446261815324;

    [[nodiscard]] static core::base::Result<GibbsEquilibriumResult> minimize(
        const std::vector<ThermoSpeciesEntry>& species,
        const std::vector<double>& initial_moles,
        double temperature_k,
        double pressure_bar,
        int max_iterations = 200,
        double tolerance = 1e-10);
};

} // namespace shunya::chem
