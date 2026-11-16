#include "tests/test_framework.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "shunya/eng/engineering_pipeline.hpp"

using namespace shunya::eng;
using namespace shunya::core::base;
using namespace shunya::core::domain;

TEST_CASE("ARCH: EngineeringWorkspaceState reactive notification & material invalidation") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.project() != nullptr);
    CHECK(ws.validate_workspace().is_valid());

    int change_count = 0;
    DependencyChangeKind last_kind = DependencyChangeKind::ProjectReset;
    auto tok = ws.subscribe([&](const WorkspaceChangeEvent& ev) {
        ++change_count;
        last_kind = ev.kind;
    });

    Uuid invalidated_mat;
    ws.register_material_dependent_invalidator([&](const Uuid& mid) {
        invalidated_mat = mid;
    });

    Material mat(Uuid::generate_random(), "TestCopper", MaterialPhysicalState::Solid, 8920.0);
    const Uuid mat_id = mat.id();
    REQUIRE(ws.material_registry().register_material(mat).is_ok());

    ws.notify_material_modified(mat_id, "Updated Cu composition");
    CHECK(invalidated_mat == mat_id);
    CHECK(change_count == 1);
    CHECK(last_kind == DependencyChangeKind::MaterialUpdated);
    CHECK(ws.project()->is_dirty());

    AmbientEnvironmentConfig env;
    env.temperature_k = -10.0;
    ws.set_environment(env);
    CHECK_FALSE(ws.validate_workspace().is_valid());

    ws.unsubscribe(tok);
    ws.notify_change(DependencyChangeKind::CircuitUpdated);
    CHECK(change_count == 2); // set_environment fired 1, unsubscribed before CircuitUpdated
}

TEST_CASE("ARCH-PH4: Core header isolation asserts zero Qt/GUI dependencies in core domain") {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(SHUNYA_SOURCE_DIR);
    std::vector<fs::path> search_dirs = {
        root / "core" / "base" / "include",
        root / "core" / "domain" / "include"
    };

    int scanned_files = 0;
    bool forbidden_found = false;
    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && (entry.path().extension() == ".hpp" || entry.path().extension() == ".h")) {
                ++scanned_files;
                std::ifstream infile(entry.path());
                std::string line;
                while (std::getline(infile, line)) {
                    if (line.find("#include <Q") != std::string::npos && line.find("<queue>") == std::string::npos) {
                        forbidden_found = true;
                        std::cerr << "Forbidden Qt include found: " << entry.path().string() << ": " << line << "\n";
                    }
                    if (line.find("#include <Qt") != std::string::npos) {
                        forbidden_found = true;
                        std::cerr << "Forbidden Qt include found: " << entry.path().string() << ": " << line << "\n";
                    }
                }
            }
        }
    }
    CHECK_FALSE(forbidden_found);
    CHECK(scanned_files > 30);
}



TEST_CASE("PH4-01.03.001: Versioned Dirty Ticks & Reactive Hub Architecture") {
    EngineeringWorkspaceState ws;

    const uint64_t initial_global = ws.global_tick();
    const uint64_t initial_mat_tick = ws.tick_for(DependencyChangeKind::MaterialUpdated);
    const uint64_t initial_circ_tick = ws.tick_for(DependencyChangeKind::CircuitUpdated);
    const uint64_t initial_sim_tick = ws.tick_for(DependencyChangeKind::SimulationStepped);

    CHECK_FALSE(ws.is_any_dirty_since(initial_global));
    CHECK_FALSE(ws.is_dirty_since(DependencyChangeKind::MaterialUpdated, initial_mat_tick));
    CHECK_FALSE(ws.is_dirty_since(DependencyChangeKind::CircuitUpdated, initial_circ_tick));

    // 1. Trigger Material change
    Uuid test_mat_id = Uuid::generate_random();
    ws.notify_change(DependencyChangeKind::MaterialUpdated, test_mat_id, "Alloy synthesis");

    CHECK(ws.is_any_dirty_since(initial_global));
    CHECK(ws.is_dirty_since(DependencyChangeKind::MaterialUpdated, initial_mat_tick));
    CHECK_FALSE(ws.is_dirty_since(DependencyChangeKind::CircuitUpdated, initial_circ_tick));
    CHECK_FALSE(ws.is_dirty_since(DependencyChangeKind::SimulationStepped, initial_sim_tick));

    const uint64_t after_mat_tick = ws.tick_for(DependencyChangeKind::MaterialUpdated);
    CHECK(after_mat_tick > initial_mat_tick);

    // 2. Trigger Simulation Step
    ws.touch(DependencyChangeKind::SimulationStepped, {}, "Euler step dt=1ms");
    CHECK(ws.is_dirty_since(DependencyChangeKind::SimulationStepped, initial_sim_tick));
    CHECK_FALSE(ws.is_dirty_since(DependencyChangeKind::MaterialUpdated, after_mat_tick));

    // 3. Inspect Mutation History Log
    auto history = ws.recent_mutations(10);
    REQUIRE(history.size() >= 2);
    const auto& last_entry = history.back();
    CHECK(last_entry.kind == DependencyChangeKind::SimulationStepped);
    CHECK(last_entry.summary == "Euler step dt=1ms");

    const auto& prev_entry = history[history.size() - 2];
    CHECK(prev_entry.kind == DependencyChangeKind::MaterialUpdated);
    CHECK(prev_entry.target_id == test_mat_id);
    CHECK(prev_entry.summary == "Alloy synthesis");
}

