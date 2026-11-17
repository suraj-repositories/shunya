#pragma once

#include "shunya/core/base/result.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace shunya::mat {

enum class ElectrolyteType : uint8_t {
    NonElectrolyte,
    WeakElectrolyte,
    StrongElectrolyte
};

enum class SaturationState : uint8_t {
    Unsaturated,
    Saturated,
    Supersaturated
};

[[nodiscard]] constexpr std::string_view saturation_state_to_string(SaturationState s) noexcept {
    switch (s) {
    case SaturationState::Unsaturated:    return "Unsaturated";
    case SaturationState::Saturated:      return "Saturated";
    case SaturationState::Supersaturated: return "Supersaturated";
    }
    return "Unknown";
}

struct SolutionThermodynamicModel {
    double base_solubility_g_per_100g{36.0};   // e.g. NaCl ~36g/100g H2O at 293K
    double delta_h_dissolution_j_mol{3880.0};  // Enthalpy of dissolution
    double reference_temp_k{293.15};
    ElectrolyteType electrolyte_type{ElectrolyteType::StrongElectrolyte};
    int ion_count_per_formula{2};              // e.g. NaCl -> 2 ions (Na+ + Cl-)
    double limiting_molar_conductivity_s_m2_mol{0.0126}; // Lambda_0
};

struct SolutionSaturationResult {
    SaturationState state{SaturationState::Unsaturated};
    double temperature_k{298.15};
    double solubility_limit_g_per_100g{0.0};
    double current_solute_concentration_g_per_100g{0.0};
    double dissolved_solute_mass_g{0.0};
    double precipitated_mass_g{0.0};
    double degree_of_dissociation{1.0}; // alpha (0..1)
    double vant_hoff_factor{1.0};       // i
    double ionic_conductivity_s_m{0.0}; // sigma
    std::string summary_description;
};

/**
 * @brief Liquid-solid solubility modeling engine with temperature-dependent saturation curves
 *        and electrolytic dissociation (van 't Hoff factor, Ostwald dilution, Kohlrausch conductivity).
 * (Roadmap task PH4-03.01.005)
 */
class SolutionSolubilityEngine {
public:
    static constexpr double GAS_CONSTANT_R = 8.31446261815324; // J/(mol*K)

    /**
     * @brief Computes temperature-dependent saturation solubility limit C_sat(T) via van 't Hoff equation:
     *        S(T) = S_0 * exp(-(Delta H_diss / R) * (1/T - 1/T_0))
     */
    [[nodiscard]] static double compute_solubility_limit(
        const SolutionThermodynamicModel& model,
        double temperature_k) noexcept;

    /**
     * @brief Computes degree of electrolytic dissociation alpha(T, C) and van 't Hoff factor i.
     *        alpha = 1.0 for StrongElectrolyte, Ostwald equilibrium for WeakElectrolyte, 0.0 for NonElectrolyte.
     */
    [[nodiscard]] static std::pair<double, double> compute_electrolytic_dissociation(
        ElectrolyteType type,
        int ion_count,
        double solute_moles_per_liter,
        double temperature_k) noexcept;

    /**
     * @brief Computes solution electrolytic conductivity using Kohlrausch's law for strong electrolytes
     *        or Ostwald dissociation for weak electrolytes.
     */
    [[nodiscard]] static double compute_ionic_conductivity(
        const SolutionThermodynamicModel& model,
        double solute_moles_per_liter,
        double alpha,
        double temperature_k) noexcept;

    /**
     * @brief Full evaluation of solution saturation, precipitation, and electrolytic dissociation.
     */
    [[nodiscard]] static SolutionSaturationResult evaluate_solution(
        const SolutionThermodynamicModel& model,
        double solute_mass_g,
        double solvent_mass_g,
        double temperature_k,
        double solute_molar_mass_g_mol = 58.44);
};

} // namespace shunya::mat
