#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

struct ThermodynamicStateRecord {
    std::string id;                 // e.g. "O2(g)", "Fe(s)", "C(diamond)"
    int atomic_number{1};
    std::string formula_state;      // e.g. "gas", "solid (BCC)", "solid (diamond)"
    bool is_reference_state{true};
    double delta_f_h_298_kj_mol{0.0};
    double delta_f_g_298_kj_mol{0.0};
    double s0_298_j_mol_k{0.0};
    double cp_298_j_mol_k{0.0};
};

/**
 * @brief Registry of CODATA / NIST-JANAF standard thermodynamic state quantities at 298.15 K (1 bar).
 */
class ThermodynamicStateRegistry {
public:
    static const std::vector<ThermodynamicStateRecord>& all_records();
    [[nodiscard]] static const ThermodynamicStateRecord* find_by_id(std::string_view id) noexcept;
    [[nodiscard]] static const ThermodynamicStateRecord* reference_state_for_element(int atomic_number) noexcept;
    [[nodiscard]] static std::vector<ThermodynamicStateRecord> records_for_element(int atomic_number);
    [[nodiscard]] static std::string to_json();
    [[nodiscard]] static core::base::ValidationReport audit_reference_states();
};

} // namespace shunya::matdb
