#include "tests/test_framework.hpp"
#include "shunya/core/domain/project.hpp"

#include <filesystem>
#include <iostream>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("ProjectManifest serialization and deserialization roundtrip") {
    ProjectManifest manifest;
    manifest.id = base::Uuid::generate_random();
    manifest.name = "Aerodynamic Wing Profile";
    manifest.description = "Transonic wing study under Mach 0.85";
    manifest.author = "Dr. Engineering";
    manifest.created_at = "2026-11-06T20:00:00Z";
    manifest.updated_at = "2026-11-06T20:30:00Z";
    manifest.schema_version = base::Version(1, 2, 3, "rc1");
    manifest.target_fidelity = FidelityTier::L3_HighFidelity;
    manifest.dependencies = {
        {"NACA_Airfoil_Lib", "2.1.0", "shunya://libs/naca"},
        {"Atmospheric_Standard_1976", "1.0.0", "shunya://libs/atmos"}
    };
    manifest.metadata["solver"] = "OpenFOAM";
    manifest.metadata["mesh_density"] = "Fine";

    std::string json = manifest.to_json();
    CHECK(!json.empty());
    CHECK(json.find("\"Aerodynamic Wing Profile\"") != std::string::npos);
    CHECK(json.find("L3_HighFidelity") != std::string::npos);

    auto parsed_res = ProjectManifest::from_json(json);
    REQUIRE(parsed_res.is_ok());

    const auto& parsed = parsed_res.value();
    CHECK(parsed.id == manifest.id);
    CHECK(parsed.name == "Aerodynamic Wing Profile");
    CHECK(parsed.description == "Transonic wing study under Mach 0.85");
    CHECK(parsed.author == "Dr. Engineering");
    CHECK(parsed.created_at == "2026-11-06T20:00:00Z");
    CHECK(parsed.updated_at == "2026-11-06T20:30:00Z");
    CHECK(parsed.schema_version.major == 1);
    CHECK(parsed.schema_version.minor == 2);
    CHECK(parsed.schema_version.patch == 3);
    CHECK(parsed.target_fidelity == FidelityTier::L3_HighFidelity);
    REQUIRE(parsed.dependencies.size() == 2);
    CHECK(parsed.dependencies[0].name == "NACA_Airfoil_Lib");
    CHECK(parsed.dependencies[0].version == "2.1.0");
    CHECK(parsed.dependencies[1].name == "Atmospheric_Standard_1976");
    CHECK(parsed.metadata.at("solver") == "OpenFOAM");
    CHECK(parsed.metadata.at("mesh_density") == "Fine");
}

TEST_CASE("ProjectManifest invalid json handling") {
    auto empty_res = ProjectManifest::from_json("");
    CHECK(empty_res.is_err());
    CHECK(empty_res.error().code == 2001);

    auto malformed_res = ProjectManifest::from_json("{\"name\": \"Invalid\"}");
    CHECK(malformed_res.is_err());
    CHECK(malformed_res.error().code == 2002);
}

TEST_CASE("Project lifecycle create_new") {
    auto proj_res = Project::create_new("Electric Drivetrain", "Lead Motor Dev");
    REQUIRE(proj_res.is_ok());
    auto proj = proj_res.value();

    CHECK(proj->manifest().name == "Electric Drivetrain");
    CHECK(proj->manifest().author == "Lead Motor Dev");
    CHECK(proj->is_dirty());
    CHECK(!proj->is_read_only());
    CHECK(proj->file_path().empty());

    // Empty name error handling
    auto fail_res = Project::create_new("");
    CHECK(fail_res.is_err());
    CHECK(fail_res.error().code == 2010);
}

TEST_CASE("Project save and reopen lifecycle") {
    std::string test_dir = "test_artifacts";
    std::filesystem::create_directories(test_dir);
    std::string test_file = test_dir + "/sample_project.shunya";

    // Clean up if already exists
    std::filesystem::remove(test_file);

    auto proj_res = Project::create_new("Thermal Heat Exchanger", "Thermodynamics Team");
    REQUIRE(proj_res.is_ok());
    auto proj = proj_res.value();
    proj->manifest().description = "Counter-flow heat exchanger efficiency simulation";
    proj->manifest().target_fidelity = FidelityTier::L4_ResearchCertified;
    proj->manifest().metadata["fluid_a"] = "Water";
    proj->manifest().metadata["fluid_b"] = "Ethylene Glycol";

    // Attempt saving with empty path and no prior path
    auto save_no_path = proj->save_to_file("");
    CHECK(save_no_path.is_err());
    CHECK(save_no_path.error().code == 2014);

    // Atomic save to file
    auto save_res = proj->save_to_file(test_file, true);
    REQUIRE(save_res.is_ok());
    CHECK(!proj->is_dirty());
    CHECK(proj->file_path() == test_file);
    CHECK(std::filesystem::exists(test_file));

    // Reopen project from file
    auto open_res = Project::open_from_file(test_file);
    REQUIRE(open_res.is_ok());
    auto reopened = open_res.value();

    CHECK(!reopened->is_dirty());
    CHECK(reopened->file_path() == test_file);
    CHECK(reopened->manifest().name == "Thermal Heat Exchanger");
    CHECK(reopened->manifest().description == "Counter-flow heat exchanger efficiency simulation");
    CHECK(reopened->manifest().author == "Thermodynamics Team");
    CHECK(reopened->manifest().target_fidelity == FidelityTier::L4_ResearchCertified);
    CHECK(reopened->manifest().metadata.at("fluid_a") == "Water");
    CHECK(reopened->manifest().metadata.at("fluid_b") == "Ethylene Glycol");

    // Overwrite existing file test (atomic)
    reopened->manifest().description = "Updated efficiency simulation parameters";
    auto overwrite_res = reopened->save_to_file(test_file, true);
    CHECK(overwrite_res.is_ok());

    // Verify update persisted
    auto reopen2 = Project::open_from_file(test_file);
    REQUIRE(reopen2.is_ok());
    CHECK(reopen2.value()->manifest().description == "Updated efficiency simulation parameters");

    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("Project read-only protection") {
    std::string test_dir = "test_artifacts";
    std::filesystem::create_directories(test_dir);
    std::string test_file = test_dir + "/readonly_project.shunya";

    auto proj_res = Project::create_new("Protected Assembly");
    REQUIRE(proj_res.is_ok());
    auto proj = proj_res.value();
    auto save_res = proj->save_to_file(test_file);
    REQUIRE(save_res.is_ok());

    // Mark read only
    proj->set_read_only(true);
    CHECK(proj->is_read_only());

    // Save attempt should fail
    auto blocked_save = proj->save_to_file(test_file);
    CHECK(blocked_save.is_err());
    CHECK(blocked_save.error().code == 2013);

    // Unset read only and save succeeds
    proj->set_read_only(false);
    auto ok_save = proj->save_to_file(test_file);
    CHECK(ok_save.is_ok());

    std::filesystem::remove(test_file);
}

TEST_CASE("Project open nonexistent file") {
    auto res = Project::open_from_file("nonexistent_path_shunya_test_dummy.shunya");
    CHECK(res.is_err());
    CHECK(res.error().code == 2011);
}
