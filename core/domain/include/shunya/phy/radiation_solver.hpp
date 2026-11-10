#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/thermal.hpp"
#include "shunya/phy/view_factor_raymarcher.hpp"

#include <vector>

namespace shunya::phy {

/**
 * @brief Diffuse gray-body Stefan-Boltzmann radiation enclosure solver (radiosity matrix & net exchange).
 */
class RadiationSolver {
public:
    static constexpr double kStefanBoltzmann = core::domain::thermal::STEFAN_BOLTZMANN;

    /**
     * @brief Solves radiosity J_i and returns net radiative heat leaving each patch (Watts).
     */
    [[nodiscard]] static core::base::Result<std::vector<double>> solve_net_heat_flux_watts(
        const std::vector<RadiativePatch3D>& patches,
        const std::vector<std::vector<double>>& view_factors);

    /**
     * @brief Simulates transient radiative equilibrium for two thermally insulated parallel blackbody plates in vacuum.
     */
    [[nodiscard]] static core::base::Result<std::pair<double, double>> simulate_parallel_plates_equilibrium(
        double t1_init_k,
        double t2_init_k,
        double area_m2,
        double thermal_mass_j_k,
        double total_time_s,
        double dt_s,
        double emissivity = 1.0);
};

} // namespace shunya::phy
