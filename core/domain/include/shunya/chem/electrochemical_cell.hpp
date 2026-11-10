#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace shunya::chem {

struct HalfCellStandardPotential {
    std::string half_reaction_id; // e.g. "Cu2+/Cu", "Zn2+/Zn"
    double e0_volts{0.0};
    int electrons_z{2};
};

/**
 * @brief Electrochemical half-cell standard potential table and Nernst non-standard cell voltage evaluator.
 */
class ElectrochemicalCellModel {
public:
    static constexpr double kGasConstantR = 8.31446261815324;
    static constexpr double kFaradayF = 96485.33212; // C / mol

    [[nodiscard]] static const std::unordered_map<std::string, HalfCellStandardPotential>&
    standard_potentials();

    [[nodiscard]] static core::base::Result<double> lookup_e0_volts(std::string_view half_cell_id);

    [[nodiscard]] static core::base::Result<double> evaluate_nernst_cell_v(
        double e0_cathode_v,
        double e0_anode_v,
        int z_electrons,
        double reaction_quotient_q,
        double temp_k = 298.15);

    [[nodiscard]] static double equilibrium_constant_k(
        double e0_cell_v,
        int z_electrons,
        double temp_k = 298.15) noexcept;

    [[nodiscard]] static double faraday_mass_deposited_kg(
        double current_a,
        double time_s,
        double molar_mass_kg_mol,
        int z_electrons) noexcept;
};

} // namespace shunya::chem
