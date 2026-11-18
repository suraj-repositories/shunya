#include "tests/test_framework.hpp"
#include "shunya/mat/material_comparison_engine.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya;
using namespace shunya::mat;

TEST_CASE("PH5-16.01: Element Evaluation and 8 Engineering Criteria") {
    auto& db = shunya::matdb::ElementDatabase::global();
    auto load_res = db.load_default();
    REQUIRE(load_res.is_ok());

    const auto* cu = db.find_by_symbol("Cu");
    const auto* al = db.find_by_symbol("Al");
    const auto* fe = db.find_by_symbol("Fe");
    const auto* au = db.find_by_symbol("Au");

    REQUIRE(cu != nullptr);
    REQUIRE(al != nullptr);
    REQUIRE(fe != nullptr);
    REQUIRE(au != nullptr);

    auto m_cu = MaterialComparisonEngine::evaluate_element(*cu);
    auto m_al = MaterialComparisonEngine::evaluate_element(*al);
    auto m_fe = MaterialComparisonEngine::evaluate_element(*fe);
    auto m_au = MaterialComparisonEngine::evaluate_element(*au);

    // 1. Electrical conductivity: Cu and Au should be significantly higher than Fe
    CHECK(m_cu.electrical_conductivity > 40.0);
    CHECK(m_fe.electrical_conductivity < 15.0);

    // 2. Corrosion resistance: Au should be highest (~99)
    CHECK(m_au.corrosion_resistance > 95.0);
    CHECK(m_fe.corrosion_resistance < 60.0);

    // 3. Density: Al should be lightweight (~2700 kg/m^3)
    CHECK(m_al.density < 3500.0);
    CHECK(m_au.density > 18000.0);

    // 4. Economic abundance: Fe should be very abundant (> 80), Au scarce (< 20)
    CHECK(m_fe.economic_abundance > 80.0);
    CHECK(m_au.economic_abundance < 25.0);
}

TEST_CASE("PH5-16.02: Radar Score Normalization & Coordinate Generation") {
    auto& db = shunya::matdb::ElementDatabase::global();
    (void)db.load_default();

    const auto* cu = db.find_by_symbol("Cu");
    const auto* al = db.find_by_symbol("Al");
    const auto* fe = db.find_by_symbol("Fe");

    REQUIRE(cu != nullptr);
    REQUIRE(al != nullptr);
    REQUIRE(fe != nullptr);

    std::vector<MaterialMetricScores> list = {
        MaterialComparisonEngine::evaluate_element(*cu),
        MaterialComparisonEngine::evaluate_element(*al),
        MaterialComparisonEngine::evaluate_element(*fe)
    };

    MaterialComparisonEngine::normalize_radar_scores(list);

    for (const auto& item : list) {
        for (int j = 0; j < 8; ++j) {
            CHECK(item.normalized_radar[j] >= 0.15);
            CHECK(item.normalized_radar[j] <= 1.0);
        }
    }

    // Al has lowest density, so its lightweight radar metric (index 4) should be 1.0
    CHECK(list[1].normalized_radar[4] == 1.0);
}

TEST_CASE("PH5-16.03: Multi-Objective Goal Ranking") {
    auto& db = shunya::matdb::ElementDatabase::global();
    (void)db.load_default();

    const auto* cu = db.find_by_symbol("Cu");
    const auto* al = db.find_by_symbol("Al");
    const auto* fe = db.find_by_symbol("Fe");
    const auto* au = db.find_by_symbol("Au");

    REQUIRE(cu != nullptr);
    REQUIRE(al != nullptr);
    REQUIRE(fe != nullptr);
    REQUIRE(au != nullptr);

    std::vector<MaterialMetricScores> list = {
        MaterialComparisonEngine::evaluate_element(*cu),
        MaterialComparisonEngine::evaluate_element(*al),
        MaterialComparisonEngine::evaluate_element(*fe),
        MaterialComparisonEngine::evaluate_element(*au)
    };

    // 1. Goal: "Maximum Electrical Conductivity" -> Cu should rank #1
    auto rank_elec = MaterialComparisonEngine::rank_materials(list, "Maximum Electrical Conductivity");
    CHECK(rank_elec.winner_name.find("Copper") != std::string::npos || rank_elec.winner_name == "Cu");
    CHECK(!rank_elec.rankings.empty());

    // 2. Goal: "Marine & Harsh Corrosion Resistance" -> Au should rank #1
    auto rank_corr = MaterialComparisonEngine::rank_materials(list, "Marine & Harsh Corrosion Resistance");
    CHECK(rank_corr.winner_name.find("Gold") != std::string::npos || rank_corr.winner_name == "Au");
}

TEST_CASE("PH5-16.04: Structured Explanation & Markdown Report Generation") {
    auto& db = shunya::matdb::ElementDatabase::global();
    (void)db.load_default();

    const auto* cu = db.find_by_symbol("Cu");
    const auto* ni = db.find_by_symbol("Ni");
    REQUIRE(cu != nullptr);
    REQUIRE(ni != nullptr);

    auto cu_ni = MaterialComparisonEngine::evaluate_mixture(
        "Cupronickel 70/30",
        {cu, ni},
        {0.70, 0.30},
        8940.0,
        29.0,
        3.5e6,
        1443.0,
        "Alloy"
    );

    auto expl = MaterialComparisonEngine::generate_explanation(
        cu_ni,
        {"Cu", "Ni"},
        {0.70, 0.30},
        "Alloy",
        "Complete Solid Solution"
    );

    CHECK(!expl.summary.empty());
    CHECK(!expl.features_advantages.empty());
    CHECK(!expl.disadvantages_risks.empty());
    CHECK(!expl.physical_mechanism.empty());
    CHECK(!expl.suggested_improvements.empty());
    CHECK(expl.physical_mechanism.find("metallic lattice") != std::string::npos);

    // Markdown Export
    std::string md = MaterialComparisonEngine::export_markdown_report(expl, cu_ni);
    CHECK(md.find("# Material Engineering Report: Cupronickel 70/30") != std::string::npos);
    CHECK(md.find("| **Electrical Conductivity** |") != std::string::npos);
    CHECK(md.find("## Physical Mechanism") != std::string::npos);
}
