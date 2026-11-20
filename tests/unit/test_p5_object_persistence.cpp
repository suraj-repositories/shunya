#include "tests/test_framework.hpp"
#include "shunya/eng/object_persistence.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/core/domain/material_assignment.hpp"

#include <filesystem>
#include <fstream>

using namespace shunya;

TEST_CASE("PH5-29.01: Object Serialization, Schema 5.0.0 & Atomic Disk Save") {
    core::domain::MaterialRegistry mat_reg;
    core::domain::Material mat_copper(core::base::Uuid::generate_random(), "Electrolytic_Copper",
                                      core::domain::MaterialPhysicalState::Solid, 8960.0);
    const auto copper_id = mat_copper.id();
    REQUIRE(mat_reg.register_material(std::move(mat_copper)).is_ok());

    eng::PhysicalObject obj;
    obj.name = "Copper_Heatsink";
    obj.category = "Thermal";
    obj.geometry = eng::ObjectGeometrySpec::make_box(0.05, 0.02, 0.05);

    eng::ObjectRegion r;
    r.region_id = "fin_array";
    r.name = "Copper Fins";
    r.material_id = copper_id;
    r.volume_fraction = 1.0;
    r.is_conductive_path = true;
    obj.regions.push_back(r);

    eng::ObjectTerminal t1;
    t1.name = "GND";
    t1.position = {0.0, -0.01, 0.0};
    obj.terminals.push_back(t1);

    // 1. Bundle Creation & Serialization
    eng::VersionedObjectBundle bundle;
    bundle.object = obj;
    bundle.metadata.author = "Thermal Specialist";
    bundle.metadata.notes = "High-conductivity heatsink model";
    bundle.metadata.application_version = "5.0.0";
    if (const auto* mat = mat_reg.get_material(copper_id)) {
        bundle.bundled_materials.push_back(*mat);
    }

    std::string json = eng::ObjectPersistence::serialize_object_bundle(bundle);
    CHECK(!json.empty());
    CHECK(json.find("\"schemaVersion\": \"5.0.0\"") != std::string::npos);
    CHECK(json.find("\"name\": \"Copper_Heatsink\"") != std::string::npos);
    CHECK(json.find("\"bundledMaterials\"") != std::string::npos);

    // 2. Deserialization
    auto des_res = eng::ObjectPersistence::deserialize_object_bundle(json);
    REQUIRE(des_res.is_ok());
    const auto& loaded_bundle = des_res.value();
    CHECK(loaded_bundle.object.name == "Copper_Heatsink");
    CHECK(loaded_bundle.object.regions.size() == 1u);
    CHECK(loaded_bundle.bundled_materials.size() == 1u);
    CHECK(loaded_bundle.bundled_materials[0].name() == "Electrolytic_Copper");

    // 3. Atomic Disk Write & Load
    std::filesystem::path test_dir = std::filesystem::current_path() / "test_scratch_persist";
    std::filesystem::create_directories(test_dir);
    std::filesystem::path file_path = test_dir / "heatsink.shunya-obj.json";

    auto save_res = eng::ObjectPersistence::save_object_atomic(file_path, bundle);
    REQUIRE(save_res.is_ok());
    CHECK(std::filesystem::exists(file_path));

    auto load_res = eng::ObjectPersistence::load_object_file(file_path);
    REQUIRE(load_res.is_ok());
    CHECK(load_res.value().object.name == "Copper_Heatsink");

    // 4. Dirty State Tracking
    eng::ObjectWorkspaceSession session;
    CHECK(!session.is_dirty(obj.id));
    session.mark_dirty(obj.id);
    CHECK(session.is_dirty(obj.id));
    session.mark_clean(obj.id);
    CHECK(!session.is_dirty(obj.id));

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
}

