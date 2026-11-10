#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/matdb/electron_config_parser.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shunya::matdb {

struct OrbitalValidationResult {
    int atomic_number{1};
    ElectronConfiguration parsed;
    bool matches_madelung_aufbau{true};
    bool is_known_exception{false};
    int valence_electrons{1};
    int unpaired_electrons{1};
    double spin_multiplicity{2.0};
};

/**
 * @brief Aufbau principle (Madelung n+l rule), Hund's rule, and 118-element orbital validator.
 */
class OrbitalFillingEngine {
public:
    /**
     * @brief Canonical Madelung filling sequence as (n, l) pairs up to 7p.
     */
    [[nodiscard]] static const std::vector<std::pair<int, int>>& madelung_order();

    /**
     * @brief Predicts ground-state electron configuration strictly from Madelung's (n+l, n) rule.
     */
    [[nodiscard]] static core::base::Result<ElectronConfiguration> predict_madelung(int atomic_number);

    /**
     * @brief Parses and validates a ground-state electron configuration for element Z.
     */
    [[nodiscard]] static core::base::Result<OrbitalValidationResult> validate(int atomic_number,
                                                                              std::string_view config_str);

    /**
     * @brief Audits all 118 elements in Elements118Catalog for quantum consistency.
     */
    [[nodiscard]] static core::base::ValidationReport audit_all_118_elements();
};

} // namespace shunya::matdb
