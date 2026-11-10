#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

struct ShomateSegment {
    double t_min_k{298.15};
    double t_max_k{6000.0};
    std::string phase{"gas"};
    double A{0.0};
    double B{0.0};
    double C{0.0};
    double D{0.0};
    double E{0.0};
    double F{0.0};
    double G{0.0};
    double H{0.0};

    /**
     * @brief Evaluates molar heat capacity Cp(T) [J/(mol*K)] using t = T/1000.
     */
    [[nodiscard]] double eval_cp_j_mol_k(double temp_k) const noexcept;

    /**
     * @brief Evaluates standard enthalpy change H(T) - H(298.15 K) [kJ/mol].
     */
    [[nodiscard]] double eval_delta_h_kj_mol(double temp_k) const noexcept;

    /**
     * @brief Evaluates standard molar entropy S(T) [J/(mol*K)].
     */
    [[nodiscard]] double eval_s_j_mol_k(double temp_k) const noexcept;
};

struct ShomateSpecies {
    std::string id;
    int atomic_number{7};
    std::vector<ShomateSegment> segments;
};

/**
 * @brief NIST Chemistry WebBook 7-term Shomate thermodynamic polynomial catalog and evaluator.
 */
class ShomateEvaluator {
public:
    static const std::vector<ShomateSpecies>& all_species();
    [[nodiscard]] static const ShomateSpecies* find_by_id(std::string_view id) noexcept;

    [[nodiscard]] static core::base::Result<double> evaluate_cp_j_mol_k(std::string_view species_id,
                                                                          double temp_k);
    [[nodiscard]] static core::base::Result<double> evaluate_specific_heat_j_kg_k(std::string_view species_id,
                                                                                  double temp_k);
    [[nodiscard]] static core::base::Result<double> evaluate_delta_h_kj_mol(std::string_view species_id,
                                                                              double temp_k);
    [[nodiscard]] static core::base::Result<double> evaluate_entropy_j_mol_k(std::string_view species_id,
                                                                               double temp_k);

    [[nodiscard]] static std::string to_json();
    [[nodiscard]] static core::base::ValidationReport audit_segment_continuity(double max_rel_jump_pct = 0.5);
};

} // namespace shunya::matdb
