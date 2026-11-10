#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <vector>

namespace shunya::mat {

struct AlloyInputElement {
    int atomic_number{22};
    double fraction_pct{0.0}; // weight % or molar %, auto-normalized to 100.0
};

struct BalancedAlloyElement {
    int atomic_number{22};
    std::string symbol;
    double weight_pct{0.0};
    double molar_pct{0.0};
    double pure_density_kg_m3{4506.0};
};

struct AlloySpecification {
    std::string name;
    std::vector<BalancedAlloyElement> elements;
    double theoretical_density_kg_m3{0.0};
    double effective_molar_mass_g_mol{0.0};
    double atomic_volume_cm3_mol{0.0};
    double thermal_expansion_per_k{0.0};
    double electrical_resistivity_ohm_m{0.0};
};

/**
 * @brief Builds balanced metal alloys with auto-normalization, theoretical density, and standard presets.
 */
class AlloyBuilder {
public:
    /**
     * @brief Builds an alloy from elemental weight percentages. If total < 100%, auto-balances remainder
     *        onto the first (balance/base) element if auto_balance_first == true, or normalizes proportionally.
     */
    [[nodiscard]] static core::base::Result<AlloySpecification> build_from_weight_pct(
        std::string name,
        std::vector<AlloyInputElement> inputs,
        bool auto_balance_first = false,
        double lattice_contraction_factor = 0.0);

    [[nodiscard]] static core::base::Result<AlloySpecification> build_from_molar_pct(
        std::string name,
        std::vector<AlloyInputElement> inputs);

    // Canonical engineering alloy presets
    [[nodiscard]] static AlloySpecification preset_ti_6al_4v();
    [[nodiscard]] static AlloySpecification preset_ss_316l();
    [[nodiscard]] static AlloySpecification preset_brass_c260();
    [[nodiscard]] static std::string standard_alloys_json();
};

} // namespace shunya::mat
