#include "tests/test_framework.hpp"
#include "shunya/core/domain/material_ui.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::core::domain;
using namespace shunya::core::domain::matui;

// =============================================================================
// MATUI-01: Periodic Table Explorer & Browser View-Model
// =============================================================================

TEST_CASE("MATUI-01: Periodic Table Explorer View-Model and Filtering") {
    PeriodicTableDatabase table;
    table.seed_representative_elements();
    PeriodicTableBrowserViewModel vm(table);

    // Search by symbol
    ElementFilter f_sym;
    f_sym.search_query = "Fe";
    auto fe_results = vm.filter(f_sym);
    REQUIRE(fe_results.size() == 1u);
    CHECK(fe_results[0].symbol == "Fe");
    CHECK(fe_results[0].name == "Iron");
    CHECK(fe_results[0].atomic_number == 26);

    // Filter by period 1 (H and He)
    ElementFilter f_period;
    f_period.period = 1;
    auto period1_results = vm.filter(f_period);
    CHECK(period1_results.size() == 2u);

    // Get detail
    auto cu_detail = vm.get_element_detail(29); // Copper
    REQUIRE(cu_detail.has_value());
    CHECK(cu_detail->symbol == "Cu");
    CHECK(cu_detail->melting_point_k > 1000.0);
}

// =============================================================================
// MATUI-02: Property Display Catalogue Grouping
// =============================================================================

TEST_CASE("MATUI-02: Material Property Catalogue Grouping") {
    PropertyDisplayItem item{
        "Thermal Conductivity",
        PropertyCategory::Thermal,
        401.0,
        "W/(m*K)",
        "T = 293.15 K, P = 1 atm",
        "CRC Handbook of Chemistry and Physics",
        PropertyStatus::Available
    };

    CHECK(item.name == "Thermal Conductivity");
    CHECK(item.category == PropertyCategory::Thermal);
    CHECK(item.value == 401.0);
    CHECK(item.status == PropertyStatus::Available);
}

// =============================================================================
// MATUI-03: Material Composition Builder
// =============================================================================

TEST_CASE("MATUI-03: Custom Material Composition Builder") {
    // Valid Stainless Steel 304 alloy approximation (70% Fe, 20% Cr, 10% Ni)
    std::vector<CompositionConstituent> constituents{
        {"Fe", 0.70},
        {"Cr", 0.20},
        {"Ni", 0.10}
    };

    auto steel_res = MaterialBuilder::build_material(
        "StainlessSteel304",
        MaterialOriginType::Alloy,
        constituents,
        8000.0 // 8000 kg/m^3
    );

    REQUIRE(steel_res.is_ok());
    CHECK(steel_res.value().name == "StainlessSteel304");
    CHECK(steel_res.value().density == 8000.0);
    CHECK(steel_res.value().constituents.size() == 3u);

    // Imbalanced alloy: fractions sum to 0.8 instead of 1.0
    std::vector<CompositionConstituent> bad_constituents{
        {"Fe", 0.70},
        {"Cr", 0.10}
    };
    auto bad_res = MaterialBuilder::build_material(
        "IncompleteAlloy",
        MaterialOriginType::Alloy,
        bad_constituents
    );
    CHECK(bad_res.is_err());
}

// =============================================================================
// MATUI-04: Material Assignment Scope
// =============================================================================

TEST_CASE("MATUI-04: Material Assignment Scope") {
    MaterialAssignmentTarget target;
    target.material_name = "Copper";
    target.object_id = "trace_layer_1";
    target.scope = AssignmentTargetScope::LayeredRegions;
    target.sub_entity_indices = {0, 1, 2};

    CHECK(target.scope == AssignmentTargetScope::LayeredRegions);
    CHECK(target.sub_entity_indices.size() == 3u);
}

// =============================================================================
// MATUI-05 & MATUI-06: Conditions & Phase Transition Inspector
// =============================================================================