TEST_CASE("PH4-01.03.002: Transactional Snapshot Checkpoints & Unlimited Undo/Redo") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());

    // 1. Unlimited command stack configuration
    CHECK(ws.can_undo() == false);
    CHECK(ws.can_redo() == false);

    // 2. Create initial baseline checkpoint
    const uint64_t cp_base_id = ws.create_checkpoint("Lithium_Baseline");
    CHECK(cp_base_id > 0);
    auto all_cps = ws.checkpoints();
    CHECK(all_cps.size() == 1);
    CHECK(all_cps[0].name == "Lithium_Baseline");

    const size_t orig_obj_count = ws.object_manager().objects().size();
    const size_t orig_mat_count = ws.material_registry().material_count();
    REQUIRE(orig_obj_count > 0);
    REQUIRE(orig_mat_count > 0);

    // 3. Execute successful multi-domain transaction
    auto tx_res = ws.execute_transaction("Add Test Material & Object", [&]() -> shunya::core::base::Result<void> {
        Material mat(Uuid::generate_random(), "TransactionTestAlloy", MaterialPhysicalState::Solid, 8920.0);
        mat.electrical().electrical_conductivity = 1.5e7;
        const Uuid mat_id = mat.id();
        auto add_mat = ws.material_registry().register_material(mat);
        if (add_mat.is_err()) return add_mat.error();

        PhysicalObject obj;
        obj.name = "TransactionTestCube";
        obj.geometry.kind = ObjectShapeKind::Box;
        obj.geometry.dim_x_m = 0.1;
        obj.geometry.dim_y_m = 0.1;
        obj.geometry.dim_z_m = 0.1;
        obj.regions.push_back(ObjectRegion{"whole", "Core", mat_id, 1.0, 0.0, true});
        auto add_obj = ws.object_manager().add_object(std::move(obj), ws.material_registry());
        if (add_obj.is_err()) return add_obj.error();

        return shunya::core::base::Result<void>::ok();
    });
    REQUIRE(tx_res.is_ok());
    CHECK(ws.object_manager().objects().size() == orig_obj_count + 1);
    CHECK(ws.material_registry().material_count() == orig_mat_count + 1);
    CHECK(ws.can_undo() == true);
    CHECK(ws.can_redo() == false);

    // 4. Test Undo multi-domain transaction
    auto undo_res = ws.undo();
    REQUIRE(undo_res.is_ok());
    CHECK(ws.object_manager().objects().size() == orig_obj_count);
    CHECK(ws.material_registry().material_count() == orig_mat_count);
    CHECK(ws.can_undo() == false);
    CHECK(ws.can_redo() == true);

    // 5. Test Redo multi-domain transaction
    auto redo_res = ws.redo();
    REQUIRE(redo_res.is_ok());
    CHECK(ws.object_manager().objects().size() == orig_obj_count + 1);
    CHECK(ws.material_registry().material_count() == orig_mat_count + 1);
    CHECK(ws.can_undo() == true);
    CHECK(ws.can_redo() == false);

    // 6. Test Failed Transaction (Atomic rollback)
    auto fail_tx = ws.execute_transaction("FailingTx", [&]() -> shunya::core::base::Result<void> {
        Material mat(Uuid::generate_random(), "ShouldBeRolledBack", MaterialPhysicalState::Solid, 1000.0);
        (void)ws.material_registry().register_material(mat);
        return shunya::core::base::Result<void>::err(shunya::core::base::Error{
            .code = 9999,
            .domain = shunya::core::base::ErrorDomain::Validation,
            .severity = shunya::core::base::Severity::Error,
            .message = "Simulated failure in transaction",
            .details = ""
        });
    });
    CHECK(fail_tx.is_err());
    // Material should have been rolled back
    CHECK(ws.material_registry().has_material("ShouldBeRolledBack") == false);

    // 7. Checkpoint Restore
    const uint64_t cp_mod_id = ws.create_checkpoint("Post_Transaction_State");
    CHECK(ws.checkpoints().size() == 2);

    auto rest_base = ws.restore_checkpoint(cp_base_id);
    REQUIRE(rest_base.is_ok());
    CHECK(ws.object_manager().objects().size() == orig_obj_count);
    CHECK(ws.material_registry().material_count() == orig_mat_count);

    auto rest_mod = ws.restore_checkpoint(cp_mod_id);
    REQUIRE(rest_mod.is_ok());
    CHECK(ws.object_manager().objects().size() == orig_obj_count + 1);
    CHECK(ws.material_registry().material_count() == orig_mat_count + 1);

    // 8. Delete Checkpoint
    CHECK(ws.delete_checkpoint(cp_base_id) == true);
    CHECK(ws.checkpoints().size() == 1);
    CHECK(ws.find_checkpoint(cp_base_id).has_value() == false);
}
