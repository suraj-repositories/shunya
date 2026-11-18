#include "shunya/mat/atomic_bond_checker.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

bool AtomicBondChecker::is_nonmetal(const matdb::ElementRecord& el) noexcept {
    const int z = el.atomic_number;
    return (z == 1 || z == 2 || z == 6 || z == 7 || z == 8 || z == 9 || z == 10 ||
            z == 15 || z == 16 || z == 17 || z == 18 || z == 34 || z == 35 || z == 36 ||
            z == 53 || z == 54 || z == 85 || z == 86 || z == 117 || z == 118 ||
            (el.group && (*el.group == 17 || *el.group == 18)));
}

bool AtomicBondChecker::is_noble_gas(const matdb::ElementRecord& el) noexcept {
    const int z = el.atomic_number;
    return (z == 2 || z == 10 || z == 18 || z == 36 || z == 54 || z == 86 || z == 118 ||
            (el.group && *el.group == 18));
}

bool AtomicBondChecker::is_metal(const matdb::ElementRecord& el) noexcept {
    if (is_nonmetal(el)) return false;
    const int z = el.atomic_number;
    // Exclude classic metalloids: B (5), Si (14), Ge (32), As (33), Sb (51), Te (52)
    if (z == 5 || z == 14 || z == 32 || z == 33 || z == 51 || z == 52) return false;
    return true;
}

