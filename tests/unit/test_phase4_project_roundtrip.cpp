#include "tests/test_framework.hpp"

#include "shunya/eng/engineering_pipeline.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;
using namespace shunya::eng;

// =============================================================================
// PH4-13.02.002: Project Roundtrip (.shunya4 JSON & .shunya4bin Binary) Verification
// =============================================================================

TEST_CASE("PH4-13.02.002: Full Headless Project Serialization Roundtrip & Numerical Equality") {
    EngineeringWorkspaceState original_ws;
    REQUIRE(original_ws.seed_lithium_battery_workflow().is_ok());

    // Verify baseline entity counts in source project
    const size_t orig_materials = original_ws.material_registry().material_count();
    const size_t orig_objects = original_ws.object_manager().objects().size();
    const size_t orig_circuits = original_ws.circuits().size();
    const size_t orig_components = original_ws.component_library().count();

    REQUIRE(orig_materials >= 5);
    REQUIRE(orig_objects >= 2);
    REQUIRE(orig_circuits >= 1);
    REQUIRE(orig_components >= 1);

    const auto temp_dir = std::filesystem::temp_directory_path() / "shunya_ph4_roundtrip_test";
    std::filesystem::create_directories(temp_dir);

    const auto json_path = temp_dir / "roundtrip_test.shunya4";
    const auto bin_path = temp_dir / "roundtrip_test.shunya4bin";

    // -------------------------------------------------------------------------
    // 1. JSON Roundtrip (.shunya4)
    // -------------------------------------------------------------------------
    auto save_json_res = original_ws.save_to_file(json_path);
    REQUIRE(save_json_res.is_ok());
    CHECK(std::filesystem::exists(json_path));

    EngineeringWorkspaceState loaded_json_ws;
    auto load_json_res = loaded_json_ws.load_from_file(json_path);
    REQUIRE(load_json_res.is_ok());

    // Entity count assertions
    CHECK(loaded_json_ws.material_registry().material_count() == orig_materials);
    CHECK(loaded_json_ws.object_manager().objects().size() == orig_objects);
    CHECK(loaded_json_ws.circuits().size() == orig_circuits);
    CHECK(loaded_json_ws.component_library().count() == orig_components);

    // Deep numerical comparison on physical objects
    for (const auto& orig_obj : original_ws.object_manager().objects()) {
        const auto* loaded_obj = loaded_json_ws.object_manager().find_by_name(orig_obj.name);
        REQUIRE(loaded_obj != nullptr);

        CHECK(std::abs(loaded_obj->geometry.dim_x_m - orig_obj.geometry.dim_x_m) < 1e-9);
        CHECK(std::abs(loaded_obj->geometry.compute_volume_m3() - orig_obj.geometry.compute_volume_m3()) < 1e-9);
        CHECK(loaded_obj->name == orig_obj.name);
    }

    // -------------------------------------------------------------------------
    // 2. Binary Roundtrip (.shunya4bin) with CRC32 Verification
    // -------------------------------------------------------------------------
    Shunya4SaveOptions bin_opts;
    bin_opts.binary_format = true;
    auto save_bin_res = original_ws.save_to_file(bin_path, bin_opts);
    REQUIRE(save_bin_res.is_ok());
    CHECK(std::filesystem::exists(bin_path));

    EngineeringWorkspaceState loaded_bin_ws;
    auto load_bin_res = loaded_bin_ws.load_from_file(bin_path);
    REQUIRE(load_bin_res.is_ok());

    CHECK(loaded_bin_ws.material_registry().material_count() == orig_materials);
    CHECK(loaded_bin_ws.object_manager().objects().size() == orig_objects);
    CHECK(loaded_bin_ws.circuits().size() == orig_circuits);
    CHECK(loaded_bin_ws.component_library().count() == orig_components);

    // Cleanup
    std::filesystem::remove(json_path);
    std::filesystem::remove(bin_path);
}
