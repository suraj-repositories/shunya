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

TEST_CASE("PH4-01.03.003: Atomic .shunya4 Project Serialization & Differential Saves") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());

    const auto temp_dir = std::filesystem::temp_directory_path() / "shunya_ph4_test_save";
    std::filesystem::create_directories(temp_dir);

    const auto json_path = temp_dir / "project_save.shunya4";
    const auto bin_path = temp_dir / "project_save.shunya4bin";
    const auto diff_path = temp_dir / "project_diff.shunya4";
    const auto corrupt_path = temp_dir / "project_corrupt.shunya4bin";

    // 1. Full JSON save (.shunya4)
    auto save_json_res = ws.save_to_file(json_path);
    REQUIRE(save_json_res.is_ok());
    CHECK(std::filesystem::exists(json_path));

    EngineeringWorkspaceState ws_json_load;
    auto load_json_res = ws_json_load.load_from_file(json_path);
    REQUIRE(load_json_res.is_ok());
    CHECK(ws_json_load.material_registry().material_count() == ws.material_registry().material_count());
    CHECK(ws_json_load.object_manager().objects().size() == ws.object_manager().objects().size());
    CHECK(ws_json_load.circuits().size() == ws.circuits().size());

    // 2. Binary Container Save (.shunya4bin) with CRC32
    Shunya4SaveOptions bin_opts;
    bin_opts.binary_format = true;
    auto save_bin_res = ws.save_to_file(bin_path, bin_opts);
    REQUIRE(save_bin_res.is_ok());
    CHECK(std::filesystem::exists(bin_path));

    EngineeringWorkspaceState ws_bin_load;
    auto load_bin_res = ws_bin_load.load_from_file(bin_path);
    REQUIRE(load_bin_res.is_ok());
    CHECK(ws_bin_load.material_registry().material_count() == ws.material_registry().material_count());
    CHECK(ws_bin_load.object_manager().objects().size() == ws.object_manager().objects().size());

    // 3. Corrupt Binary Rejection
    std::vector<uint8_t> corrupt_bytes;
    {
        std::ifstream is(bin_path, std::ios::binary);
        corrupt_bytes.assign((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    }
    REQUIRE(corrupt_bytes.size() > sizeof(Shunya4BinaryHeader) + 10);
    corrupt_bytes.back() ^= 0xFF; // Corrupt payload byte
    {
        std::ofstream os(corrupt_path, std::ios::binary | std::ios::trunc);
        os.write(reinterpret_cast<const char*>(corrupt_bytes.data()), static_cast<std::streamsize>(corrupt_bytes.size()));
    }

    EngineeringWorkspaceState ws_corrupt;
    auto corrupt_res = ws_corrupt.load_from_file(corrupt_path);
    CHECK(corrupt_res.is_err()); // Must fail due to CRC32 mismatch!

    // 4. Incremental / Differential Save
    const uint64_t base_tick = ws.global_tick();

    // Mutate materials only
    Material diff_mat(Uuid::generate_random(), "DiffSilicon", MaterialPhysicalState::Solid, 2330.0);
    REQUIRE(ws.material_registry().register_material(diff_mat).is_ok());
    ws.touch(DependencyChangeKind::MaterialUpdated, diff_mat.id(), "Added DiffSilicon");
    CHECK(ws.global_tick() > base_tick);

    Shunya4SaveOptions diff_opts;
    diff_opts.incremental = true;
    diff_opts.base_tick = base_tick;
    auto diff_save_res = ws.save_to_file(diff_path, diff_opts);
    REQUIRE(diff_save_res.is_ok());

    // Apply differential update onto ws_bin_load (which has original state without DiffSilicon)
    CHECK_FALSE(ws_bin_load.material_registry().has_material("DiffSilicon"));
    auto apply_diff_res = ws_bin_load.load_from_file(diff_path);
    REQUIRE(apply_diff_res.is_ok());
    CHECK(ws_bin_load.material_registry().has_material("DiffSilicon"));
    // Other objects should remain intact
    CHECK(ws_bin_load.object_manager().objects().size() == ws.object_manager().objects().size());

    // Clean up temporary files
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("PH4-01.03.004: Asynchronous Project Loading Pipeline & Progress Reporting") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());

    const auto temp_dir = std::filesystem::temp_directory_path() / "shunya_ph4_async_load_test";
    std::filesystem::create_directories(temp_dir);
    const auto project_file = temp_dir / "async_test.shunya4";
    REQUIRE(ws.save_to_file(project_file).is_ok());

    // 1. Synchronous load with progress callbacks
    std::vector<ProjectLoadProgress> reports;
    EngineeringWorkspaceState ws_sync;
    auto sync_res = ws_sync.load_from_file_with_progress(project_file, [&](const ProjectLoadProgress& p) {
        reports.push_back(p);
    });
    REQUIRE(sync_res.is_ok());
    REQUIRE(reports.size() >= 4);
    CHECK(reports.front().fraction <= 0.15);
    CHECK(reports.back().fraction == 1.0);
    // Assert monotonic progress
    for (size_t i = 1; i < reports.size(); ++i) {
        CHECK(reports[i].fraction >= reports[i - 1].fraction);
    }

    // 2. Asynchronous background load via future
    EngineeringWorkspaceState ws_async;
    std::vector<ProjectLoadProgress> async_reports;
    auto future_res = ws_async.load_from_file_async(project_file, [&](const ProjectLoadProgress& p) {
        async_reports.push_back(p);
    });
    REQUIRE(future_res.valid());
    auto async_res = future_res.get();
    REQUIRE(async_res.is_ok());
    CHECK(ws_async.material_registry().material_count() == ws.material_registry().material_count());
    CHECK(ws_async.object_manager().objects().size() == ws.object_manager().objects().size());
    CHECK(async_reports.size() >= 4);

    // 3. Cancellation support with CancellationToken
    EngineeringWorkspaceState ws_cancel;
    CancellationToken cancel_tok;
    cancel_tok.cancel(); // Pre-cancelled
    auto cancel_res = ws_cancel.load_from_file_with_progress(project_file, nullptr, cancel_tok);
    CHECK(cancel_res.is_err());
    CHECK(cancel_res.error().code == 4920);

    // 4. Schema Migration during loading
    const std::string legacy_v001 = R"({
        "schema_version": "0.0.1",
        "id": "11111111-2222-3333-4444-555555555555",
        "name": "LegacyProject",
        "author": "OldUser",
        "created_at": "2026-01-01T00:00:00Z",
        "modified_at": "2026-01-01T00:00:00Z"
    })";
    const auto legacy_file = temp_dir / "legacy_v001.json";
    {
        std::ofstream os(legacy_file);
        os << legacy_v001;
    }
    EngineeringWorkspaceState ws_legacy;
    auto leg_future = ws_legacy.load_from_file_async(legacy_file);
    auto leg_res = leg_future.get();
    REQUIRE(leg_res.is_ok());
    REQUIRE(ws_legacy.project() != nullptr);
    CHECK(ws_legacy.project()->manifest().name == "LegacyProject");

    // Clean up temporary files
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("PH4-01.03.005: Multi-Domain Entity-Relationship Graph & Dependency Tracking") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());

    EntityRelationshipGraph graph = ws.build_entity_graph();

    // 1. Assert nodes and edges populated across all domains
    CHECK(graph.node_count() > 0);
    CHECK(graph.edge_count() > 0);

    size_t mat_count = 0, obj_count = 0, comp_count = 0, circ_count = 0;
    for (const auto& [id, node] : graph.nodes()) {
        if (node.kind == EntityKind::Material) ++mat_count;
        if (node.kind == EntityKind::PhysicalObject) ++obj_count;
        if (node.kind == EntityKind::Component) ++comp_count;
        if (node.kind == EntityKind::Circuit) ++circ_count;
    }
    CHECK(mat_count > 0);
    CHECK(obj_count > 0);
    CHECK(comp_count > 0);
    CHECK(circ_count > 0);

    // 2. Validate acyclic nature of seeded engineering graph
    CHECK_FALSE(graph.has_cycles());
    auto topo_res = graph.topological_sort();
    REQUIRE(topo_res.is_ok());
    CHECK(topo_res.value().size() == graph.node_count());

    // 3. Transitive dependency tracking
    // Pick an object that has assigned materials
    const auto& objs = ws.object_manager().objects();
    REQUIRE(!objs.empty());
    const auto& test_obj = objs.front();
    const auto obj_deps = graph.get_transitive_dependencies(test_obj.id);
    CHECK_FALSE(obj_deps.empty());

    // Transitive dependents of a material
    REQUIRE(!test_obj.regions.empty());
    const auto test_mat_id = test_obj.regions.front().material_id;
    const auto mat_dependents = graph.get_transitive_dependents(test_mat_id);
    CHECK_FALSE(mat_dependents.empty());
    // The test_obj must be in the dependents of test_mat_id
    CHECK(std::find(mat_dependents.begin(), mat_dependents.end(), test_obj.id) != mat_dependents.end());

    // 4. Cycle detection
    EntityRelationshipGraph cyclic_graph;
    Uuid id_a = Uuid::generate_random();
    Uuid id_b = Uuid::generate_random();
    Uuid id_c = Uuid::generate_random();
    cyclic_graph.add_node({id_a, EntityKind::Material, "NodeA", ""});
    cyclic_graph.add_node({id_b, EntityKind::PhysicalObject, "NodeB", ""});
    cyclic_graph.add_node({id_c, EntityKind::Component, "NodeC", ""});
    cyclic_graph.add_edge(id_a, id_b, RelationKind::AssignedTo);
    cyclic_graph.add_edge(id_b, id_c, RelationKind::GeneratedFrom);
    cyclic_graph.add_edge(id_c, id_a, RelationKind::Contains); // Cycle!
    CHECK(cyclic_graph.has_cycles() == true);
    CHECK(cyclic_graph.topological_sort().is_err());

    // 5. Orphaned edge detection
    EntityRelationshipGraph orphan_graph;
    orphan_graph.add_node({id_a, EntityKind::Material, "NodeA", ""});
    orphan_graph.add_edge(id_a, Uuid::generate_random(), RelationKind::AssignedTo); // Missing target!
    auto orphans = orphan_graph.find_orphaned_edges();
    CHECK(orphans.size() == 1);

    // 6. Visualization formats (DOT and JSON)
    const std::string dot = graph.to_dot();
    CHECK(dot.find("digraph ShunyaEntityGraph") != std::string::npos);
    CHECK(dot.find("Material") != std::string::npos);

    const std::string json = graph.to_json();
    CHECK(json.find("\"nodes\"") != std::string::npos);
    CHECK(json.find("\"edges\"") != std::string::npos);
}