TEST_CASE("PH5-29.02: Safe File Exchange, Trash/Undo & Missing Material Resolution") {
    // 1. Setup Source Workspace with a custom synthetic material
    core::domain::MaterialRegistry source_mat_reg;
    core::domain::Material custom_alloy(core::base::Uuid::generate_random(), "Aerogel_Insulator_X1",
                                        core::domain::MaterialPhysicalState::Solid, 120.0);
    const auto alloy_id = custom_alloy.id();
    REQUIRE(source_mat_reg.register_material(std::move(custom_alloy)).is_ok());

    eng::PhysicalObjectManager source_mgr;
    eng::PhysicalObject thermal_shield;
    thermal_shield.name = "Cryo_Thermal_Shield";
    thermal_shield.geometry = eng::ObjectGeometrySpec::make_cylinder(0.040, 0.080);
    eng::ObjectRegion r_shield;
    r_shield.name = "aerogel_core";
    r_shield.material_id = alloy_id;
    r_shield.volume_fraction = 1.0;
    thermal_shield.regions.push_back(r_shield);
    REQUIRE(source_mgr.add_object(thermal_shield, source_mat_reg).is_ok());

    // Export bundle with bundled materials
    eng::VersionedObjectBundle export_bundle;
    export_bundle.object = thermal_shield;
    export_bundle.metadata.author = "Cryogenics Lab";
    export_bundle.bundled_materials.push_back(*source_mat_reg.get_material(alloy_id));

    std::string exported_json = eng::ObjectPersistence::serialize_object_bundle(export_bundle);

    // 2. Target Workspace: DOES NOT HAVE Aerogel_Insulator_X1!
    core::domain::MaterialRegistry target_mat_reg;
    eng::PhysicalObjectManager target_mgr;
    CHECK(!target_mat_reg.has_material(alloy_id));

    // 3. Pre-Import Preview Generation
    auto prev_res = eng::ObjectPersistence::generate_import_preview(exported_json, target_mat_reg);
    REQUIRE(prev_res.is_ok());
    const auto& prev = prev_res.value();
    CHECK(prev.name == "Cryo_Thermal_Shield");
    CHECK(prev.bundled_materials_count == 1u);
    CHECK(prev.material_names.size() == 1u);
    CHECK(prev.material_names[0].find("bundled") != std::string::npos);

    // 4. Import & Automatic Material Resolution
    auto imp_res = eng::ObjectPersistence::import_and_resolve_materials(
        export_bundle, target_mgr, target_mat_reg, eng::NameConflictResolution::KeepBoth);
    REQUIRE(imp_res.is_ok());

    // Verify custom material was injected into target registry
    CHECK(target_mat_reg.has_material(alloy_id));
    const auto* imported_mat = target_mat_reg.get_material(alloy_id);
    REQUIRE(imported_mat != nullptr);
    CHECK(imported_mat->name() == "Aerogel_Insulator_X1");
    CHECK(std::abs(imported_mat->density() - 120.0) < 1e-3);

    // Verify imported object in target manager
    const auto* target_obj = target_mgr.find_by_name("Cryo_Thermal_Shield");
    REQUIRE(target_obj != nullptr);
    CHECK(target_obj->computed.valid);
    CHECK(target_obj->computed.mass_kg > 0.0);

    // 5. Name Conflict Resolution (Duplicate Import with KeepBoth)
    auto imp_dup = eng::ObjectPersistence::import_and_resolve_materials(
        export_bundle, target_mgr, target_mat_reg, eng::NameConflictResolution::KeepBoth);
    REQUIRE(imp_dup.is_ok());
    CHECK(imp_dup.value().name == "Cryo_Thermal_Shield (1)");

    // 6. Trash & Restore Protocol
    std::filesystem::path scratch_root = std::filesystem::current_path() / "test_scratch_trash";
    std::filesystem::path obj_dir = scratch_root / "objects";
    std::filesystem::path trash_dir = scratch_root / ".trash";
    std::filesystem::create_directories(obj_dir);
    std::filesystem::create_directories(trash_dir);

    std::filesystem::path file_to_delete = obj_dir / "Cryo_Thermal_Shield.shunya-obj.json";
    REQUIRE(eng::ObjectPersistence::save_object_atomic(file_to_delete, export_bundle).is_ok());
    CHECK(std::filesystem::exists(file_to_delete));

    // Delete to trash
    auto del_res = eng::ObjectPersistence::delete_to_trash(file_to_delete, trash_dir);
    REQUIRE(del_res.is_ok());
    CHECK(!std::filesystem::exists(file_to_delete));
    CHECK(std::filesystem::exists(del_res.value()));

    // Restore from trash
    auto rest_res = eng::ObjectPersistence::restore_from_trash(del_res.value(), obj_dir);
    REQUIRE(rest_res.is_ok());
    CHECK(std::filesystem::exists(file_to_delete));

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(scratch_root, ec);
}
