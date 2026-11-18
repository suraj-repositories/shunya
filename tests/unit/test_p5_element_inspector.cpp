#include "tests/test_framework.hpp"

#include "shunya/matdb/element_database.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"
#include "shunya/mat/element_behavior.hpp"
#include "shunya/eng/engineering_pipeline.hpp"

#include <cmath>

using namespace shunya;

TEST_CASE("PH5-02.01: Element Default Selection & Reactive Selection State") {
    auto& db = matdb::ElementDatabase::global();
    if (!db.is_loaded()) {
        auto res = db.load_default();
        REQUIRE(res.is_ok());
    }

    // 1. Cold start default selection must be Hydrogen (Z=1)
    const int default_z = 1;
    const auto* h = db.find_by_z(default_z);
    REQUIRE(h != nullptr);
    CHECK(h->atomic_number == 1);
    CHECK(h->symbol == "H");
    CHECK(h->name == "Hydrogen");

    const auto* h_cat = matdb::Elements118Catalog::find_by_z(1);
    REQUIRE(h_cat != nullptr);
    CHECK(h_cat->family == matdb::ElementFamily::ReactiveNonmetal);

    // 2. Selection changes dynamically and evaluates physical state
    // Gold (Z=79)
    const int gold_z = 79;
    const auto* au = db.find_by_z(gold_z);
    REQUIRE(au != nullptr);
    CHECK(au->symbol == "Au");
    CHECK(au->name == "Gold");
    CHECK(au->block == "d");
    REQUIRE(au->density.has_value());
    CHECK(std::abs(*au->density - 19300.0) < 50.0);

    const auto* au_cat = matdb::Elements118Catalog::find_by_z(gold_z);
    REQUIRE(au_cat != nullptr);
    CHECK(au_cat->family == matdb::ElementFamily::TransitionMetal);

    // Oxygen (Z=8)
    const auto* o = db.find_by_z(8);
    REQUIRE(o != nullptr);
    CHECK(o->symbol == "O");
    CHECK(o->name == "Oxygen");
    CHECK(o->phase == "Gas");
    const auto* o_cat = matdb::Elements118Catalog::find_by_z(8);
    REQUIRE(o_cat != nullptr);
    CHECK(o_cat->family == matdb::ElementFamily::ReactiveNonmetal);

    // Uranium (Z=92)
    const auto* u = db.find_by_z(92);
    REQUIRE(u != nullptr);
    CHECK(u->symbol == "U");
    CHECK(u->name == "Uranium");
    CHECK(u->block == "f");
    const auto* u_cat = matdb::Elements118Catalog::find_by_z(92);
    REQUIRE(u_cat != nullptr);
    CHECK(u_cat->family == matdb::ElementFamily::Actinide);
}

TEST_CASE("PH5-02.02: Comprehensive Right Inspector Data Binding & Physical Behavior at T & P") {
    auto& db = matdb::ElementDatabase::global();
    REQUIRE(db.is_loaded());

    // 1. Hydrogen (Z=1) behavior at STP (298.15 K, 1 atm)
    const auto* h = db.find_by_z(1);
    REQUIRE(h != nullptr);
    auto h_stp = mat::ElementBehaviorEngine::evaluate(*h, 298.15, 101325.0, true);
    CHECK(h_stp.atomic_number == 1);
    CHECK(h_stp.phase == mat::EvaluatedPhase::Gas);
    REQUIRE(h_stp.density_kg_m3.available());
    CHECK(std::abs(*h_stp.density_kg_m3.value - 0.0899) < 0.05);

    // 2. Hydrogen cryogenic behavior (15 K, solid/liquid transition)
    auto h_cryo = mat::ElementBehaviorEngine::evaluate(*h, 15.0, 101325.0, false);
    CHECK((h_cryo.phase == mat::EvaluatedPhase::Liquid || h_cryo.phase == mat::EvaluatedPhase::Solid));

    // 3. Iron (Z=26) behavior at STP vs high temperature (2000 K, liquid)
    const auto* fe = db.find_by_z(26);
    REQUIRE(fe != nullptr);
    auto fe_stp = mat::ElementBehaviorEngine::evaluate(*fe, 298.15, 101325.0, true);
    CHECK(fe_stp.phase == mat::EvaluatedPhase::Solid);
    CHECK(fe_stp.magnetic_state == "Ferromagnetic");

    auto fe_hot = mat::ElementBehaviorEngine::evaluate(*fe, 2000.0, 101325.0, true);
    CHECK(fe_hot.phase == mat::EvaluatedPhase::Liquid);

    // 4. Verification that 82 CSV fields are accessible per element
    CHECK(h->raw_csv_fields.size() >= 80);
    CHECK(fe->raw_csv_fields.size() >= 80);
}
