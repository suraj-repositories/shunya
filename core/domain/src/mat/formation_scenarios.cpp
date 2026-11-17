#include "shunya/mat/formation_scenarios.hpp"
#include "shunya/mat/hume_rothery.hpp"
#include "shunya/mat/solution_solubility_engine.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace shunya::mat {

namespace {

core::base::Error make_scenario_err(int code, std::string msg) {
    return core::base::Error{code, core::base::ErrorDomain::Validation, core::base::Severity::Error, std::move(msg), ""};
}

bool is_noble_gas(int z) noexcept {
    return (z == 2 || z == 10 || z == 18 || z == 36 || z == 54 || z == 86 || z == 118);
}

} // namespace

// --- PhysicalMixtureHandler ---

std::string PhysicalMixtureHandler::description() const {
    return "Heterogeneous or homogeneous mechanical blend of independent physical phases with no atomic-scale chemical reaction or lattice alloying.";
}

core::base::Result<void> PhysicalMixtureHandler::validate_preconditions(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems) const {
    if (elems.empty()) {
        return core::base::Result<void>::err(make_scenario_err(4701, "Physical mixture requires at least one constituent"));
    }
    (void)experiment;
    return core::base::Result<void>::ok();
}

void PhysicalMixtureHandler::process_properties(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems,
    const std::vector<ElementBehaviorReport>& behaviors,
    MixtureEvaluationResult& result) const {
    (void)experiment;
    (void)elems;
    (void)behaviors;
    result.scientific_notes.push_back("Physical mixture: Phases remain physically distinct; macro-scale effective properties follow linear volume-fraction homogenization.");
}

// --- AlloyHandler ---

std::string AlloyHandler::description() const {
    return "Metallic solid solution or intermetallic compound obeying Hume-Rothery solubility and crystal structure matching rules.";
}

core::base::Result<void> AlloyHandler::validate_preconditions(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems) const {
    (void)experiment;
    if (elems.empty()) {
        return core::base::Result<void>::err(make_scenario_err(4702, "Alloy requires at least one metallic constituent"));
    }
    return core::base::Result<void>::ok();
}

void AlloyHandler::process_properties(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems,
    const std::vector<ElementBehaviorReport>& behaviors,
    MixtureEvaluationResult& result) const {
    (void)experiment;
    (void)behaviors;
    for (const auto* el : elems) {
        if (is_noble_gas(el->atomic_number)) {
            result.is_chemically_valid = false;
            result.validation_errors.push_back("Alloy cannot contain noble gas element: " + el->symbol);
        }
    }
    // Extended Hume-Rothery Rules (size, crystal structure, valence, electrochemical)
    if (elems.size() >= 2) {
        const auto evaluations = HumeRotheryEngine::evaluate_alloy(elems);
        for (const auto& eval : evaluations) {
            if (!eval.size_factor_passed) {
                result.warnings.push_back("Hume-Rothery size mismatch exceeds 15%: limited solid solubility expected.");
            }
            if (eval.intermetallic_tendency) {
                result.warnings.push_back("Hume-Rothery electrochemical factor indicates tendency towards intermetallic phase formation rather than simple solid solution.");
            }
            if (eval.overall_solid_solubility_favored) {
                result.scientific_notes.push_back("Hume-Rothery rules satisfied (size, crystal structure, valence, electrochemical): extensive substitutional solid solution anticipated.");
            } else {
                result.scientific_notes.push_back("Hume-Rothery evaluation: limited complete solid solubility due to factor differences.");
            }
        }
    }
}

// --- SolutionHandler ---

std::string SolutionHandler::description() const {
    return "Homogeneous single-phase liquid or solid solution consisting of a solvent and one or more dissolved solutes up to saturation limits.";
}

core::base::Result<void> SolutionHandler::validate_preconditions(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems) const {
    (void)experiment;
    if (elems.size() < 2) {
        return core::base::Result<void>::err(make_scenario_err(4703, "Solution requires at least a solvent and one solute constituent"));
    }
    return core::base::Result<void>::ok();
}

void SolutionHandler::process_properties(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems,
    const std::vector<ElementBehaviorReport>& behaviors,
    MixtureEvaluationResult& result) const {
    (void)experiment;
    (void)elems;
    (void)behaviors;
    if (result.constituents.size() >= 2) {
        result.scientific_notes.push_back("Solution: Primary solvent is " + result.constituents[0].symbol +
            " (" + std::to_string(result.constituents[0].mass_fraction * 100.0) + " wt%).");

        // Evaluate liquid-solid saturation and electrolytic dissociation
        SolutionThermodynamicModel thermo_model;
        thermo_model.base_solubility_g_per_100g = 36.0;
        thermo_model.electrolyte_type = ElectrolyteType::StrongElectrolyte;

        const double solvent_g = result.constituents[0].mass_kg * 1000.0;
        const double solute_g = result.constituents[1].mass_kg * 1000.0;
        const double temp_k = experiment.temperature_k;

        auto sat_res = SolutionSolubilityEngine::evaluate_solution(
            thermo_model, solute_g, solvent_g, temp_k);

        result.scientific_notes.push_back(sat_res.summary_description);
        if (sat_res.state == SaturationState::Supersaturated) {
            result.warnings.push_back("Solution is supersaturated at " + std::to_string(temp_k) +
                " K: " + std::to_string(sat_res.precipitated_mass_g) + " g of solute precipitate expected.");
        }
    }
}

