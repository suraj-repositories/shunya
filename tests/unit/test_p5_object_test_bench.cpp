#include "tests/test_framework.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/object_test_bench.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/cavity_physics.hpp"

#include <cmath>

using namespace shunya;

TEST_CASE("PH5-26.01: Incandescent Light Bulb Electrical & Thermal Qualification at 120V") {
    core::domain::MaterialRegistry reg;

    // 1. Build canonical A19 Incandescent Light Bulb
    eng::PhysicalObject bulb;
    bulb.id = core::base::Uuid::generate_random();
    bulb.name = "Incandescent_Bulb_A19";
    bulb.geometry = eng::ObjectGeometrySpec::make_box(0.06, 0.06, 0.11);
    bulb.operating_temp_k = 2750.0;
    bulb.computed.mass_kg = 0.035; // 35 grams
    bulb.computed.surface_area_m2 = 0.012; // 120 cm^2
    bulb.computed.axial_electrical_resistance_ohm = 240.0; // 240 Ohm hot

    // Terminals (+V, GND)
    bulb.terminals.push_back({"+V", {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 120.0, true});
    bulb.terminals.push_back({"GND", {0.0, 0.0, -0.02}, {0.0, 0.0, -1.0}, 0.0, true});

    // Cavity: 80 kPa Argon/Nitrogen fill, 0.8 mm glass shell
    bulb.cavity.kind = eng::CavityKind::FillGas;
    bulb.cavity.fill_pressure_pa = 80000.0;
    bulb.cavity.fill_temp_k = 293.15;
    bulb.cavity.wall_thickness_m = 0.0008;
    bulb.cavity.envelope_radius_m = 0.030;

    // Run Standard Room Preset (120 V applied)
    auto stimuli = eng::ObjectTestBenchEngine::preset_standard_room();
    auto report = eng::ObjectTestBenchEngine::qualify_object(bulb, reg, stimuli);

    CHECK(report.overall_passed == true);
    CHECK(report.composite_score >= 85.0);
    CHECK(report.domains.size() == 5);

    // 2. Electrical Domain Verification
    auto elec = eng::ObjectTestBenchEngine::evaluate_electrical(bulb, reg, stimuli);
    CHECK(elec.passed == true);
    CHECK(elec.score >= 90.0);
    // At 120 V and 240 Ohm, I = 0.50 A, P = 60 W
    CHECK(elec.metric_summary.find("120.0 V") != std::string::npos);
    CHECK(elec.metric_summary.find("0.50 A") != std::string::npos);
    CHECK(elec.metric_summary.find("60.0 W") != std::string::npos);

    // 3. Thermal Domain Verification
    auto therm = eng::ObjectTestBenchEngine::evaluate_thermal(bulb, reg, stimuli);
    CHECK(therm.passed == true);
    // Core temperature ~2750 K, envelope ~380 K (below 820 K softening limit)
    CHECK(therm.metric_summary.find("2750 K") != std::string::npos);
    CHECK(therm.metric_summary.find("380 K") != std::string::npos);
    CHECK(therm.metric_summary.find("Limit: 820") != std::string::npos);
}

TEST_CASE("PH5-26.02: Mechanical, Pressure & Environmental Degradation Qualification") {
    core::domain::MaterialRegistry reg;

    eng::PhysicalObject bulb;
    bulb.id = core::base::Uuid::generate_random();
    bulb.name = "Incandescent_Bulb_A19";
    bulb.geometry = eng::ObjectGeometrySpec::make_box(0.06, 0.06, 0.11);
    bulb.computed.mass_kg = 0.035;
    bulb.computed.axial_electrical_resistance_ohm = 240.0;
    bulb.terminals.push_back({"+V", {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 120.0, true});
    bulb.terminals.push_back({"GND", {0.0, 0.0, -0.02}, {0.0, 0.0, -1.0}, 0.0, true});

    bulb.cavity.kind = eng::CavityKind::FillGas;
    bulb.cavity.fill_pressure_pa = 80000.0;
    bulb.cavity.fill_temp_k = 293.15;
    bulb.cavity.wall_thickness_m = 0.0008;
    bulb.cavity.envelope_radius_m = 0.030;

    auto stimuli = eng::ObjectTestBenchEngine::preset_standard_room();

    // 1. Mechanical Qualification
    auto mech = eng::ObjectTestBenchEngine::evaluate_mechanical(bulb, reg, stimuli);
    CHECK(mech.passed == true);
    // Deflection should be micro-scale for 35 gram object
    CHECK(mech.details.size() >= 2);

    // 2. Pressure / Vacuum Qualification
    auto press = eng::ObjectTestBenchEngine::evaluate_pressure_vacuum(bulb, reg, stimuli);
    CHECK(press.passed == true);
    // Delta P between 80 kPa and 101.325 kPa ambient is ~21.3 kPa
    CHECK(press.metric_summary.find("Delta P") != std::string::npos);
    CHECK(press.metric_summary.find("Hoop Stress") != std::string::npos);

    // 3. Environmental Degradation Qualification
    auto env_deg = eng::ObjectTestBenchEngine::evaluate_environmental_degradation(bulb, reg, stimuli);
    CHECK(env_deg.passed == true);
    CHECK(env_deg.metric_summary.find("Corrosion Rate") != std::string::npos);
}

TEST_CASE("PH5-26.03: Missing Terminals Warning & Qualification Dossier Export") {
    core::domain::MaterialRegistry reg;

    // Object with NO electrical terminals
    eng::PhysicalObject insulator_block;
    insulator_block.id = core::base::Uuid::generate_random();
    insulator_block.name = "Ceramic_Spacer";
    insulator_block.geometry = eng::ObjectGeometrySpec::make_box(0.05, 0.05, 0.05);
    insulator_block.computed.mass_kg = 0.12;

    auto stimuli = eng::ObjectTestBenchEngine::preset_standard_room();

    // Running electrical qualification on an object without terminals must flag warning
    auto elec = eng::ObjectTestBenchEngine::evaluate_electrical(insulator_block, reg, stimuli);
    CHECK(elec.warnings.empty() == false);
    CHECK(elec.warnings[0].find("no electrical terminals") != std::string::npos);

    // Complete qualification report serialization
    auto rep = eng::ObjectTestBenchEngine::qualify_object(insulator_block, reg, stimuli);
    CHECK(rep.curve_temperature_vs_time.size() == 21);
    CHECK(rep.curve_voltage_vs_current.size() == 21);
    CHECK(rep.curve_deflection_vs_load.size() == 21);

    // Markdown dossier export
    std::string md = rep.to_markdown();
    CHECK(md.find("# Multiphysics Qualification Dossier: Ceramic_Spacer") != std::string::npos);
    CHECK(md.find("## Physics Domain Scorecards") != std::string::npos);
    CHECK(md.find("Thermal") != std::string::npos);
    CHECK(md.find("Mechanical") != std::string::npos);

    // JSON export
    std::string json = rep.to_json();
    CHECK(json.find("\"objectName\": \"Ceramic_Spacer\"") != std::string::npos);
    CHECK(json.find("\"compositeScore\"") != std::string::npos);
}
