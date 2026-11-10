#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/phase_change_enthalpy.hpp"

#include <cstddef>
#include <vector>

namespace shunya::phy {

struct PhaseChangeDomain1D {
    size_t num_cells{100};
    double length_m{0.10};
    double wall_temp_k{373.15};
    double initial_temp_k{273.15};
    double t_melt_k{273.15};
    double density_kg_m3{1000.0};
    double k_liquid_w_mk{0.60};
    double k_solid_w_mk{2.22};
    double cp_liquid_j_kg_k{4184.0};
    double cp_solid_j_kg_k{2108.0};
    double latent_heat_j_kg{334000.0};
};

struct PhaseChangeResult1D {
    std::vector<double> cell_temperatures_k;
    std::vector<double> liquid_fraction;
    std::vector<double> enthalpy_j_m3;
    double interface_position_m{0.0};
    double total_absorbed_energy_j_m2{0.0};
};

struct LumpedMeltingSample {
    double time_s{0.0};
    double temperature_k{273.15};
    double liquid_fraction{0.0};
    double absorbed_heat_j{0.0};
};

/**
 * @brief Conservative enthalpy-method phase change solver conserving latent heat without mesh deformation.
 */
class EnthalpyPhaseChangeSolver {
public:
    /**
     * @brief Simulates 1D melting/solidification front (Stefan problem) using explicit conservative enthalpy update.
     */
    [[nodiscard]] static core::base::Result<PhaseChangeResult1D> solve_1d_stefan(
        const PhaseChangeDomain1D& domain,
        double total_time_s,
        double cfl_factor = 0.25);

    /**
     * @brief Simulates heating/melting of a finite mass M absorbing constant power Q_watts across solid->0C plateau->liquid.
     */
    [[nodiscard]] static std::vector<LumpedMeltingSample> simulate_lumped_melting(
        double mass_kg,
        double initial_temp_k,
        double input_power_w,
        const mat::LatentPhaseParameters& params,
        double total_time_s,
        double dt_s);
};

} // namespace shunya::phy
