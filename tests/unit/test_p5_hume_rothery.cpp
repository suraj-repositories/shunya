#include "tests/test_framework.hpp"
#include "shunya/mat/atomic_bond_checker.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cmath>
#include <string>

using namespace shunya;
using namespace shunya::mat;

TEST_CASE("PH5-15.01: Chemical Bond Classification (NaCl, H2O, C2, Cu-Ni)") {
    auto& db = shunya::matdb::ElementDatabase::global();
    auto load_res = db.load_default();
    REQUIRE(load_res.is_ok());

    const auto* na = db.find_by_symbol("Na");
    const auto* cl = db.find_by_symbol("Cl");
    const auto* h = db.find_by_symbol("H");
    const auto* o = db.find_by_symbol("O");
    const auto* c = db.find_by_symbol("C");
    const auto* cu = db.find_by_symbol("Cu");
    const auto* ni = db.find_by_symbol("Ni");

    REQUIRE(na != nullptr);
    REQUIRE(cl != nullptr);
    REQUIRE(h != nullptr);
    REQUIRE(o != nullptr);
    REQUIRE(c != nullptr);
    REQUIRE(cu != nullptr);
    REQUIRE(ni != nullptr);

    // 1. NaCl -> Ionic (DeltaEN > 1.7)
    auto nacl = AtomicBondChecker::analyze_pair(*na, *cl);
    CHECK(nacl.delta_electronegativity > 1.7);
    CHECK(nacl.primary_bond_type == AtomicBondType::Ionic);
    CHECK(nacl.plain_language_verdict.find("ionic") != std::string::npos);

    // 2. H2O -> Polar Covalent (0.5 <= DeltaEN <= 1.7 between nonmetals)
    auto ho = AtomicBondChecker::analyze_pair(*h, *o);
    CHECK(ho.delta_electronegativity >= 0.5);
    CHECK(ho.delta_electronegativity <= 1.7);
    CHECK(ho.primary_bond_type == AtomicBondType::PolarCovalent);

    // 3. C-C (C2) -> Nonpolar Covalent (DeltaEN < 0.5)
    auto cc = AtomicBondChecker::analyze_pair(*c, *c);
    CHECK(cc.delta_electronegativity < 0.5);
    CHECK(cc.primary_bond_type == AtomicBondType::NonpolarCovalent);

    // 4. Cu-Ni -> Metallic
    auto cuni = AtomicBondChecker::analyze_pair(*cu, *ni);
    CHECK(cuni.primary_bond_type == AtomicBondType::Metallic);
}

TEST_CASE("PH5-15.02: Hume-Rothery Miscibility Prediction (Cu-Ni vs. Fe-Pb)") {
    auto& db = shunya::matdb::ElementDatabase::global();
    (void)db.load_default();

    const auto* cu = db.find_by_symbol("Cu");
    const auto* ni = db.find_by_symbol("Ni");
    const auto* fe = db.find_by_symbol("Fe");
    const auto* pb = db.find_by_symbol("Pb");

    REQUIRE(cu != nullptr);
    REQUIRE(ni != nullptr);
    REQUIRE(fe != nullptr);
    REQUIRE(pb != nullptr);

    // Cu-Ni: Complete isomorphous solid solution
    auto cuni = AtomicBondChecker::analyze_pair(*cu, *ni);
    CHECK(cuni.miscibility == MiscibilityState::CompleteSolidSolution);
    CHECK(cuni.radius_difference_pct <= 15.0);
    CHECK(cuni.plain_language_verdict.find("complete isomorphous") != std::string::npos);

    // Fe-Pb: Immiscible / Phase-separated
    auto fepb = AtomicBondChecker::analyze_pair(*fe, *pb);
    CHECK(fepb.miscibility == MiscibilityState::ImmisciblePhaseSeparated);
    CHECK(fepb.plain_language_verdict.find("immiscible") != std::string::npos);
}

TEST_CASE("PH5-15.03: Atomic Bond Report & Scenario Conflict Detection") {
    auto& db = shunya::matdb::ElementDatabase::global();
    (void)db.load_default();

    const auto* na = db.find_by_symbol("Na");
    const auto* cl = db.find_by_symbol("Cl");
    REQUIRE(na != nullptr);
    REQUIRE(cl != nullptr);

    // Scenario Conflict: Selecting "Alloy" for Na + Cl
    auto rep = AtomicBondChecker::analyze_mixture({na, cl}, {0.5, 0.5}, "Alloy");
    CHECK(!rep.is_formation_scenario_compatible);
    CHECK(!rep.chemical_warnings.empty());
    bool found_mismatch = false;
    for (const auto& w : rep.chemical_warnings) {
        if (w.find("Scenario mismatch") != std::string::npos) found_mismatch = true;
    }
    CHECK(found_mismatch);

    // ChemicalCompound: Suggests NaCl stoichiometry
    auto comp_rep = AtomicBondChecker::analyze_mixture({na, cl}, {0.5, 0.5}, "ChemicalCompound");
    CHECK(comp_rep.is_formation_scenario_compatible);
    CHECK(!comp_rep.stoichiometric_suggestions.empty());
    CHECK(comp_rep.stoichiometric_suggestions[0].find("NaCl") != std::string::npos);
}
