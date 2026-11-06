#include "tests/test_framework.hpp"
#include "shunya/core/domain/validator.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"

#include <filesystem>
#include <fstream>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("ProjectValidator nominal clean project") {
    auto proj_res = Project::create_new("Clean Drone Simulation", "Aero Engineer");
    REQUIRE(proj_res.is_ok());
    auto proj = proj_res.value();

    SceneGraph scene;
    auto root = scene.create_entity("Chassis").value();
    auto motor = scene.create_entity("Motor", root->id()).value();
    motor->set_attachment("type", "brushless");

    ProjectValidator validator;
    auto report = validator.validate(*proj, scene);
    CHECK(report.is_valid());
    CHECK(!report.has_errors());
    CHECK(!report.has_warnings());
    CHECK(report.error_count() == 0);
}

TEST_CASE("ProjectValidator manifest anomaly detection") {
    ProjectValidator validator;

    // 1. Empty name
    ProjectManifest m1;
    m1.id = base::Uuid::generate_random();
    m1.name = "";
    auto rep1 = validator.validate_manifest(m1);
    CHECK(!rep1.is_valid());
    CHECK(rep1.has_error(2401));

    // 2. Nil UUID
    ProjectManifest m2;
    m2.id = base::Uuid{}; // nil
    m2.name = "Nil Project";
    auto rep2 = validator.validate_manifest(m2);
    CHECK(!rep2.is_valid());
    CHECK(rep2.has_error(2402));

    // 3. Incompatible future major version
    ProjectManifest m3;
    m3.id = base::Uuid::generate_random();
    m3.name = "Future Major Project";
    m3.schema_version = base::Version(99, 0, 0, "future");
    auto rep3 = validator.validate_manifest(m3);
    CHECK(!rep3.is_valid());
    CHECK(rep3.has_error(2403));
}

TEST_CASE("ProjectValidator scene graph missing asset detection") {
    std::string test_dir = "test_artifacts/validator_test";
    std::filesystem::create_directories(test_dir);
    std::string existing_model = test_dir + "/rotor.step";
    {
        std::ofstream out(existing_model);
        out << "STEP-21 CAD DATA";
    }

    SceneGraph scene;
    auto root = scene.create_entity("Root Assembly").value();

    // Entity with existing relative asset
    auto rotor = scene.create_entity("Rotor", root->id()).value();
    rotor->set_attachment("cad_mesh", "rotor.step");

    // Entity with non-existent relative asset
    auto stator = scene.create_entity("Stator", root->id()).value();
    stator->set_attachment("cad_mesh", "missing_stator_model.step");

    // Entity with external URI scheme (should not check local disk)
    auto remote_sensor = scene.create_entity("Sensor", root->id()).value();
    remote_sensor->set_attachment("step_file", "https://models.example.com/sensor.step");

    ProjectValidationOptions opts;
    opts.check_asset_files_exist = true;
    opts.project_root_path = test_dir;

    ProjectValidator validator;
    auto report = validator.validate_scene_graph(scene, opts);

    CHECK(!report.is_valid());
    CHECK(report.has_error(2415)); // Asset file not found
    CHECK(report.error_count() == 1);

    // Clean up
    std::filesystem::remove(existing_model);
    std::filesystem::remove(test_dir);
}
