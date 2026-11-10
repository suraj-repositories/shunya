#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::chem {

struct ThermoSpeciesEntry {
    std::string formula;
    std::unordered_map<std::string, int> elements; // Element symbol -> stoichiometric count
    double hf_298_j_mol{0.0};     // Standard enthalpy of formation at 298.15 K [J/mol]
    double s_298_j_mol_k{0.0};    // Standard molar entropy at 298.15 K, 1 bar [J/(mol*K)]
    double cp_const_j_mol_k{29.1}; // Mean/constant Cp (used when no NASA/Shomate polynomial supplied)

    // Optional polynomial delta_G0(T) override [J/mol]; if finite, overrides hf - T*s + Cp integrals
    double explicit_g0_t_j_mol{1e30};

    [[nodiscard]] double standard_gibbs_j_mol(double temp_k) const noexcept;
    [[nodiscard]] double standard_enthalpy_j_mol(double temp_k) const noexcept;
};

struct GibbsEquilibriumResult {
    bool converged{false};
    int iterations{0};
    double temperature_k{298.15};
    double pressure_bar{1.0};
    double total_moles{0.0};
    double total_gibbs_j{0.0};
    std::vector<double> species_moles;
    std::vector<double> mole_fractions;
    std::unordered_map<std::string, double> species_mole_map;
    std::unordered_map<std::string, double> fraction_map;
};

} // namespace shunya::chem
