#pragma once

#include "shunya/matdb/crystal_database.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace shunya::mat {

struct HumeRotheryEvaluation {
    bool overall_solid_solubility_favored{true};

    // 1. Size factor (|r_solute - r_solvent| / r_solvent < 15%)
    double atomic_radius_mismatch_pct{0.0};
    bool size_factor_passed{true};

    // 2. Crystal structure factor
    std::string solvent_crystal_structure{"Unknown"};
    std::string solute_crystal_structure{"Unknown"};
    bool crystal_structure_matched{true};

    // 3. Valence factor (minimal difference in common valence states)
    int solvent_valence{1};
    int solute_valence{1};
    int valence_difference{0};
    bool valence_factor_favorable{true};

    // 4. Electrochemical factor (Delta Chi <= 0.4 favors solution, > 0.4 favors intermetallic compound)
    double electronegativity_delta{0.0};
    bool electrochemical_factor_passed{true};
    bool intermetallic_tendency{false};

    std::vector<std::string> rule_evaluations;
};

/**
 * @brief Extended Hume-Rothery Rules Engine for alloy solid solubility evaluation.
 * Evaluates:
 * 1. Atomic size mismatch (< 15%).
 * 2. Crystal structure isomorphism (FCC/BCC/HCP matching).
 * 3. Relative valence factor (|Delta V| <= 1).
 * 4. Electrochemical factor (Pauling Delta Chi <= 0.4 vs > 0.4 intermetallic tendency).
 * (Roadmap task PH4-03.01.004)
 */
class HumeRotheryEngine {
public:
    [[nodiscard]] static HumeRotheryEvaluation evaluate_pair(
        const matdb::ElementRecord& solvent,
        const matdb::ElementRecord& solute) noexcept;

    [[nodiscard]] static std::vector<HumeRotheryEvaluation> evaluate_alloy(
        const std::vector<const matdb::ElementRecord*>& elements);

    /**
     * @brief Resolves primary nominal valence for metallic solid-solution consideration.
     */
    [[nodiscard]] static int resolve_nominal_valence(const matdb::ElementRecord& elem) noexcept;

    /**
     * @brief Resolves dominant crystal structure prototype string for an element.
     */
    [[nodiscard]] static std::string resolve_crystal_structure(const matdb::ElementRecord& elem);
};

} // namespace shunya::mat
