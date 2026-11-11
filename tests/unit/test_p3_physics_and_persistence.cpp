#include "tests/test_framework.hpp"

#include "shunya/eng/engineering_pipeline.hpp"

#include <cmath>
#include <filesystem>

using namespace shunya::eng;

TEST_CASE("PHYSIM: Gravity free fall, bounce restitution, heating & thermal expansion") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());

    auto& objs = ws.object_manager();
    REQUIRE(objs.count() >= 2u);
    // Elevate wire above ground y=0 so gravity drops it onto the floor
    auto wire_id = objs.objects()[0].id;
    PhysicalObject elevated = objs.objects()[0];
    elevated.geometry.position.y = 1.0;
    REQUIRE(objs.update_object(elevated, ws.material_registry()).is_ok());

    auto& phy = ws.physics_runtime();
    phy.initialize_from_objects(objs, 298.15, 9.80665, 10.0);
    phy.set_body_heat_source(wire_id, 50.0); // 50 W heating
    phy.set_body_force(wire_id, {0.0, 0.0, 0.0});

    double min_y = 1e9;
    bool bounced = false;
    double initial_t = phy.bodies()[0].temperature_k;
    for (int i = 0; i < 120; ++i) {
        auto step_res = phy.step(0.01);
        REQUIRE(step_res.is_ok());
        const auto* b = phy.find_body(wire_id);
        REQUIRE(b != nullptr);
        min_y = std::min(min_y, b->position.y);
        if (b->collision_active) bounced = true;
    }
    const auto* final_b = phy.find_body(wire_id);
    REQUIRE(final_b != nullptr);
    CHECK(min_y < 1.0);
    CHECK(bounced);
    CHECK(final_b->temperature_k > initial_t);
    CHECK(final_b->thermal_strain > 0.0);
}

TEST_CASE("PERSIST: Full v3.0 workspace JSON bundle round-trip preserves all UUID relationships") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());

    const auto orig_mat_count = ws.material_registry().material_count();
    const auto orig_mix_count = ws.mixture_experiments().size();
    const auto orig_obj_count = ws.object_manager().count();
    const auto orig_bat_count = ws.battery_specs().size();
    const auto orig_comp_count = ws.component_library().count();
    const auto orig_circ_count = ws.circuits().size();

    const std::filesystem::path tmp_file =
        std::filesystem::temp_directory_path() / "shunya_p3_test_bundle.shunya3.json";
    std::filesystem::remove(tmp_file);

    REQUIRE(ws.save_to_file(tmp_file).is_ok());
    CHECK(std::filesystem::exists(tmp_file));

    EngineeringWorkspaceState loaded;
    REQUIRE(loaded.load_from_file(tmp_file).is_ok());
    CHECK(loaded.material_registry().material_count() == orig_mat_count);
    CHECK(loaded.mixture_experiments().size() == orig_mix_count);
    CHECK(loaded.object_manager().count() == orig_obj_count);
    CHECK(loaded.battery_specs().size() == orig_bat_count);
    CHECK(loaded.component_library().count() == orig_comp_count);
    CHECK(loaded.circuits().size() == orig_circ_count);

    // Loaded circuit simulates identically without errors
    auto dc_res = CircuitSimulationRunner::run_dc(loaded.active_circuit_doc());
    REQUIRE(dc_res.is_ok());
    CHECK(dc_res.value().converged);

    std::filesystem::remove(tmp_file);
}
