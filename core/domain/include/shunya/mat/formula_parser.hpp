#pragma once

#include "shunya/core/base/result.hpp"

#include <map>
#include <string>
#include <string_view>

namespace shunya::mat {

struct FormulaComposition {
    std::string raw_formula;
    std::map<int, int> counts_by_z; // Z -> stoichiometric count
    double molecular_weight_g_mol{0.0};

    [[nodiscard]] int count_of(std::string_view symbol) const noexcept;
    [[nodiscard]] int total_atom_count() const noexcept;
    [[nodiscard]] std::map<std::string, double> mass_fractions() const;
    [[nodiscard]] std::string hill_formula() const;
};

/**
 * @brief Recursive-descent stoichiometric formula parser with nested groups and dot-hydrate multipliers.
 */
class FormulaParser {
public:
    [[nodiscard]] static core::base::Result<FormulaComposition> parse(std::string_view formula);
};

} // namespace shunya::mat
