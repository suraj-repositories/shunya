#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/periodic_table.hpp"

#include <optional>
#include <string>
#include <vector>

namespace shunya::matdb {

struct ElectronegativityScales {
    std::optional<double> pauling;
    std::optional<double> allen;
    std::optional<double> mulliken_ev;
};

struct AtomicRadiiPm {
    std::optional<double> empirical_pm;
    std::optional<double> covalent_single_pm;
    std::optional<double> covalent_double_pm;
    std::optional<double> covalent_triple_pm;
    std::optional<double> vdw_pm;
};

struct ExtendedPhysicochemicalRecord {
    int atomic_number{1};
    ElectronegativityScales electronegativity;
    AtomicRadiiPm radii_pm;
};

/**
 * @brief Registry of multi-scale electronegativities and atomic radii (pm) for Z=1..118.
 */
class PeriodicTableExtended {
public:
    static const std::vector<ExtendedPhysicochemicalRecord>& all_records();
    [[nodiscard]] static const ExtendedPhysicochemicalRecord* find_by_z(int atomic_number) noexcept;

    /**
     * @brief Populates Pauling electronegativity onto existing elements in db.
     */
    static void populate_electronegativity(core::domain::PeriodicTableDatabase& db);

    [[nodiscard]] static std::string to_json();
    [[nodiscard]] static core::base::ValidationReport audit_consistency();
};

} // namespace shunya::matdb
