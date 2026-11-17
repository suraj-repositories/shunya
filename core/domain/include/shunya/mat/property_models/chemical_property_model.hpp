#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::mat {

enum class ReactivityRating : uint8_t {
    Inert,
    Low,
    Moderate,
    High,
    ExtremelyReactive
};

[[nodiscard]] constexpr std::string_view reactivity_rating_to_string(ReactivityRating r) noexcept {
    switch (r) {
    case ReactivityRating::Inert:             return "Inert";
    case ReactivityRating::Low:               return "Low";
    case ReactivityRating::Moderate:          return "Moderate";
    case ReactivityRating::High:              return "High";
    case ReactivityRating::ExtremelyReactive: return "Extremely Reactive";
    }
    return "Unknown";
}

enum class CorrosionResistanceClass : uint8_t {
    Immune,           // e.g. Gold, Platinum
    Passivated,       // e.g. Stainless steel, Titanium, Aluminum
    Moderate,         // e.g. Copper, Brass
    Susceptible,      // e.g. Mild carbon steel
    RapidlyAttacked   // e.g. Alkali metals, Magnesium in salt spray
};

[[nodiscard]] constexpr std::string_view corrosion_class_to_string(CorrosionResistanceClass c) noexcept {
    switch (c) {
    case CorrosionResistanceClass::Immune:         return "Immune";
    case CorrosionResistanceClass::Passivated:     return "Passivated";
    case CorrosionResistanceClass::Moderate:       return "Moderate";
    case CorrosionResistanceClass::Susceptible:    return "Susceptible";
    case CorrosionResistanceClass::RapidlyAttacked:return "Rapidly Attacked";
    }
    return "Unknown";
}

struct ChemicalElementConstituent {
    std::string symbol;
    int atomic_number{0};
    int stoichiometric_coefficient{1};
    double mass_fraction{0.0};
    double mole_fraction{0.0};
    int nominal_oxidation_state{0};
};

/**
 * @brief Structured Chemical Property Model:
 *        Chemical formula, stoichiometry, oxidation states, reactivity ratings,
 *        corrosion resistance, and environmental stability metrics.
 * (Roadmap task PH4-03.02.001)
 */
struct ChemicalPropertyModel {
    std::string chemical_formula{"Cu"};
    std::string iupac_name{"Copper"};
    std::vector<ChemicalElementConstituent> constituents;
    double molecular_weight_g_mol{63.546};

    // Reactivity & Environmental Stability
    ReactivityRating reactivity{ReactivityRating::Low};
    CorrosionResistanceClass corrosion_resistance{CorrosionResistanceClass::Moderate};
    double electronegativity_difference{0.0};
    double acid_resistance_rating{0.8};       // [0..1]
    double alkali_resistance_rating{0.9};     // [0..1]
    double oxidation_resistance_index{0.85};   // [0..1]
    bool flammable{false};
    bool water_reactive{false};
    bool toxic{false};

    [[nodiscard]] bool is_noble() const noexcept;
    [[nodiscard]] bool is_chemically_balanced() const noexcept;
    [[nodiscard]] int net_oxidation_sum() const noexcept;
    [[nodiscard]] std::string to_summary_string() const;
};

} // namespace shunya::mat
