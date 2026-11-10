#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/periodic_table.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

enum class DecayMode : uint8_t {
    Stable,
    BetaMinus,
    BetaPlus,
    ElectronCapture,
    Alpha,
    SpontaneousFission,
    IsomericTransition,
    ProtonEmission,
    NeutronEmission
};

enum class TimeUnit : uint8_t {
    Nanoseconds,
    Microseconds,
    Milliseconds,
    Seconds,
    Minutes,
    Hours,
    Days,
    Years
};

/**
 * @brief Converts a half-life duration from an arbitrary time unit into SI seconds.
 */
[[nodiscard]] double half_life_to_seconds(double value, TimeUnit unit) noexcept;

[[nodiscard]] constexpr std::string_view decay_mode_to_string(DecayMode mode) noexcept {
    switch (mode) {
    case DecayMode::Stable:             return "Stable";
    case DecayMode::BetaMinus:          return "BetaMinus";
    case DecayMode::BetaPlus:           return "BetaPlus";
    case DecayMode::ElectronCapture:    return "ElectronCapture";
    case DecayMode::Alpha:              return "Alpha";
    case DecayMode::SpontaneousFission: return "SpontaneousFission";
    case DecayMode::IsomericTransition: return "IsomericTransition";
    case DecayMode::ProtonEmission:     return "ProtonEmission";
    case DecayMode::NeutronEmission:    return "NeutronEmission";
    }
    return "Stable";
}

struct IsotopeRecord {
    int atomic_number{1};
    int mass_number{1};
    double exact_mass_amu{1.007825};
    double abundance_pct{100.0};
    bool is_stable{true};
    std::optional<double> half_life_seconds;
    std::string spin_parity;
    std::vector<DecayMode> decay_modes;
    std::vector<std::string> decay_daughters;
};

/**
 * @brief Registry of natural terrestrial isotopes and key synthetic/radiogenic nuclides.
 */
class IsotopeRepository {
public:
    static const std::vector<IsotopeRecord>& all_isotopes();
    [[nodiscard]] static std::vector<IsotopeRecord> isotopes_for_element(int atomic_number);
    [[nodiscard]] static const IsotopeRecord* find_isotope(int atomic_number, int mass_number) noexcept;
    [[nodiscard]] static double natural_abundance_sum(int atomic_number) noexcept;

    /**
     * @brief Populates isotopes into matching PeriodicElement entries of db.
     */
    static void populate_isotopes(core::domain::PeriodicTableDatabase& db);

    [[nodiscard]] static std::string to_json();
    [[nodiscard]] static std::string generate_sqlite_isotopes_ddl();
    [[nodiscard]] static core::base::ValidationReport audit_abundances();
};

} // namespace shunya::matdb
