#include "tests/test_framework.hpp"
#include "shunya/core/domain/packaging.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"

#include <filesystem>
#include <fstream>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("PackageManifest serialization roundtrip") {
    PackageManifest manifest;
    manifest.package_id = base::Uuid::generate_random();
    manifest.project_id = base::Uuid::generate_random();
    manifest.project_name = "Subsea Turbine";
    manifest.created_at = "2026-11-06T22:00:00Z";
    manifest.entries = {
        {"project.json", PackageEntryType::Manifest, 512, 12345678},
        {"scene.json", PackageEntryType::Scene, 1024, 87654321},
        {"assets/blade.step", PackageEntryType::Asset, 204800, 11223344}
    };

    std::string json = manifest.to_json();
    CHECK(!json.empty());
    CHECK(json.find("\"Subsea Turbine\"") != std::string::npos);

    auto parsed_res = PackageManifest::from_json(json);
    REQUIRE(parsed_res.is_ok());
    const auto& parsed = parsed_res.value();
    CHECK(parsed.package_id == manifest.package_id);
    CHECK(parsed.project_id == manifest.project_id);
    CHECK(parsed.project_name == "Subsea Turbine");
    REQUIRE(parsed.entries.size() == 3);
    CHECK(parsed.entries[2].relative_path == "assets/blade.step");
    CHECK(parsed.entries[2].size_bytes == 204800);
}

TEST_CASE("ProjectPackager packaging and unpacking lifecycle") {
    std::string test_dir = "test_artifacts/packaging_test";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    std::string src_asset_dir = test_dir + "/source_assets";
    std::filesystem::create_directories(src_asset_dir);

    // Create mock source assets
    std::string cad_file = src_asset_dir + "/airfoil.step";
    {
        std::ofstream out(cad_file, std::ios::out | std::ios::binary | std::ios::trunc);
        out << "ISO-10303-21; CAD WING DATA;";
    }
    std::string script_file = src_asset_dir + "/control.py";
    {
        std::ofstream out(script_file, std::ios::out | std::ios::binary | std::ios::trunc);
        out << "def update_thrust(): pass\n";
    }

    // Create mock project and scene
    auto proj = Project::create_new("Aerospace Wing Package", "Aero Lab").value();
    SceneGraph scene;
    auto root = scene.create_entity("Wing Assembly").value();
    auto wing = scene.create_entity("Wing Segment", root->id()).value();
    wing->set_attachment("cad_mesh", "airfoil.step");
    wing->set_attachment("script_file", "control.py");

    std::string pkg_dir = test_dir + "/exported_pkg";
    ProjectPackager packager;

    // Create package
    auto pkg_res = packager.create_package(*proj, scene, src_asset_dir, pkg_dir);
    REQUIRE(pkg_res.is_ok());

    CHECK(std::filesystem::exists(pkg_dir + "/package_manifest.json"));
    CHECK(std::filesystem::exists(pkg_dir + "/project.json"));
    CHECK(std::filesystem::exists(pkg_dir + "/scene.json"));
    CHECK(std::filesystem::exists(pkg_dir + "/assets/airfoil.step"));
    CHECK(std::filesystem::exists(pkg_dir + "/scripts/control.py"));

    // Verify package integrity
    auto report = packager.verify_package(pkg_dir);
    CHECK(report.is_valid());
    CHECK(report.error_count() == 0);

    // Unpack and load
    auto load_res = packager.unpack_and_load(pkg_dir);
    REQUIRE(load_res.is_ok());
    auto [loaded_proj, loaded_scene] = std::move(load_res.value());

    CHECK(loaded_proj->manifest().name == "Aerospace Wing Package");
    CHECK(loaded_scene->entity_count() == 2);
    auto found_wing = loaded_scene->find_by_name("Wing Segment");
    REQUIRE(found_wing.size() == 1);
    CHECK(found_wing[0]->get_attachment("cad_mesh").value() == "airfoil.step");

    // Corruption test: tamper with asset file
    {
        std::ofstream tamper(pkg_dir + "/assets/airfoil.step", std::ios::app | std::ios::binary);
        tamper << "TAMPERED BYTES";
    }
    auto corrupted_rep = packager.verify_package(pkg_dir);
    CHECK(!corrupted_rep.is_valid());
    CHECK(corrupted_rep.has_error(2513) || corrupted_rep.has_error(2514));

    // Cleanup
    std::filesystem::remove_all(test_dir);
}