ElementPairBondAnalysis AtomicBondChecker::analyze_pair(
    const matdb::ElementRecord& a,
    const matdb::ElementRecord& b) {
    ElementPairBondAnalysis p;
    p.element_a = a.symbol;
    p.element_b = b.symbol;

    const double en_a = a.electronegativity.value_or(1.8);
    const double en_b = b.electronegativity.value_or(1.8);
    p.delta_electronegativity = std::abs(en_a - en_b);

    const double r_a = a.atomic_radius.has_value() ? (a.atomic_radius.value() * 1e12) : 140.0;
    const double r_b = b.atomic_radius.has_value() ? (b.atomic_radius.value() * 1e12) : 140.0;
    const double max_r = std::max(r_a, r_b);
    p.radius_difference_pct = (max_r > 0.0) ? (std::abs(r_a - r_b) / max_r) * 100.0 : 0.0;

    const bool a_nonmetal = is_nonmetal(a);
    const bool b_nonmetal = is_nonmetal(b);
    const bool a_metal = is_metal(a);
    const bool b_metal = is_metal(b);
    const bool noble = is_noble_gas(a) || is_noble_gas(b);

    const std::string struct_a = HumeRotheryEngine::resolve_crystal_structure(a);
    const std::string struct_b = HumeRotheryEngine::resolve_crystal_structure(b);
    p.crystal_structures = struct_a + " - " + struct_b;

    // Check known immiscible elemental pairs (e.g. Fe-Pb, Fe-Bi, Cu-Pb, Al-Pb)
    const bool is_fe_pb = ((a.symbol == "Fe" && b.symbol == "Pb") || (a.symbol == "Pb" && b.symbol == "Fe"));
    const bool is_fe_bi = ((a.symbol == "Fe" && b.symbol == "Bi") || (a.symbol == "Bi" && b.symbol == "Fe"));
    const bool is_cu_pb = ((a.symbol == "Cu" && b.symbol == "Pb") || (a.symbol == "Pb" && b.symbol == "Cu"));

    // Check violent hazard pairing (e.g. Alkali Metal + Halogen)
    const bool a_alkali = (a.atomic_number == 3 || a.atomic_number == 11 || a.atomic_number == 19 || a.atomic_number == 37 || a.atomic_number == 55);
    const bool b_alkali = (b.atomic_number == 3 || b.atomic_number == 11 || b.atomic_number == 19 || b.atomic_number == 37 || b.atomic_number == 55);
    const bool a_halogen = (a.atomic_number == 9 || a.atomic_number == 17 || a.atomic_number == 35 || a.atomic_number == 53);
    const bool b_halogen = (b.atomic_number == 9 || b.atomic_number == 17 || b.atomic_number == 35 || b.atomic_number == 53);

    if ((a_alkali && b_halogen) || (b_alkali && a_halogen)) {
        p.is_hazardous = true;
        p.hazard_warning = "Violent exothermic reaction hazard: Alkali metal vigorously reacts with halogen.";
    }

    // 1. Bond Classification
    if (noble) {
        p.primary_bond_type = AtomicBondType::NoneOrImmiscible;
        p.miscibility = MiscibilityState::ImmisciblePhaseSeparated;
        p.plain_language_verdict = "Inert noble gas configuration; unreactive under standard conditions.";
    } else if (a_metal && b_metal) {
        p.primary_bond_type = AtomicBondType::Metallic;

        // Hume-Rothery Miscibility Evaluation
        if (is_fe_pb || is_fe_bi || is_cu_pb || p.radius_difference_pct > 25.0) {
            p.miscibility = MiscibilityState::ImmisciblePhaseSeparated;
            std::ostringstream ss;
            ss << "Radius mismatch " << std::fixed << std::setprecision(1) << p.radius_difference_pct
               << "% (" << struct_a << " vs " << struct_b << "): Virtually immiscible in solid and liquid states.";
            p.plain_language_verdict = ss.str();
        } else if (p.radius_difference_pct <= 15.0 && struct_a == struct_b && p.delta_electronegativity <= 0.4) {
            p.miscibility = MiscibilityState::CompleteSolidSolution;
            std::ostringstream ss;
            ss << "DeltaEN = " << std::fixed << std::setprecision(2) << p.delta_electronegativity
               << " and radius mismatch " << std::setprecision(1) << p.radius_difference_pct
               << "%: Forms complete isomorphous solid solution (" << struct_a << ").";
            p.plain_language_verdict = ss.str();
        } else if (p.delta_electronegativity > 0.4) {
            p.miscibility = MiscibilityState::IntermetallicPrecipitation;
            std::ostringstream ss;
            ss << "DeltaEN = " << std::fixed << std::setprecision(2) << p.delta_electronegativity
               << " indicates high affinity for intermetallic compound phase precipitation.";
            p.plain_language_verdict = ss.str();
        } else {
            p.miscibility = MiscibilityState::LimitedSolidSolution;
            std::ostringstream ss;
            ss << "Limited solid solubility governed by crystal structure disparity ("
               << struct_a << " vs " << struct_b << ").";
            p.plain_language_verdict = ss.str();
        }
    } else if ((a_metal && b_nonmetal) || (b_metal && a_nonmetal)) {
        if (p.delta_electronegativity > 1.7) {
            p.primary_bond_type = AtomicBondType::Ionic;
            std::ostringstream ss;
            ss << "DeltaEN = " << std::fixed << std::setprecision(2) << p.delta_electronegativity
               << " indicates predominantly ionic crystal lattice formation.";
            p.plain_language_verdict = ss.str();
        } else {
            p.primary_bond_type = AtomicBondType::PolarCovalent;
            std::ostringstream ss;
            ss << "DeltaEN = " << std::fixed << std::setprecision(2) << p.delta_electronegativity
               << " indicates strong polar-covalent bonding with partial ionic character.";
            p.plain_language_verdict = ss.str();
        }
        p.miscibility = MiscibilityState::IntermetallicPrecipitation;
    } else {
        // Both nonmetals
        if (p.delta_electronegativity < 0.5) {
            p.primary_bond_type = AtomicBondType::NonpolarCovalent;
            std::ostringstream ss;
            ss << "DeltaEN = " << std::fixed << std::setprecision(2) << p.delta_electronegativity
               << " indicates nonpolar covalent electron sharing.";
            p.plain_language_verdict = ss.str();
        } else {
            p.primary_bond_type = AtomicBondType::PolarCovalent;
            std::ostringstream ss;
            ss << "DeltaEN = " << std::fixed << std::setprecision(2) << p.delta_electronegativity
               << " indicates polar covalent molecular bonding.";
            p.plain_language_verdict = ss.str();
        }
        p.miscibility = MiscibilityState::CompleteSolidSolution;
    }

    return p;
}