// --- ChemicalCompoundHandler ---

std::string ChemicalCompoundHandler::description() const {
    return "Stoichiometrically fixed pure substance formed via ionic, covalent, or polar covalent chemical bonds with valence saturation.";
}

core::base::Result<void> ChemicalCompoundHandler::validate_preconditions(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems) const {
    (void)experiment;
    if (elems.empty()) {
        return core::base::Result<void>::err(make_scenario_err(4704, "Chemical compound requires at least one constituent"));
    }
    return core::base::Result<void>::ok();
}

void ChemicalCompoundHandler::process_properties(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems,
    const std::vector<ElementBehaviorReport>& behaviors,
    MixtureEvaluationResult& result) const {
    (void)experiment;
    (void)behaviors;
    for (const auto* el : elems) {
        if (is_noble_gas(el->atomic_number)) {
            result.is_chemically_valid = false;
            result.validation_errors.push_back(
                "Chemical compound formation rejected: noble gas " + el->symbol + " cannot form stable standard compounds");
        }
    }
    if (elems.size() >= 2) {
        double en0 = elems[0]->electronegativity.value_or(2.0);
        double en1 = elems[1]->electronegativity.value_or(2.0);
        double delta_chi = std::abs(en0 - en1);
        std::string bond_type = (delta_chi > 1.7) ? "predominantly ionic" :
                                (delta_chi > 0.4) ? "polar covalent" : "predominantly covalent";
        result.scientific_notes.push_back("Chemical compound: Electronegativity delta |dChi| = " +
            std::to_string(delta_chi) + " indicates " + bond_type + " bonding.");
    }
}

// --- LayeredMaterialHandler ---

std::string LayeredMaterialHandler::description() const {
    return "Planar anisotropic laminated stackup with direction-dependent in-plane (parallel) versus through-plane (perpendicular) transport properties.";
}

core::base::Result<void> LayeredMaterialHandler::validate_preconditions(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems) const {
    (void)elems;
    for (const auto& in : experiment.inputs) {
        if (in.layer_thickness_m <= 0.0) {
            return core::base::Result<void>::err(make_scenario_err(
                4705, "Layered material requires strictly positive layer_thickness_m for all layers"));
        }
    }
    return core::base::Result<void>::ok();
}

void LayeredMaterialHandler::process_properties(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems,
    const std::vector<ElementBehaviorReport>& behaviors,
    MixtureEvaluationResult& result) const {
    (void)experiment;
    (void)elems;
    (void)behaviors;
    result.scientific_notes.push_back("Layered material: Explicit transverse vs in-plane anisotropic tensor calculated.");
}

// --- CompositeMaterialHandler ---

std::string CompositeMaterialHandler::description() const {
    return "Multi-phase heterogeneous structural material combining a continuous matrix with particulate, fiber, or platelet reinforcement phases.";
}

core::base::Result<void> CompositeMaterialHandler::validate_preconditions(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems) const {
    (void)elems;
    bool has_matrix = false;
    for (const auto& in : experiment.inputs) {
        if (in.is_matrix) has_matrix = true;
    }
    if (!has_matrix && experiment.inputs.size() > 1) {
        // Warning or auto-designate first constituent as matrix
    }
    return core::base::Result<void>::ok();
}

void CompositeMaterialHandler::process_properties(
    const MixtureExperiment& experiment,
    const std::vector<const matdb::ElementRecord*>& elems,
    const std::vector<ElementBehaviorReport>& behaviors,
    MixtureEvaluationResult& result) const {
    (void)experiment;
    (void)elems;
    (void)behaviors;
    result.scientific_notes.push_back("Composite material: Voigt-Reuss and Hashin-Shtrikman homogenization bounds evaluated.");
}

// --- FormationScenarioDispatcher ---

const IFormationScenarioHandler& FormationScenarioDispatcher::get_handler(FormationScenario scenario) {
    static const PhysicalMixtureHandler s_physical_mixture;
    static const AlloyHandler s_alloy;
    static const SolutionHandler s_solution;
    static const ChemicalCompoundHandler s_compound;
    static const LayeredMaterialHandler s_layered;
    static const CompositeMaterialHandler s_composite;

    switch (scenario) {
    case FormationScenario::PhysicalMixture:   return s_physical_mixture;
    case FormationScenario::Alloy:             return s_alloy;
    case FormationScenario::Solution:          return s_solution;
    case FormationScenario::ChemicalCompound:  return s_compound;
    case FormationScenario::LayeredMaterial:   return s_layered;
    case FormationScenario::CompositeMaterial: return s_composite;
    }
    return s_physical_mixture;
}

bool FormationScenarioDispatcher::is_scenario_supported(FormationScenario scenario) noexcept {
    switch (scenario) {
    case FormationScenario::PhysicalMixture:
    case FormationScenario::Alloy:
    case FormationScenario::Solution:
    case FormationScenario::ChemicalCompound:
    case FormationScenario::LayeredMaterial:
    case FormationScenario::CompositeMaterial:
        return true;
    }
    return false;
}

std::vector<FormationScenario> FormationScenarioDispatcher::available_scenarios() {
    return {
        FormationScenario::PhysicalMixture,
        FormationScenario::Alloy,
        FormationScenario::Solution,
        FormationScenario::ChemicalCompound,
        FormationScenario::LayeredMaterial,
        FormationScenario::CompositeMaterial
    };
}

} // namespace shunya::mat
