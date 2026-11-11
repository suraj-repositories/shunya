#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/mat/element_behavior.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::mat {

enum class FormationScenario : uint8_t {
    PhysicalMixture,
    Alloy,
    Solution,
    ChemicalCompound,
    LayeredMaterial,
    CompositeMaterial
};

[[nodiscard]] constexpr std::string_view formation_scenario_to_string(FormationScenario s) noexcept {
    switch (s) {
    case FormationScenario::PhysicalMixture:  return "PhysicalMixture";
    case FormationScenario::Alloy:            return "Alloy";
    case FormationScenario::Solution:         return "Solution";
    case FormationScenario::ChemicalCompound: return "ChemicalCompound";
    case FormationScenario::LayeredMaterial:  return "LayeredMaterial";
    case FormationScenario::CompositeMaterial:return "CompositeMaterial";
    }
    return "PhysicalMixture";
}

[[nodiscard]] core::base::Result<FormationScenario> formation_scenario_from_string(std::string_view s);

enum class QuantityInputBasis : uint8_t {
    MassGrams,
    MassFractionPct,
    MoleCount,
    MoleFractionPct,
    VolumeFractionPct
};

struct MixtureConstituentInput {
    std::string symbol_or_id; // Element symbol (e.g. "Cu", "Li") or Material UUID
    QuantityInputBasis basis{QuantityInputBasis::MassFractionPct};
    double amount{0.0};       // g, %, mol, or vol%
    double layer_thickness_m{1e-3}; // Used when FormationScenario == LayeredMaterial
    bool is_matrix{false};          // Used when FormationScenario == CompositeMaterial
};

struct NormalizedConstituent {
    std::string symbol;
    std::string name;
    int atomic_number{0};
    double mass_fraction{0.0};   // 0..1
    double mole_fraction{0.0};   // 0..1
    double volume_fraction{0.0}; // 0..1
    double mass_kg{0.0};
    double moles{0.0};
    double layer_thickness_m{1e-3};
    std::optional<double> pure_density_kg_m3;
    std::optional<double> pure_melting_point_k;
    std::optional<double> pure_thermal_conductivity;
    std::optional<double> pure_electrical_conductivity;
    std::optional<double> pure_specific_heat;
    std::optional<double> pure_young_modulus_pa;
    std::optional<double> pure_thermal_expansion;
};

struct ElementComparisonRow {
    std::string property_name;
    std::string unit;
    std::optional<double> mixture_value;
    std::vector<std::pair<std::string, std::optional<double>>> constituent_values; // (symbol, val)
    EpistemicStatus epistemic{EpistemicStatus::ApproximateSimulation};
};

struct MixtureEvaluationResult {
    bool is_chemically_valid{true};
    bool has_warnings{false};
    EpistemicStatus overall_epistemic{EpistemicStatus::ApproximateSimulation};

    std::vector<NormalizedConstituent> constituents;
    double total_mass_kg{0.0};
    double effective_molar_mass_g_mol{0.0};

    EvaluatedPhase predicted_phase{EvaluatedPhase::Unknown};
    EvaluatedProperty<double> density_kg_m3;
    EvaluatedProperty<double> specific_heat_j_kgk;
    EvaluatedProperty<double> thermal_conductivity_w_mk;
    EvaluatedProperty<double> thermal_conductivity_through_plane_w_mk;
    EvaluatedProperty<double> electrical_conductivity_s_m;
    EvaluatedProperty<double> electrical_resistivity_ohm_m;
    EvaluatedProperty<double> electrical_conductivity_through_plane_s_m;
    EvaluatedProperty<double> thermal_expansion_1_k;
    EvaluatedProperty<double> young_modulus_pa;
    EvaluatedProperty<double> bulk_modulus_pa;
    EvaluatedProperty<double> shear_modulus_pa;
    EvaluatedProperty<double> poisson_ratio;
    EvaluatedProperty<double> estimated_melting_point_k;

    // Composite homogenization bounds (Voigt/Reuss/HS) where mechanical moduli exist
    std::optional<double> young_voigt_pa;
    std::optional<double> young_reuss_pa;
    std::optional<double> young_hs_lower_pa;
    std::optional<double> young_hs_upper_pa;

    std::vector<std::string> validation_errors;
    std::vector<std::string> warnings;
    std::vector<std::string> scientific_notes;
    std::vector<ElementComparisonRow> comparison_rows;
};

struct MixtureExperiment {
    core::base::Uuid id{core::base::Uuid::generate_random()};
    std::string name{"New Mixture Experiment"};
    std::string description;
    FormationScenario scenario{FormationScenario::Alloy};
    double temperature_k{298.15};
    double pressure_pa{101325.0};
    bool oxidizing_atmosphere{true};
    double user_density_override_kg_m3{0.0}; // >0 indicates UserExperimentalAssumption override
    std::vector<MixtureConstituentInput> inputs;
    std::optional<core::base::Uuid> promoted_material_id;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static core::base::Result<MixtureExperiment> from_json(std::string_view json_str);
};

/**
 * @brief Material Mixing Laboratory & Property Calculation Engine (MIXLAB + MATPROP).
 *        Evaluates compositions against ElementDatabase & ElementBehaviorEngine, enforces
 *        formation rules, computes mixing laws & bounds, and promotes valid mixtures to Material.
 */
class MaterialMixingLab {
public:
    [[nodiscard]] static core::base::Result<MixtureEvaluationResult> evaluate(
        const MixtureExperiment& experiment,
        const matdb::ElementDatabase& db = matdb::ElementDatabase::global());

    [[nodiscard]] static core::base::Result<core::domain::Material> promote_to_material(
        MixtureExperiment& experiment,
        const matdb::ElementDatabase& db = matdb::ElementDatabase::global());

    // Sample mixture properties across a temperature range [t_min_k, t_max_k]
    [[nodiscard]] static core::base::Result<std::vector<std::pair<double, MixtureEvaluationResult>>> sweep_temperature(
        const MixtureExperiment& experiment,
        double t_min_k,
        double t_max_k,
        size_t steps,
        const matdb::ElementDatabase& db = matdb::ElementDatabase::global());

    // Pre-built scientifically grounded starter experiments
    [[nodiscard]] static std::vector<MixtureExperiment> standard_presets();
};

} // namespace shunya::mat
