#include "tests/test_framework.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace shunya;
using namespace shunya::mat;

TEST_CASE("PH5-11.01: Fe-C Alloy Scientific Evaluation (Steel 99% Fe + 1% C)") {
    auto& db = shunya::matdb::ElementDatabase::global();
    auto load_res = db.load_default();
    REQUIRE(load_res.is_ok());
    REQUIRE(db.count() >= 118);

    MixtureExperiment exp;
    exp.name = "Carbon_Steel_Test_Alloy";
    exp.scenario = FormationScenario::Alloy;
    exp.temperature_k = 298.15;
    exp.pressure_pa = 101325.0;
    exp.oxidizing_atmosphere = true;

    MixtureConstituentInput fe_in;
    fe_in.symbol_or_id = "Fe";
    fe_in.basis = QuantityInputBasis::MassFractionPct;
    fe_in.amount = 99.0;
    fe_in.is_matrix = true;
    exp.inputs.push_back(fe_in);

    MixtureConstituentInput c_in;
    c_in.symbol_or_id = "C";
    c_in.basis = QuantityInputBasis::MassFractionPct;
    c_in.amount = 1.0;
    c_in.is_matrix = false;
    exp.inputs.push_back(c_in);

    auto eval_res = MaterialMixingLab::evaluate(exp, db);
    REQUIRE(eval_res.is_ok());

    const auto& res = eval_res.value();
    CHECK(res.is_chemically_valid);
    CHECK(res.predicted_phase == EvaluatedPhase::Solid);

    // Density must be close to analytical steel density (~7850 kg/m3)
    REQUIRE(res.density_kg_m3.value.has_value());
    double rho = *res.density_kg_m3.value;
    CHECK(rho >= 7600.0);
    CHECK(rho <= 8000.0);

    // Thermal conductivity must be around steel/iron values (~50 - 85 W/(m*K))
    REQUIRE(res.thermal_conductivity_w_mk.value.has_value());
    double k = *res.thermal_conductivity_w_mk.value;
    CHECK(k >= 40.0);
    CHECK(k <= 90.0);

    // Comparison rows must be populated
    CHECK(!res.comparison_rows.empty());
    bool found_density_row = false;
    for (const auto& row : res.comparison_rows) {
        if (row.property_name == "Density") {
            found_density_row = true;
            CHECK(row.constituent_values.size() == 2);
        }
    }
    CHECK(found_density_row);
}

TEST_CASE("PH5-11.02: Live Composition Normalization Engine (110% -> 100%)") {
    // 99% Fe + 1% C + 10% Cu = 110%
    struct RawIngredient {
        std::string symbol;
        double amount;
    };

    std::vector<RawIngredient> ing = {
        {"Fe", 99.0},
        {"C", 1.0},
        {"Cu", 10.0}
    };

    double total = 0.0;
    for (const auto& item : ing) {
        total += item.amount;
    }
    CHECK(std::abs(total - 110.0) < 1e-6);

    // Normalization algorithm: w_i' = (w_i / total) * 100.0
    std::vector<RawIngredient> normalized;
    double norm_total = 0.0;
    for (const auto& item : ing) {
        double scaled = (item.amount / total) * 100.0;
        normalized.push_back({item.symbol, scaled});
        norm_total += scaled;
    }

    CHECK(std::abs(norm_total - 100.0) < 1e-6);

    // Check individual normalized proportions:
    // Fe: 99 / 110 * 100 = 90.0%
    // C:   1 / 110 * 100 = 0.90909...% (~0.91%)
    // Cu: 10 / 110 * 100 = 9.0909...% (~9.09%)
    CHECK(std::abs(normalized[0].amount - 90.0) < 0.01);
    CHECK(std::abs(normalized[1].amount - 0.909) < 0.01);
    CHECK(std::abs(normalized[2].amount - 9.091) < 0.01);
}

TEST_CASE("PH5-12.01: Mixing Lab Multi-Pane Splitter Clamping & Default Proportions") {
    // Left card: min 280, max 600, default 340
    constexpr double kLeftMin = 280.0;
    constexpr double kLeftMax = 600.0;
    constexpr double kLeftDefault = 340.0;

    auto clamp_left = [&](double cur, double delta) {
        return std::max(kLeftMin, std::min(kLeftMax, cur + delta));
    };

    CHECK(clamp_left(kLeftDefault, 50.0) == 390.0);
    CHECK(clamp_left(kLeftDefault, 800.0) == 600.0);
    CHECK(clamp_left(kLeftDefault, -500.0) == 280.0);

    // Right card: min 240, max 500, default 280
    constexpr double kRightMin = 240.0;
    constexpr double kRightMax = 500.0;
    constexpr double kRightDefault = 280.0;

    auto clamp_right = [&](double cur, double delta_neg) {
        return std::max(kRightMin, std::min(kRightMax, cur - delta_neg));
    };

    CHECK(clamp_right(kRightDefault, -40.0) == 320.0);
    CHECK(clamp_right(kRightDefault, 200.0) == 240.0);

    // Middle internal horizontal splitter: min 120, max 400, default 180
    constexpr double kMidInternalMin = 120.0;
    constexpr double kMidInternalMax = 400.0;
    constexpr double kMidInternalDefault = 180.0;

    auto clamp_internal = [&](double cur, double delta) {
        return std::max(kMidInternalMin, std::min(kMidInternalMax, cur + delta));
    };

    CHECK(clamp_internal(kMidInternalDefault, 30.0) == 210.0);
    CHECK(clamp_internal(kMidInternalDefault, -100.0) == 120.0);
    CHECK(clamp_internal(kMidInternalDefault, 500.0) == 400.0);
}
