#include "tests/test_framework.hpp"

#include "shunya/matdb/element_database.hpp"
#include "shunya/mat/element_behavior.hpp"

#include <string>

using namespace shunya;

TEST_CASE("PH5-03.01: Thermodynamic Phase Classification at 100 K, 298.15 K & 3000 K") {
    auto& db = matdb::ElementDatabase::global();
    if (!db.is_loaded()) {
        auto res = db.load_default();
        REQUIRE(res.is_ok());
    }

    // 1. STP (298.15 K, 1 atm)
    // Only Bromine (Z=35) and Mercury (Z=80) are liquid elements at standard temperature
    const auto* hg = db.find_by_z(80);
    REQUIRE(hg != nullptr);
    auto hg_rep = mat::ElementBehaviorEngine::evaluate(*hg, 298.15, 101325.0, true);
    CHECK(hg_rep.phase == mat::EvaluatedPhase::Liquid);

    const auto* br = db.find_by_z(35);
    REQUIRE(br != nullptr);
    auto br_rep = mat::ElementBehaviorEngine::evaluate(*br, 298.15, 101325.0, true);
    CHECK(br_rep.phase == mat::EvaluatedPhase::Liquid);

    // Common standard gases
    const auto* h = db.find_by_z(1);
    CHECK(mat::ElementBehaviorEngine::evaluate(*h, 298.15, 101325.0, true).phase == mat::EvaluatedPhase::Gas);
    const auto* he = db.find_by_z(2);
    CHECK(mat::ElementBehaviorEngine::evaluate(*he, 298.15, 101325.0, true).phase == mat::EvaluatedPhase::Gas);
    const auto* o = db.find_by_z(8);
    CHECK(mat::ElementBehaviorEngine::evaluate(*o, 298.15, 101325.0, true).phase == mat::EvaluatedPhase::Gas);

    // Common standard solids
    const auto* fe = db.find_by_z(26);
    CHECK(mat::ElementBehaviorEngine::evaluate(*fe, 298.15, 101325.0, true).phase == mat::EvaluatedPhase::Solid);
    const auto* cu = db.find_by_z(29);
    CHECK(mat::ElementBehaviorEngine::evaluate(*cu, 298.15, 101325.0, true).phase == mat::EvaluatedPhase::Solid);

    // 2. Near-ambient temperature transition (303 K)
    // Gallium (Z=31, Tm = 302.9 K) melts into liquid at 303 K
    const auto* ga = db.find_by_z(31);
    REQUIRE(ga != nullptr);
    auto ga_cold = mat::ElementBehaviorEngine::evaluate(*ga, 295.0, 101325.0, true);
    CHECK(ga_cold.phase == mat::EvaluatedPhase::Solid);
    auto ga_warm = mat::ElementBehaviorEngine::evaluate(*ga, 305.0, 101325.0, true);
    CHECK(ga_warm.phase == mat::EvaluatedPhase::Liquid);

    // 3. Cryogenic temperature (100 K)
    // Mercury (Tm = 234 K) freezes solid at 100 K
    auto hg_100k = mat::ElementBehaviorEngine::evaluate(*hg, 100.0, 101325.0, true);
    CHECK(hg_100k.phase == mat::EvaluatedPhase::Solid);

    // 4. Extreme high temperature (3000 K - 3500 K)
    // Copper (Tm = 1357.77 K, Tb = 3200.15 K) is Liquid at 3000 K and vaporizes into Gas at 3500 K
    auto cu_3000k = mat::ElementBehaviorEngine::evaluate(*cu, 3000.0, 101325.0, true);
    CHECK(cu_3000k.phase == mat::EvaluatedPhase::Liquid);
    auto cu_3500k = mat::ElementBehaviorEngine::evaluate(*cu, 3500.0, 101325.0, true);
    CHECK(cu_3500k.phase == mat::EvaluatedPhase::Gas);

    // Tungsten (Tm = 3695 K) remains Solid at 3000 K
    const auto* w = db.find_by_z(74);
    REQUIRE(w != nullptr);
    auto w_3000k = mat::ElementBehaviorEngine::evaluate(*w, 3000.0, 101325.0, true);
    CHECK(w_3000k.phase == mat::EvaluatedPhase::Solid);
}

TEST_CASE("PH5-03.02: Multi-Criteria Real-Time Search & Table Filtering") {
    auto& db = matdb::ElementDatabase::global();
    REQUIRE(db.is_loaded());

    // 1. Search by symbol (case insensitive)
    matdb::ElementQuery q_sym;
    q_sym.search_text = "fe";
    auto res_sym = db.query(q_sym);
    REQUIRE(!res_sym.empty());
    bool found_fe = false;
    for (const auto* r : res_sym) {
        if (r->symbol == "Fe" && r->atomic_number == 26) found_fe = true;
    }
    CHECK(found_fe);

    // 2. Search by atomic number exact string
    matdb::ElementQuery q_z;
    q_z.search_text = "29";
    auto res_z = db.query(q_z);
    REQUIRE(!res_z.empty());
    CHECK(res_z[0]->symbol == "Cu");
    CHECK(res_z[0]->atomic_number == 29);

    // 3. Search by name prefix/substring
    matdb::ElementQuery q_name;
    q_name.search_text = "Tungsten";
    auto res_name = db.query(q_name);
    REQUIRE(!res_name.empty());
    CHECK(res_name[0]->symbol == "W");
    CHECK(res_name[0]->atomic_number == 74);
}
