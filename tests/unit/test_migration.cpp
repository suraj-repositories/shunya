#include "tests/test_framework.hpp"
#include "shunya/core/domain/migration.hpp"
#include "shunya/core/domain/project.hpp"

#include <filesystem>
#include <fstream>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("ProjectMigrator schema version extraction") {
    std::string modern_json = R"({
        "schema_version": "0.1.0-dev",
        "id": "12345678-1234-4234-8234-123456789abc",
        "name": "Modern Project"
    })";

    auto ver_res = ProjectMigrator::extract_schema_version(modern_json);
    REQUIRE(ver_res.is_ok());
    CHECK(ver_res.value().major == 0);
    CHECK(ver_res.value().minor == 1);
    CHECK(ver_res.value().patch == 0);

    std::string legacy_json = R"({
        "id": "12345678-1234-4234-8234-123456789abc",
        "name": "Legacy Project Without Version"
    })";

    auto legacy_ver = ProjectMigrator::extract_schema_version(legacy_json);
    REQUIRE(legacy_ver.is_ok());
    CHECK(legacy_ver.value().major == 0);
    CHECK(legacy_ver.value().minor == 0);
    CHECK(legacy_ver.value().patch == 1);
}

TEST_CASE("ProjectMigrator migration planning") {
    ProjectMigrator migrator;

    base::Version v001{0, 0, 1, "alpha"};
    base::Version v010{0, 1, 0, "dev"};
    base::Version v999{9, 9, 9, "future"};

    CHECK(migrator.can_migrate(v001, v010));
    CHECK(!migrator.can_migrate(v001, v999));

    auto plan = migrator.create_plan(v001, v010);
    CHECK(plan.is_valid);
    CHECK(plan.step_descriptions.size() == 1);

    auto invalid_plan = migrator.create_plan(v001, v999);
    CHECK(!invalid_plan.is_valid);
}

TEST_CASE("ProjectMigrator preview migration") {
    ProjectMigrator migrator;
    std::string legacy_json = R"({
  "id": "a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11",
  "name": "Legacy Gearbox"
})";

    base::Version target_v{0, 1, 0, "dev"};
    auto preview_res = migrator.preview_migration(legacy_json, target_v);
    REQUIRE(preview_res.is_ok());

    const auto& migrated = preview_res.value();
    CHECK(migrated.find("\"schema_version\": \"0.1.0-dev\"") != std::string::npos);
    CHECK(migrated.find("\"target_fidelity\": \"L2_EngineeringApproximation\"") != std::string::npos);
    CHECK(migrated.find("\"Legacy Gearbox\"") != std::string::npos);

    // Verify migrated preview can be parsed as a valid ProjectManifest
    auto parse_res = ProjectManifest::from_json(migrated);
    REQUIRE(parse_res.is_ok());
    CHECK(parse_res.value().name == "Legacy Gearbox");
    CHECK(parse_res.value().target_fidelity == FidelityTier::L2_EngineeringApproximation);
}

TEST_CASE("ProjectMigrator file migration, backup and rollback") {
    std::string test_dir = "test_artifacts";
    std::filesystem::create_directories(test_dir);
    std::string file_path = test_dir + "/legacy_project.shunya";
    std::string backup_path = file_path + ".bak";

    // Clean any prior state
    std::filesystem::remove(file_path);
    std::filesystem::remove(backup_path);

    std::string original_content = R"({
  "id": "11111111-2222-4333-8444-555555555555",
  "name": "Legacy Turbocharger",
  "description": "Pre-v0.1 simulation model"
})";

    {
        std::ofstream out(file_path, std::ios::out);
        out << original_content;
    }

    ProjectMigrator migrator;
    base::Version target_v{0, 1, 0, "dev"};

    // Migrate file with backup
    auto mig_res = migrator.migrate_file_with_backup(file_path, target_v, true);
    REQUIRE(mig_res.is_ok());

    CHECK(std::filesystem::exists(file_path));
    CHECK(std::filesystem::exists(backup_path));

    // Verify migrated file can be opened directly as Project
    auto proj_res = Project::open_from_file(file_path);
    REQUIRE(proj_res.is_ok());
    CHECK(proj_res.value()->manifest().name == "Legacy Turbocharger");
    CHECK(proj_res.value()->manifest().target_fidelity == FidelityTier::L2_EngineeringApproximation);

    // Verify backup still contains the exact original content
    {
        std::ifstream bak_in(backup_path);
        std::ostringstream ss;
        ss << bak_in.rdbuf();
        CHECK(ss.str() == original_content);
    }

    // Rollback from backup
    auto rollback_res = migrator.rollback_from_backup(file_path);
    REQUIRE(rollback_res.is_ok());

    // Verify file content is restored to original
    {
        std::ifstream cur_in(file_path);
        std::ostringstream ss;
        ss << cur_in.rdbuf();
        CHECK(ss.str() == original_content);
    }

    // Clean up
    std::filesystem::remove(file_path);
    std::filesystem::remove(backup_path);
}

TEST_CASE("ProjectMigrator error handling") {
    ProjectMigrator migrator;

    // Non-existent file
    auto fail_file = migrator.migrate_file_with_backup("nonexistent_path_dummy.shunya", {0, 1, 0, "dev"});
    CHECK(fail_file.is_err());
    CHECK(fail_file.error().code == 2302);

    // Rollback without backup
    auto fail_rollback = migrator.rollback_from_backup("nonexistent_file_no_backup.shunya");
    CHECK(fail_rollback.is_err());
    CHECK(fail_rollback.error().code == 2303);
}
