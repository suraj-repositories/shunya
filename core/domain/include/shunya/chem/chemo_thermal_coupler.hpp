#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/chem/arrhenius_evaluator.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <vector>

namespace shunya::chem {

struct AdiabaticReactorConfig {
    double initial_conc_mol_m3{10.0};
    double initial_temp_k{450.0};
    double ambient_temp_k{450.0};
    double density_kg_m3{1000.0};
    double cp_j_kg_k{2000.0};
    double delta_h_rxn_j_mol{-800000.0}; // Exothermic negative
    double heat_loss_u_w_m3_k{0.0};      // 0 = perfectly adiabatic insulated vessel
    ModifiedArrheniusParams rate_params{1.0e8, 0.0, 95000.0};
};

struct ThermalRunawaySample {
    double time_s{0.0};
    double temp_k{300.0};
    double conc_mol_m3{0.0};
    double q_gen_w_m3{0.0};
};

/**
 * @brief Bidirectional chemo-thermal coupler linking exothermic kinetics Q_gen = -r * dH_rxn to 3D thermal conduction.
 */
class ChemoThermalCoupler {
public:
    [[nodiscard]] static core::base::Result<std::vector<ThermalRunawaySample>> simulate_reactor(
        const AdiabaticReactorConfig& cfg,
        double total_time_s,
        double dt_s);

    /**
     * @brief Evaluates instantaneous volumetric heat generation Q_gen = -r * dH_rxn and injects into ThermalSolver3D node.
     */
    static double couple_to_thermal_cell(
        phy::ThermalSolver3D& thermal_solver,
        size_t ix, size_t iy, size_t iz,
        double conc_mol_m3,
        const ModifiedArrheniusParams& rate,
        double delta_h_rxn_j_mol);
};

} // namespace shunya::chem
