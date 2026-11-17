#include "shunya/mat/hume_rothery.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

namespace {

[[nodiscard]] std::string prototype_to_name(matdb::StructurePrototype proto) {
    switch (proto) {
    case matdb::StructurePrototype::FCC: return "FCC";
    case matdb::StructurePrototype::BCC: return "BCC";
    case matdb::StructurePrototype::HCP: return "HCP";
    case matdb::StructurePrototype::SC: return "SC";
    case matdb::StructurePrototype::DiamondCubic: return "DiamondCubic";
    case matdb::StructurePrototype::GraphiteHexagonal: return "GraphiteHexagonal";
    case matdb::StructurePrototype::Other: return "Other";
    }
    return "Unknown";
}

} // namespace

int HumeRotheryEngine::resolve_nominal_valence(const matdb::ElementRecord& elem) noexcept {
    const int z = elem.atomic_number;
    if (z == 29) return 1; // Cu
    if (z == 30) return 2; // Zn
    if (z == 13) return 3; // Al
    if (z == 26) return 2; // Fe (ferrous/metallic)
    if (z == 28) return 2; // Ni
    if (z == 22) return 4; // Ti
    if (z == 47) return 1; // Ag
    if (z == 79) return 1; // Au
    if (z == 50) return 4; // Sn

    if (elem.group.has_value()) {
        const int g = *elem.group;
        if (g == 1) return 1;
        if (g == 2) return 2;
        if (g >= 3 && g <= 10) return 2;
        if (g == 11) return 1;
        if (g == 12) return 2;
        if (g == 13) return 3;
        if (g == 14) return 4;
        if (g == 15) return 3;
    }
    return 2;
}

std::string HumeRotheryEngine::resolve_crystal_structure(const matdb::ElementRecord& elem) {
    const auto structs = matdb::CrystallographyDatabase::structures_for_element(elem.atomic_number);
    if (!structs.empty()) {
        return prototype_to_name(structs.front().prototype);
    }

    const int sg = elem.space_group_number.value_or(0);
    if (sg == 225) return "FCC";
    if (sg == 229) return "BCC";
    if (sg == 194) return "HCP";
    if (sg == 227) return "DiamondCubic";

    return "Unknown";
}

HumeRotheryEvaluation HumeRotheryEngine::evaluate_pair(
    const matdb::ElementRecord& solvent,
    const matdb::ElementRecord& solute) noexcept {
    HumeRotheryEvaluation eval;

    // 1. Atomic Radius Mismatch
    const double r_solvent = solvent.atomic_radius.value_or(1.4e-10);
    const double r_solute = solute.atomic_radius.value_or(1.4e-10);
    const double diff_r = std::abs(r_solute - r_solvent);
    const double r_ratio = diff_r / std::max(1e-15, r_solvent);
    eval.atomic_radius_mismatch_pct = r_ratio * 100.0;
    eval.size_factor_passed = (r_ratio <= 0.15);

    // 2. Crystal Structure Match
    eval.solvent_crystal_structure = resolve_crystal_structure(solvent);
    eval.solute_crystal_structure = resolve_crystal_structure(solute);
    eval.crystal_structure_matched = (eval.solvent_crystal_structure == eval.solute_crystal_structure &&
                                      eval.solvent_crystal_structure != "Unknown");

    // 3. Valence Factor
    eval.solvent_valence = resolve_nominal_valence(solvent);
    eval.solute_valence = resolve_nominal_valence(solute);
    eval.valence_difference = std::abs(eval.solvent_valence - eval.solute_valence);
    eval.valence_factor_favorable = (eval.valence_difference <= 1);

    // 4. Electrochemical Factor
    const double chi_solvent = solvent.electronegativity.value_or(1.9);
    const double chi_solute = solute.electronegativity.value_or(1.9);
    eval.electronegativity_delta = std::abs(chi_solvent - chi_solute);
    eval.electrochemical_factor_passed = (eval.electronegativity_delta <= 0.4);
    eval.intermetallic_tendency = (eval.electronegativity_delta > 0.4);

    eval.overall_solid_solubility_favored = (eval.size_factor_passed &&
                                            eval.electrochemical_factor_passed &&
                                            eval.valence_factor_favorable &&
                                            eval.crystal_structure_matched);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    // Size message
    ss << "1. Size Factor: " << eval.atomic_radius_mismatch_pct << "% mismatch ("
       << (eval.size_factor_passed ? "PASS <= 15%" : "FAIL > 15%") << ").";
    eval.rule_evaluations.push_back(ss.str());
    ss.str("");

    // Crystal structure message
    ss << "2. Crystal Structure: Solvent=" << eval.solvent_crystal_structure
       << ", Solute=" << eval.solute_crystal_structure << " ("
       << (eval.crystal_structure_matched ? "MATCH" : "MISMATCH") << ").";
    eval.rule_evaluations.push_back(ss.str());
    ss.str("");

    // Valence message
    ss << "3. Valence Factor: Solvent=" << eval.solvent_valence
       << ", Solute=" << eval.solute_valence << ", Delta=" << eval.valence_difference << " ("
       << (eval.valence_factor_favorable ? "FAVORABLE" : "LIMITED") << ").";
    eval.rule_evaluations.push_back(ss.str());
    ss.str("");

    // Electrochemical message
    ss << "4. Electrochemical Factor: Delta Chi=" << eval.electronegativity_delta << " ("
       << (eval.electrochemical_factor_passed ? "SOLID SOLUTION FAVORED" : "INTERMETALLIC TENDENCY") << ").";
    eval.rule_evaluations.push_back(ss.str());

    return eval;
}

std::vector<HumeRotheryEvaluation> HumeRotheryEngine::evaluate_alloy(
    const std::vector<const matdb::ElementRecord*>& elements) {
    std::vector<HumeRotheryEvaluation> evaluations;
    if (elements.size() < 2) return evaluations;

    const auto* solvent = elements[0];
    for (size_t i = 1; i < elements.size(); ++i) {
        if (elements[i] != nullptr && solvent != nullptr) {
            evaluations.push_back(evaluate_pair(*solvent, *elements[i]));
        }
    }
    return evaluations;
}

} // namespace shunya::mat
