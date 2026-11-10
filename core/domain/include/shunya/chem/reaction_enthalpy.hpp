#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/matdb/shomate_evaluator.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::chem {

struct ThermochemicalSpecies {
    std::string formula;
    double delta_hf_298_kj_mol{0.0}; // Standard enthalpy of formation at 298.15 K [kJ/mol]
    double cp_298_j_mol_k{29.1};     // Fallback constant molar Cp if Shomate ID absent
    std::string shomate_id;          // Optional link to ShomateEvaluator database
};

struct StoichTerm {
    std::string formula;
    double nu{1.0}; // Positive for products, negative for reactants
};

/**
 * @brief Calculates standard enthalpy of reaction (Hess's Law) & Kirchhoff temperature variation Delta H(T).
 */
class ReactionEnthalpyCalculator {
public:
    static void register_defaults(std::unordered_map<std::string, ThermochemicalSpecies>& db);

    [[nodiscard]] static const std::unordered_map<std::string, ThermochemicalSpecies>& default_database();

    [[nodiscard]] static core::base::Result<double> species_enthalpy_kj_mol(
        const ThermochemicalSpecies& sp,
        double temp_k);

    [[nodiscard]] static core::base::Result<double> reaction_enthalpy_kj_mol(
        const std::vector<StoichTerm>& terms,
        double temp_k = 298.15,
        const std::unordered_map<std::string, ThermochemicalSpecies>* custom_db = nullptr);
};

} // namespace shunya::chem
