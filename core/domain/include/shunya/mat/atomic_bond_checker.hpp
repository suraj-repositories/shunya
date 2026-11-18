#pragma once

#include "shunya/matdb/element_database.hpp"
#include "shunya/mat/hume_rothery.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::mat {

enum class AtomicBondType : uint8_t {
    Ionic,
    PolarCovalent,
    NonpolarCovalent,
    Metallic,
    CoordinateCovalent,
    NoneOrImmiscible
};

[[nodiscard]] constexpr std::string_view atomic_bond_type_to_string(AtomicBondType b) noexcept {
    switch (b) {
    case AtomicBondType::Ionic: return "Ionic";
    case AtomicBondType::PolarCovalent: return "Polar Covalent";
    case AtomicBondType::NonpolarCovalent: return "Nonpolar Covalent";
    case AtomicBondType::Metallic: return "Metallic";
    case AtomicBondType::CoordinateCovalent: return "Coordinate Covalent";
    case AtomicBondType::NoneOrImmiscible: return "None / Immiscible";
    }
    return "Unknown";
}

enum class MiscibilityState : uint8_t {
    CompleteSolidSolution,
    LimitedSolidSolution,
    IntermetallicPrecipitation,
    ImmisciblePhaseSeparated
};

[[nodiscard]] constexpr std::string_view miscibility_state_to_string(MiscibilityState m) noexcept {
    switch (m) {
    case MiscibilityState::CompleteSolidSolution: return "Complete Solid Solution";
    case MiscibilityState::LimitedSolidSolution: return "Limited Solid Solution";
    case MiscibilityState::IntermetallicPrecipitation: return "Intermetallic Precipitation";
    case MiscibilityState::ImmisciblePhaseSeparated: return "Immiscible / Phase-Separated";
    }
    return "Unknown";
}

struct ElementPairBondAnalysis {
    std::string element_a;
    std::string element_b;
    double delta_electronegativity{0.0};
    double radius_difference_pct{0.0};
    AtomicBondType primary_bond_type{AtomicBondType::Metallic};
    MiscibilityState miscibility{MiscibilityState::CompleteSolidSolution};
    bool is_hazardous{false};
    std::string hazard_warning;
    std::string plain_language_verdict;
    std::string crystal_structures;
};

struct AtomicBondReport {
    bool is_formation_scenario_compatible{true};
    std::string primary_overall_bond_type{"Metallic"};
    std::string overall_miscibility{"Complete Solid Solution"};
    std::vector<ElementPairBondAnalysis> pairwise_analyses;
    std::vector<std::string> stoichiometric_suggestions;
    std::vector<std::string> chemical_warnings;
    std::vector<std::string> recommendations;
};

class AtomicBondChecker {
public:
    [[nodiscard]] static bool is_nonmetal(const matdb::ElementRecord& el) noexcept;
    [[nodiscard]] static bool is_metal(const matdb::ElementRecord& el) noexcept;
    [[nodiscard]] static bool is_noble_gas(const matdb::ElementRecord& el) noexcept;

    [[nodiscard]] static ElementPairBondAnalysis analyze_pair(
        const matdb::ElementRecord& a,
        const matdb::ElementRecord& b);

    [[nodiscard]] static AtomicBondReport analyze_mixture(
        const std::vector<const matdb::ElementRecord*>& elements,
        const std::vector<double>& mole_fractions,
        const std::string& scenario);
};

} // namespace shunya::mat
