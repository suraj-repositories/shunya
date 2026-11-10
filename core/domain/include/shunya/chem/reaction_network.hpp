#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/chemistry.hpp"
#include "shunya/chem/stiff_ode_solver.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::chem {

/**
 * @brief Compiles stoichiometric mechanisms into stiff ODE systems conserving atomic element mass via ROS2.
 */
class ReactionNetworkEngine {
public:
    ReactionNetworkEngine() = default;

    void add_species(const core::domain::chem::ChemicalSpecies& species, double initial_molar_conc = 0.0);
    core::base::Result<void> add_reaction(const core::domain::chem::ChemicalReaction& rxn);

    void set_concentration(std::string_view formula, double conc_mol_l);
    [[nodiscard]] double concentration(std::string_view formula) const noexcept;
    [[nodiscard]] const std::vector<double>& concentrations() const noexcept { return concentrations_; }
    [[nodiscard]] double temperature_k() const noexcept { return temperature_k_; }
    void set_temperature_k(double temp_k) noexcept { temperature_k_ = temp_k; }

    /**
     * @brief Total moles of chemical element (e.g., "H", "O", "C") across all species per liter.
     */
    [[nodiscard]] double element_moles_per_l(std::string_view element) const noexcept;
    [[nodiscard]] double total_mass_kg_per_l() const noexcept;

    [[nodiscard]] core::base::Result<std::vector<std::vector<double>>> integrate(
        double t_end_s,
        double dt_s,
        bool use_arrhenius_rates = false);

private:
    std::vector<core::domain::chem::ChemicalSpecies> species_list_;
    std::unordered_map<std::string, size_t> species_index_;
    std::unordered_map<std::string, core::domain::chem::ChemicalSpecies> species_map_;
    std::vector<core::domain::chem::ChemicalReaction> reactions_;
    std::vector<double> concentrations_;
    double temperature_k_{298.15};
};

} // namespace shunya::chem
