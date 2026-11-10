#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/periodic_table.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

enum class StandardPhase : uint8_t {
    Gas,
    Liquid,
    Solid,
    Unknown
};

enum class ElementFamily : uint8_t {
    AlkaliMetal,
    AlkalineEarth,
    TransitionMetal,
    PostTransitionMetal,
    Metalloid,
    ReactiveNonmetal,
    NobleGas,
    Lanthanide,
    Actinide
};

struct ExtendedElementRecord {
    int atomic_number{1};
    std::string symbol;
    std::string name;
    int period{1};
    int group{1};
    char block{'s'};
    double atomic_weight_amu{1.008};
    StandardPhase standard_state{StandardPhase::Gas};
    ElementFamily family{ElementFamily::ReactiveNonmetal};
    std::string electron_configuration;
    std::vector<int> common_oxidation_states;
};

/**
 * @brief Complete 118-element IUPAC 2021 reference registry & JSON/SQLite exchange adapter.
 */
class Elements118Catalog {
public:
    static const std::vector<ExtendedElementRecord>& all_elements();
    [[nodiscard]] static const ExtendedElementRecord* find_by_z(int atomic_number) noexcept;
    [[nodiscard]] static const ExtendedElementRecord* find_by_symbol(std::string_view symbol) noexcept;

    /**
     * @brief Populates a core PeriodicTableDatabase with all 118 canonical IUPAC elements.
     */
    static void populate_database(core::domain::PeriodicTableDatabase& db);

    [[nodiscard]] static std::string to_json();
    [[nodiscard]] static std::string generate_sqlite_elements_v2_ddl();
    [[nodiscard]] static core::base::ValidationReport audit_completeness();
};

} // namespace shunya::matdb