TEST_CASE("MATUI-05 & MATUI-06: Conditions and Phase Transition Inspection") {
    // Copper: Tm = 1357.77 K, Tb = 2835.0 K
    double t_m = 1357.77;
    double t_b = 2835.0;

    // Solid at room temperature
    auto solid_report = PhaseInspector::inspect_phase(300.0, t_m, t_b);
    CHECK(solid_report.phase == MaterialPhaseState::Solid);
    CHECK(solid_report.liquid_fraction == 0.0);
    CHECK(solid_report.warnings.empty());

    // Melting transition at Tm
    auto melting_report = PhaseInspector::inspect_phase(t_m, t_m, t_b);
    CHECK(melting_report.phase == MaterialPhaseState::MeltingTwoPhase);
    CHECK(!melting_report.warnings.empty());

    // Liquid copper at 1500 K
    auto liquid_report = PhaseInspector::inspect_phase(1500.0, t_m, t_b);
    CHECK(liquid_report.phase == MaterialPhaseState::Liquid);
    CHECK(liquid_report.liquid_fraction == 1.0);

    // Gaseous copper at 3000 K
    auto gas_report = PhaseInspector::inspect_phase(3000.0, t_m, t_b);
    CHECK(gas_report.phase == MaterialPhaseState::Gas);
    CHECK(!gas_report.warnings.empty());
}

// =============================================================================
// MATUI-07: Property-vs-Condition Curves Plotter
// =============================================================================

TEST_CASE("MATUI-07: Property vs Temperature Curve Sampling") {
    // Model thermal conductivity k(T) = k0 * (1 - 0.001 * (T - 300))
    auto k_func = [](double t_k) {
        return 400.0 * (1.0 - 0.0005 * (t_k - 300.0));
    };

    auto curve_pts = MaterialCurvePlotter::sample_curve(k_func, 300.0, 500.0, 5);
    REQUIRE(curve_pts.size() == 5u);
    CHECK(curve_pts[0].condition_x == 300.0);
    CHECK(curve_pts[0].property_y == 400.0);
    CHECK(curve_pts[4].condition_x == 500.0);
    CHECK(curve_pts[4].property_y < 400.0);
}

// =============================================================================
// MATUI-08 & MATUI-09: Validation Report & Solver Dependency Mapping
// =============================================================================

TEST_CASE("MATUI-08 & MATUI-09: Solver Requirement Validation and Blocking Flags") {
    CustomMaterialDefinition mat;
    mat.name = "Insulator";
    mat.density = 2000.0;
    mat.specific_heat = 800.0;
    mat.thermal_conductivity = 0.5;
    mat.electrical_conductivity = 0.0; // Missing electrical conductivity!

    // Valid for Thermal solver
    auto therm_rep = MaterialSolverRequirementValidator::validate_for_domain(
        mat, SolverDomainDependency::Thermal);
    CHECK(therm_rep.is_valid_for_run);
    CHECK(therm_rep.blocking_missing_properties.empty());

    // Blocked for Circuit solver
    auto circ_rep = MaterialSolverRequirementValidator::validate_for_domain(
        mat, SolverDomainDependency::Circuit);
    CHECK_FALSE(circ_rep.is_valid_for_run);
    REQUIRE(circ_rep.blocking_missing_properties.size() == 1u);
    CHECK(circ_rep.blocking_missing_properties[0] == "electrical_conductivity");
}

// =============================================================================
// MATUI-10: Material Side-by-Side Comparison Matrix
// =============================================================================

TEST_CASE("MATUI-10: Material Side-by-Side Compare Matrix") {
    CustomMaterialDefinition copper;
    copper.name = "Copper";
    copper.density = 8960.0;
    copper.specific_heat = 385.0;
    copper.thermal_conductivity = 401.0;
    copper.electrical_conductivity = 5.96e7;
    copper.youngs_modulus = 1.1e11;

    CustomMaterialDefinition aluminum;
    aluminum.name = "Aluminum";
    aluminum.density = 2700.0;
    aluminum.specific_heat = 900.0;
    aluminum.thermal_conductivity = 237.0;
    aluminum.electrical_conductivity = 3.77e7;
    aluminum.youngs_modulus = 7.0e10;

    auto diffs = MaterialComparator::compare_materials(copper, aluminum);
    CHECK(diffs.size() == 5u);

    // Verify density difference row
    CHECK(diffs[0].property_name == "Density");
    CHECK(diffs[0].mat_a_value == 8960.0);
    CHECK(diffs[0].mat_b_value == 2700.0);
    CHECK(diffs[0].absolute_diff == (8960.0 - 2700.0));
    CHECK(diffs[0].percentage_diff > 0.0);
}
