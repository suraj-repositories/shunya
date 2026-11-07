#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/periodic_table.hpp"
#include "shunya/core/domain/material.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain::matui {

// =============================================================================
// MATUI-01: Periodic Table Explorer & Browser View-Model
// =============================================================================

struct ElementFilter {
    std::optional<int> group;
    std::optional<int> period;
    std::optional<char> block;
    std::string search_query; // name or symbol
};

struct ElementDetailViewModel {
    int atomic_number{1};
    std::string symbol;
    std::string name;
    double atomic_weight{1.008};
    char block{'s'};
    int period{1};
    int group{1};
    double melting_point_k{14.01};
    double boiling_point_k{20.28};
    double density_at_stp{0.08988}; // kg/m^3
};

class PeriodicTableBrowserViewModel {
public:
    explicit PeriodicTableBrowserViewModel(PeriodicTableDatabase table);

    [[nodiscard]] std::vector<ElementDetailViewModel> filter(const ElementFilter& filter) const;
    [[nodiscard]] std::optional<ElementDetailViewModel> get_element_detail(int atomic_number) const;

private:
    PeriodicTableDatabase table_;
    std::vector<ElementDetailViewModel> element_list_;
};

// =============================================================================
// MATUI-02: Property Catalogue Grouping & Availability
// =============================================================================

enum class PropertyCategory {
    Thermal,
    Electrical,
    Mechanical,
    Optical,
    Chemical
};

enum class PropertyStatus {
    Available,
    ConditionDependent,
    Estimated,
    Missing
};

struct PropertyDisplayItem {
    std::string name;
    PropertyCategory category{PropertyCategory::Mechanical};
    double value{0.0};
    std::string unit;
    std::string condition; // e.g. "T = 293.15 K, P = 1 atm"
    std::string source;    // e.g. "NIST Chemistry WebBook"
    PropertyStatus status{PropertyStatus::Available};
};

// =============================================================================
// MATUI-03: Material Composition Builder (Element, Compound, Alloy, Mixture)
// =============================================================================

enum class MaterialOriginType {
    PureElement,
    Isotope,
    Compound,
    Alloy,
    CompositeMixture
};

struct CompositionConstituent {
    std::string formula_or_name;
    double mass_fraction{1.0}; // 0.0 to 1.0
};

struct CustomMaterialDefinition {
    std::string name;
    MaterialOriginType origin_type{MaterialOriginType::PureElement};
    std::vector<CompositionConstituent> constituents;
    double density{1000.0};
    double specific_heat{1000.0};
    double thermal_conductivity{1.0};
    double electrical_conductivity{1.0e6};
    double youngs_modulus{1.0e9};
};

class MaterialBuilder {
public:
    static base::Result<CustomMaterialDefinition> build_material(
        std::string name,
        MaterialOriginType origin,
        std::vector<CompositionConstituent> constituents,
        double base_density = 1000.0);
};

// =============================================================================
// MATUI-04: Material Assignment Scope
// =============================================================================

enum class AssignmentTargetScope {
    WholeObject,
    SelectedFaces,
    Volumes,
    LayeredRegions
};

struct MaterialAssignmentTarget {
    std::string material_name;
    std::string object_id;
    AssignmentTargetScope scope{AssignmentTargetScope::WholeObject};
    std::vector<uint32_t> sub_entity_indices; // face or layer indices
};

// =============================================================================
// MATUI-05 & MATUI-06: Conditions & Phase Transition Inspector
// =============================================================================

struct ConditionState {
    double temperature_kelvin{293.15};
    double pressure_pascal{101325.0};
};

enum class MaterialPhaseState {
    Solid,
    MeltingTwoPhase,
    Liquid,
    BoilingTwoPhase,
    Gas,
    Plasma
};

struct PhaseStatusReport {
    MaterialPhaseState phase{MaterialPhaseState::Solid};
    double liquid_fraction{0.0}; // 0.0 to 1.0 during melting
    std::vector<std::string> warnings;
};

class PhaseInspector {
public:
    static PhaseStatusReport inspect_phase(
        double current_temp_k,
        double melting_point_k,
        double boiling_point_k,
        double latent_heat_fusion = 200000.0);
};

// =============================================================================
// MATUI-07: Property-vs-Condition Curves for UI Plotting
// =============================================================================

struct CurveDataPoint {
    double condition_x{0.0}; // e.g. T (K) or P (Pa)
    double property_y{0.0};  // e.g. k(T), rho(T)
};

class MaterialCurvePlotter {
public:
    // Samples property curve at N points between x_min and x_max
    static std::vector<CurveDataPoint> sample_curve(
        const std::function<double(double)>& property_func,
        double x_min,
        double x_max,
        size_t num_samples = 50);
};

// =============================================================================
// MATUI-08 & MATUI-09: Validation Report & Solver Dependency Mapping
// =============================================================================

enum class SolverDomainDependency {
    Thermal,
    Circuit,
    Mechanics,
    Fluid,
    Optics
};

struct MaterialValidationReport {
    std::string material_name;
    bool is_valid_for_run{true};
    std::vector<std::string> blocking_missing_properties;
    std::vector<std::string> warnings;
};

class MaterialSolverRequirementValidator {
public:
    static MaterialValidationReport validate_for_domain(
        const CustomMaterialDefinition& mat,
        SolverDomainDependency domain);
};

// =============================================================================
// MATUI-10: Material Side-by-Side Compare View
// =============================================================================

struct PropertyComparisonRow {
    std::string property_name;
    double mat_a_value{0.0};
    double mat_b_value{0.0};
    double absolute_diff{0.0};
    double percentage_diff{0.0};
    std::string unit;
};

class MaterialComparator {
public:
    static std::vector<PropertyComparisonRow> compare_materials(
        const CustomMaterialDefinition& a,
        const CustomMaterialDefinition& b);
};

} // namespace shunya::core::domain::matui
