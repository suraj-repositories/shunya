#include "tests/test_framework.hpp"
#include "shunya/mat/virtual_test_bench.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cmath>
#include <string>

using namespace shunya;
using namespace shunya::mat;

TEST_CASE("PH5-14.01: Virtual Test Bench Environmental Presets Calibration") {
    auto leo = VirtualTestBenchEngine::preset_spacecraft_leo();
    CHECK(leo.name == "Spacecraft (LEO)");
    CHECK(leo.pressure_pa < 1e-4);
    CHECK(leo.gravity_m_s2 == 0.0);
    CHECK(leo.temperature_k == 170.0);

    auto sea = VirtualTestBenchEngine::preset_deep_sea();
    CHECK(sea.name == "Deep Sea Submersible");
    CHECK(sea.pressure_pa >= 1e8); // 110 MPa
    CHECK(sea.salinity_ppt == 35.0);

    auto furnace = VirtualTestBenchEngine::preset_industrial_furnace();
    CHECK(furnace.name == "Industrial Furnace");
    CHECK(furnace.temperature_k == 1273.15);

    auto cable = VirtualTestBenchEngine::preset_underground_cable();
    CHECK(cable.electric_field_v_m >= 1e5);

    auto coastal = VirtualTestBenchEngine::preset_outdoor_coastal();
    CHECK(coastal.salinity_ppt == 10.0);
    CHECK(coastal.relative_humidity_pct == 85.0);
}

TEST_CASE("PH5-14.02: 7-Domain Multiphysics Qualification of Cu-Ni Alloy in Spacecraft LEO") {
    auto& db = shunya::matdb::ElementDatabase::global();
    auto load_res = db.load_default();
    REQUIRE(load_res.is_ok());

    MixtureExperiment exp;
    exp.name = "Cupronickel_70_30";
    exp.scenario = FormationScenario::Alloy;
    exp.temperature_k = 298.15;
    exp.pressure_pa = 101325.0;

    MixtureConstituentInput cu_in;
    cu_in.symbol_or_id = "Cu";
    cu_in.basis = QuantityInputBasis::MassFractionPct;
    cu_in.amount = 70.0;
    cu_in.is_matrix = true;
    exp.inputs.push_back(cu_in);

    MixtureConstituentInput ni_in;
    ni_in.symbol_or_id = "Ni";
    ni_in.basis = QuantityInputBasis::MassFractionPct;
    ni_in.amount = 30.0;
    ni_in.is_matrix = false;
    exp.inputs.push_back(ni_in);

    auto eval_res = MaterialMixingLab::evaluate(exp, db);
    REQUIRE(eval_res.is_ok());

    const auto& mix = eval_res.value();
    auto leo_env = VirtualTestBenchEngine::preset_spacecraft_leo();

    auto report = VirtualTestBenchEngine::qualify_material(mix, leo_env);
    CHECK(report.domain_results.size() == 7);
    CHECK(report.overall_qualification_passed);
    CHECK(report.aggregate_score >= 85.0);
    CHECK(report.critical_failures.empty());

    // Verify individual domain benchmark models:
    // 1. Thermal
    auto thm = report.domain_results[0];
    CHECK(thm.domain_name == "Thermal Environment");
    CHECK(thm.passed);

    // 2. Mechanical
    auto mech = report.domain_results[1];
    CHECK(mech.domain_name == "Mechanical & Gravity");
    CHECK(mech.passed);

    // 3. Vacuum & Space
    auto vac = report.domain_results[2];
    CHECK(vac.domain_name == "Vacuum & Space");
    CHECK(vac.passed);

    // 4. Electrical
    auto elec = report.domain_results[3];
    CHECK(elec.domain_name == "Electrical Current");
    CHECK(elec.passed);

    // 5. Caloric
    auto cal = report.domain_results[4];
    CHECK(cal.domain_name == "Caloric Heat");
    CHECK(cal.passed);

    // 6. Oxidation
    auto ox = report.domain_results[5];
    CHECK(ox.domain_name == "Rust & Oxidation Kinetics");
    CHECK(ox.passed);

    // 7. Extreme
    auto ext = report.domain_results[6];
    CHECK(ext.domain_name == "Extreme Environments");
    CHECK(ext.passed);
}

TEST_CASE("PH5-14.03: Environmental Stress Failure Detection (Steel in Saline Coastal Environment)") {
    auto& db = shunya::matdb::ElementDatabase::global();
    (void)db.load_default();

    MixtureExperiment exp;
    exp.name = "Unprotected_Carbon_Steel";
    exp.scenario = FormationScenario::Alloy;
    exp.temperature_k = 298.15;
    exp.pressure_pa = 101325.0;

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

    const auto& mix = eval_res.value();
    auto coastal_env = VirtualTestBenchEngine::preset_outdoor_coastal();

    auto report = VirtualTestBenchEngine::qualify_material(mix, coastal_env);
    // Unprotected carbon steel has PBR = 2.14 in 10 ppt coastal saline environment -> oxidation fails!
    CHECK(!report.overall_qualification_passed);
    CHECK(!report.critical_failures.empty());

    bool found_oxidation_failure = false;
    for (const auto& f : report.critical_failures) {
        if (f.find("Rust & Oxidation") != std::string::npos) {
            found_oxidation_failure = true;
        }
    }
    CHECK(found_oxidation_failure);
}
