#include "tests/test_framework.hpp"
#include "shunya/mat/experiment_persistence.hpp"
#include "shunya/matdb/element_database.hpp"

#include <filesystem>
#include <string>

using namespace shunya;
using namespace shunya::mat;

TEST_CASE("PH5-18.01: Experiment Serialization & Schema 5.0.0 Round-Trip") {
    VersionedExperimentBundle bundle;
    bundle.metadata.schema_version = "5.0.0";
    bundle.metadata.author = "Marie Curie";
    bundle.metadata.tags = {"Alloy", "High-Temperature", "Lab-Tested"};
    bundle.metadata.notes = "Superalloy formulation testing.";

    bundle.experiment.name = "Inconel_Test_Specimen";
    bundle.experiment.scenario = FormationScenario::Alloy;
    bundle.experiment.temperature_k = 1273.15;
    bundle.experiment.pressure_pa = 101325.0;

    MixtureConstituentInput in1;
    in1.symbol_or_id = "Ni";
    in1.amount = 70.0;
    bundle.experiment.inputs.push_back(in1);

    MixtureConstituentInput in2;
    in2.symbol_or_id = "Cr";
    in2.amount = 30.0;
    bundle.experiment.inputs.push_back(in2);

    std::string json_str = ExperimentPersistence::serialize_to_json(bundle);
    CHECK(!json_str.empty());
    CHECK(json_str.find("\"schemaVersion\": \"5.0.0\"") != std::string::npos);
    CHECK(json_str.find("\"author\": \"Marie Curie\"") != std::string::npos);
    CHECK(json_str.find("Inconel_Test_Specimen") != std::string::npos);

    // Deserialize
    auto res = ExperimentPersistence::deserialize_from_json(json_str);
    REQUIRE(res.is_ok());

    const auto& loaded = res.value();
    CHECK(loaded.metadata.author == "Marie Curie");
    CHECK(loaded.metadata.tags.size() == 3);
    CHECK(loaded.experiment.name == "Inconel_Test_Specimen");
    CHECK(loaded.experiment.scenario == FormationScenario::Alloy);
    CHECK(loaded.experiment.temperature_k == 1273.15);
    CHECK(loaded.experiment.inputs.size() == 2);
    CHECK(loaded.experiment.inputs[0].symbol_or_id == "Ni");
}

TEST_CASE("PH5-18.02: Atomic File Save, Trash & Restore") {
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "shunya_test_p5_persist";
    std::filesystem::path trash_dir = test_dir / ".trash";
    std::filesystem::create_directories(test_dir);

    VersionedExperimentBundle bundle;
    bundle.experiment.name = "Atomic_Save_Test";
    bundle.experiment.scenario = FormationScenario::PhysicalMixture;

    std::filesystem::path file_path = test_dir / "atomic_exp.shunya-mix.json";

    // 1. Atomic Save
    auto save_res = ExperimentPersistence::save_atomic(file_path, bundle);
    REQUIRE(save_res.is_ok());
    CHECK(std::filesystem::exists(file_path));

    // 2. Load
    auto load_res = ExperimentPersistence::load_file(file_path);
    REQUIRE(load_res.is_ok());
    CHECK(load_res.value().experiment.name == "Atomic_Save_Test");

    // 3. Delete to Trash
    auto trash_res = ExperimentPersistence::delete_to_trash(file_path, trash_dir);
    REQUIRE(trash_res.is_ok());
    CHECK(!std::filesystem::exists(file_path));
    CHECK(std::filesystem::exists(trash_res.value()));

    // 4. Restore from Trash
    auto restore_res = ExperimentPersistence::restore_from_trash(trash_res.value(), test_dir);
    REQUIRE(restore_res.is_ok());
    CHECK(std::filesystem::exists(file_path));

    // Cleanup
    std::filesystem::remove_all(test_dir);
}

TEST_CASE("PH5-18.03: Schema Migration from Legacy Formats") {
    // Legacy Phase 3 / Phase 4 format without top-level schemaVersion or experiment wrapper
    std::string legacy_json = R"({
        "id": "12345678-1234-5678-1234-567812345678",
        "name": "Legacy_Steel_Recipe",
        "description": "Old phase format",
        "scenario": "Alloy",
        "temperature_k": 298.15,
        "pressure_pa": 101325.0,
        "inputs": [
            {"symbol_or_id": "Fe", "basis": "MassFractionPct", "amount": 99.0},
            {"symbol_or_id": "C", "basis": "MassFractionPct", "amount": 1.0}
        ]
    })";

    auto res = ExperimentPersistence::deserialize_from_json(legacy_json);
    REQUIRE(res.is_ok());

    const auto& loaded = res.value();
    CHECK(loaded.metadata.schema_version == "5.0.0"); // Automatically upgraded
    CHECK(loaded.experiment.name == "Legacy_Steel_Recipe");
    CHECK(loaded.experiment.inputs.size() == 2);
    CHECK(!loaded.validation_warnings.empty());
    CHECK(loaded.validation_warnings[0].find("Upgraded experiment schema") != std::string::npos);
}

TEST_CASE("PH5-18.04: Name Conflict Resolution") {
    std::vector<std::string> existing = {
        "Bronze_Standard",
        "Bronze_Standard_1",
        "Bronze_Standard_2",
        "Brass_Alpha"
    };

    std::string resolved1 = ExperimentPersistence::resolve_name_conflict("Brass_Beta", existing);
    CHECK(resolved1 == "Brass_Beta");

    std::string resolved2 = ExperimentPersistence::resolve_name_conflict("Bronze_Standard", existing);
    CHECK(resolved2 == "Bronze_Standard_3");
}
