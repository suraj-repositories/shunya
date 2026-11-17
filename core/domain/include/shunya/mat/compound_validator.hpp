#pragma once

#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace shunya::mat {

enum class BondCharacter : uint8_t {
    NonPolarCovalent,
    PolarCovalent,
    Ionic,
    Metallic
};

[[nodiscard]] constexpr std::string_view bond_character_to_string(BondCharacter bc) noexcept {
    switch (bc) {
    case BondCharacter::NonPolarCovalent: return "Non-Polar Covalent";
    case BondCharacter::PolarCovalent:    return "Polar Covalent";
    case BondCharacter::Ionic:            return "Ionic";
    case BondCharacter::Metallic:         return "Metallic";
    }
    return "Unknown";
}

struct StoichiometricValidationResult {
    bool is_stoichiometrically_balanced{true};
    bool is_valence_saturated{true};
    BondCharacter bond_character{BondCharacter::PolarCovalent};
    double pauling_electronegativity_delta{0.0};
    double ionic_character_pct{0.0};
    std::string empirical_formula;
    int net_formal_charge{0};
    std::map<std::string, int> estimated_oxidation_states;
    std::vector<std::string> validation_messages;
};

/**
 * @brief Scientific validator for chemical compound stoichiometry, octet/valence saturation,
 *        and Pauling electronegativity delta bond-character verification.
 * (Roadmap task PH4-03.01.002)
 */
class CompoundValidator {
public:
    [[nodiscard]] static StoichiometricValidationResult validate_compound(
        const std::vector<NormalizedConstituent>& constituents,
        const std::vector<const matdb::ElementRecord*>& records);

    /**
     * @brief Computes Pauling ionic bond fraction percentage:
     *        P_ionic = (1 - exp(-0.25 * (Delta Chi)^2)) * 100%
     */
    [[nodiscard]] static double compute_pauling_ionic_pct(double delta_chi) noexcept;

    /**
     * @brief Formulates the standard chemical empirical formula with integer stoichiometric ratios (e.g. "H2O", "LiFePO4").
     */
    [[nodiscard]] static std::string synthesize_empirical_formula(
        const std::vector<NormalizedConstituent>& constituents);

    /**
     * @brief Evaluates whether constituents can achieve electroneutral valence shell saturation.
     */
    [[nodiscard]] static bool verify_valence_shell_saturation(
        const std::vector<const matdb::ElementRecord*>& records,
        const std::vector<double>& mole_fractions);
};

} // namespace shunya::mat
