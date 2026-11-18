#include "tests/test_framework.hpp"

#include "shunya/mat/alloy_builder.hpp"
#include "shunya/mat/composite_homogenizer.hpp"
#include "shunya/mat/hume_rothery.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::mat;
using namespace shunya::matdb;

// =============================================================================
// PH4-13.01.001: Phase 4 Materials Engineering & Mixing Scenarios Verification
// =============================================================================

TEST_CASE("PH4-13.01.001-01: Hume-Rothery Heuristics & Solid Solubility Rules") {
    // 1. Favorable solid solution: Cu (solvent, FCC, r=128 pm) and Ni (solute, FCC, r=124 pm)
    ElementRecord cu;
    cu.atomic_number = 29;
    cu.symbol = "Cu";
    cu.name = "Copper";
    cu.atomic_radius = 128.0e-12;
    cu.space_group_number = 225; // FCC
    cu.electronegativity = 1.90;

    ElementRecord ni;
    ni.atomic_number = 28;
    ni.symbol = "Ni";
    ni.name = "Nickel";
    ni.atomic_radius = 124.0e-12;
    ni.space_group_number = 225; // FCC
    ni.electronegativity = 1.91;

    auto eval_cu_ni = HumeRotheryEngine::evaluate_pair(cu, ni);
    CHECK(eval_cu_ni.size_factor_passed);
    CHECK(eval_cu_ni.atomic_radius_mismatch_pct < 15.0);
    CHECK(eval_cu_ni.crystal_structure_matched);
    CHECK(eval_cu_ni.valence_factor_favorable);
    CHECK(eval_cu_ni.electrochemical_factor_passed);
    CHECK_FALSE(eval_cu_ni.intermetallic_tendency);
    CHECK(eval_cu_ni.overall_solid_solubility_favored);

    // 2. High size-mismatch / immiscible pair: Cu (r=128 pm) and Pb (r=175 pm) -> mismatch > 36%
    ElementRecord pb;
    pb.atomic_number = 82;
    pb.symbol = "Pb";
    pb.name = "Lead";
    pb.atomic_radius = 175.0e-12;
    pb.space_group_number = 225;
    pb.electronegativity = 2.33;

    auto eval_cu_pb = HumeRotheryEngine::evaluate_pair(cu, pb);
    CHECK_FALSE(eval_cu_pb.size_factor_passed);
    CHECK(eval_cu_pb.atomic_radius_mismatch_pct > 15.0);
    CHECK_FALSE(eval_cu_pb.overall_solid_solubility_favored);

    // 3. Intermetallic tendency: Ti (electronegativity 1.54) and Al (1.61) with structure mismatch
    ElementRecord ti;
    ti.atomic_number = 22;
    ti.symbol = "Ti";
    ti.name = "Titanium";
    ti.atomic_radius = 147.0e-12;
    ti.space_group_number = 194; // HCP
    ti.electronegativity = 1.54;

    ElementRecord al;
    al.atomic_number = 13;
    al.symbol = "Al";
    al.name = "Aluminium";
    al.atomic_radius = 143.0e-12;
    al.space_group_number = 225; // FCC
    al.electronegativity = 1.61;

    auto eval_ti_al = HumeRotheryEngine::evaluate_pair(ti, al);
    CHECK(eval_ti_al.size_factor_passed);
    // Crystal structures differ (HCP vs FCC) -> favors compound/multiphase rather than complete solution
    CHECK_FALSE(eval_ti_al.crystal_structure_matched);
}

TEST_CASE("PH4-13.01.001-02: Six Core Mixing Scenarios Verification") {
    // Scenario 1: Binary Solid Solution (Cu-Ni 70/30 Monel / Cupronickel)
    auto cuni = AlloyBuilder::build_from_weight_pct("CuNi_70_30", {{29, 70.0}, {28, 30.0}});
    REQUIRE(cuni.is_ok());
    CHECK(cuni.value().theoretical_density_kg_m3 > 8800.0);
    CHECK(cuni.value().theoretical_density_kg_m3 < 9000.0);

    // Scenario 2: Eutectic Alloy (Brass C260 Cu-Zn 70/30)
    auto brass = AlloyBuilder::preset_brass_c260();
    CHECK(brass.elements.size() == 2u);
    CHECK(brass.theoretical_density_kg_m3 > 8200.0);
    CHECK(brass.theoretical_density_kg_m3 < 8600.0);

    // Scenario 3: Intermetallic Compound (auto-balance onto base element)
    auto auto_alloy = AlloyBuilder::build_from_weight_pct("AutoBronze", {{29, 0.0}, {50, 10.0}}, true);
    REQUIRE(auto_alloy.is_ok());
    CHECK(std::abs(auto_alloy.value().elements[0].weight_pct - 90.0) < 1e-6);

    // Scenario 4: Layered Laminate Composite (Voigt in-plane vs Reuss through-plane)
    // Tungsten (K=310 GPa, G=161 GPa) and Copper (K=140 GPa, G=48 GPa)
    auto comp = CompositeHomogenizer::evaluate({
        {"W", 0.8, 310.0, 161.0},
        {"Cu", 0.2, 140.0, 48.0}
    });
    REQUIRE(comp.is_ok());
    const auto& bounds = comp.value();

    // Voigt upper bound >= Hashin-Shtrikman upper >= Hashin-Shtrikman lower >= Reuss lower bound
    CHECK(bounds.bulk_voigt_gpa >= bounds.bulk_hs_upper_gpa - 1e-6);
    CHECK(bounds.bulk_hs_upper_gpa >= bounds.bulk_hs_lower_gpa - 1e-6);
    CHECK(bounds.bulk_hs_lower_gpa >= bounds.bulk_reuss_gpa - 1e-6);

    CHECK(bounds.shear_voigt_gpa >= bounds.shear_hs_upper_gpa - 1e-6);
    CHECK(bounds.shear_hs_upper_gpa >= bounds.shear_hs_lower_gpa - 1e-6);
    CHECK(bounds.shear_hs_lower_gpa >= bounds.shear_reuss_gpa - 1e-6);

    // Scenario 5: Fiber / Particle Composite (effective Young's modulus and Poisson's ratio)
    CHECK(bounds.effective_youngs_gpa > 150.0);
    CHECK(bounds.effective_poisson > 0.25);
    CHECK(bounds.effective_poisson < 0.35);

    // Scenario 6: Superalloy (Ti-6Al-4V)
    auto ti64 = AlloyBuilder::preset_ti_6al_4v();
    CHECK(ti64.elements.size() == 3u);
    CHECK(std::abs(ti64.theoretical_density_kg_m3 - 4430.0) / 4430.0 <= 0.01);

    // Austenitic Stainless Steel SS 316L
    auto ss316 = AlloyBuilder::preset_ss_316l();
    CHECK(ss316.theoretical_density_kg_m3 > 7600.0);
    CHECK(ss316.theoretical_density_kg_m3 < 8100.0);
}