AtomicBondReport AtomicBondChecker::analyze_mixture(
    const std::vector<const matdb::ElementRecord*>& elements,
    const std::vector<double>& mole_fractions,
    const std::string& scenario) {
    (void)mole_fractions;
    AtomicBondReport rep;
    rep.is_formation_scenario_compatible = true;

    if (elements.empty()) return rep;

    const size_t n = elements.size();
    bool has_immiscible = false;
    bool has_ionic = false;
    bool has_metallic = false;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            auto pair = analyze_pair(*elements[i], *elements[j]);
            if (pair.miscibility == MiscibilityState::ImmisciblePhaseSeparated) has_immiscible = true;
            if (pair.primary_bond_type == AtomicBondType::Ionic) has_ionic = true;
            if (pair.primary_bond_type == AtomicBondType::Metallic) has_metallic = true;
            if (pair.is_hazardous) {
                rep.chemical_warnings.push_back(pair.hazard_warning);
            }
            rep.pairwise_analyses.push_back(std::move(pair));
        }
    }

    // Check scenario conflicts
    if (scenario == "Alloy") {
        if (has_ionic) {
            rep.is_formation_scenario_compatible = false;
            rep.chemical_warnings.push_back("Scenario mismatch: Elements form an ionic compound rather than a metallic alloy. Switch to 'ChemicalCompound'.");
            rep.recommendations.push_back("Change Formation Scenario to 'ChemicalCompound' or 'Solution'.");
        } else if (has_immiscible) {
            rep.chemical_warnings.push_back("Constituents are solid-state immiscible; alloy will experience severe phase separation or macrosegregation.");
            rep.recommendations.push_back("Consider 'CompositeMaterial' with powder metallurgy consolidation or rapid solidification.");
        }
        rep.primary_overall_bond_type = "Metallic";
        rep.overall_miscibility = has_immiscible ? "Immiscible / Phase-Separated" : "Solid Solution";
    } else if (scenario == "ChemicalCompound") {
        rep.primary_overall_bond_type = has_ionic ? "Ionic" : "Polar Covalent";
        rep.overall_miscibility = "Stoichiometric Compound";

        // Provide Stoichiometric Suggestions
        bool has_fe = false, has_o = false, has_na = false, has_cl = false, has_h = false;
        for (const auto* el : elements) {
            if (el->symbol == "Fe") has_fe = true;
            else if (el->symbol == "O") has_o = true;
            else if (el->symbol == "Na") has_na = true;
            else if (el->symbol == "Cl") has_cl = true;
            else if (el->symbol == "H") has_h = true;
        }

        if (has_fe && has_o) {
            rep.stoichiometric_suggestions.push_back("Fe2O3 (Iron(III) oxide / Hematite: 40% Fe, 60% O)");
            rep.stoichiometric_suggestions.push_back("Fe3O4 (Iron(II,III) oxide / Magnetite: 42.9% Fe, 57.1% O)");
        }
        if (has_na && has_cl) {
            rep.stoichiometric_suggestions.push_back("NaCl (Sodium chloride / Halite: 50% Na, 50% Cl)");
        }
        if (has_h && has_o) {
            rep.stoichiometric_suggestions.push_back("H2O (Water: 66.7% H, 33.3% O)");
        }
    } else {
        rep.primary_overall_bond_type = has_metallic ? "Metallic" : (has_ionic ? "Ionic" : "Covalent");
        rep.overall_miscibility = has_immiscible ? "Phase-Separated Mixture" : "Homogeneous";
    }

    return rep;
}

} // namespace shunya::mat
