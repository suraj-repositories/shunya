#include "tests/test_framework.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "shunya/eng/engineering_pipeline.hpp"
#include "shunya/core/domain/mock_hardware_rasterizer.hpp"
#include "shunya/perf/simulation_step_arena.hpp"
#include "shunya/cad3d/half_edge_pool.hpp"
#include "shunya/core/domain/smart_asset_handle.hpp"
#include "shunya/core/base/zero_copy_views.hpp"
#include "shunya/perf/lru_asset_cache.hpp"

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

TEST_CASE("PH4-01.03.006: Data Corruption Recovery & Orphaned Asset Garbage Collection") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());

    // 1. Orphaned Asset Garbage Collection
    // Inject orphaned material
    Material orphan_mat(Uuid::generate_random(), "OrphanMat", MaterialPhysicalState::Solid, 1200.0);
    REQUIRE(ws.material_registry().register_material(orphan_mat).is_ok());
    CHECK(ws.material_registry().has_material(orphan_mat.id()));

    // Inject corrupted region pointing to nonexistent material
    PhysicalObject bad_obj;
    bad_obj.name = "CorruptedRegionObject";
    bad_obj.regions.push_back(ObjectRegion{"corrupt_reg", "BadRegion", Uuid::generate_random(), 1.0, 0.0, true});
    ws.object_manager().objects_mut().push_back(std::move(bad_obj));

    // Inject orphaned component pointing to nonexistent source object
    GeneratedComponentDefinition orphan_comp;
    orphan_comp.name = "OrphanComponent";
    orphan_comp.source_object_id = Uuid::generate_random(); // Nonexistent!
    orphan_comp.derived.is_qualified = true;
    REQUIRE(ws.component_library().register_component(std::move(orphan_comp)).is_ok());

    // Inject orphaned circuit instance pointing to nonexistent generated component
    REQUIRE(!ws.circuits().empty());
    PlacedCircuitComponent bad_inst;
    bad_inst.id = "BAD_R1";
    bad_inst.kind = CircuitSymbolKind::Resistor;
    bad_inst.generated_component_id = Uuid::generate_random(); // Nonexistent!
    ws.circuits().front().components_mut().push_back(std::move(bad_inst));

    // Inject dangling wire with only 1 point
    CircuitWire bad_wire;
    bad_wire.id = "DanglingWire";
    bad_wire.points_mm.push_back({0.0, 0.0}); // Only 1 point!
    ws.circuits().front().wires_mut().push_back(std::move(bad_wire));

    // Execute Garbage Collection with pruning
    auto gc_rep = ws.collect_garbage_and_prune_orphans(true);
    CHECK(gc_rep.repaired_regions >= 1);
    CHECK(gc_rep.pruned_materials >= 1);
    CHECK(gc_rep.pruned_components >= 1);
    CHECK(gc_rep.pruned_circuit_instances >= 1);
    CHECK(gc_rep.pruned_dangling_wires >= 1);
    CHECK_FALSE(ws.material_registry().has_material("OrphanMat"));

    // 2. Automated Corruption Recovery from Truncated / Damaged JSON
    const std::string full_json = ws.to_bundle_json();
    REQUIRE(full_json.size() > 500);

    // Severely truncate the JSON bundle midway through materials/objects
    std::string truncated_json = full_json.substr(0, full_json.size() / 2);

    EngineeringWorkspaceState ws_recovered;
    auto recover_res = ws_recovered.recover_from_corrupted_bundle(truncated_json);
    REQUIRE(recover_res.is_ok());
    auto rec_rep = recover_res.value();
    CHECK(rec_rep.recovered == true);
    CHECK(rec_rep.salvaged_materials > 0);
    CHECK_FALSE(rec_rep.repairs_applied.empty());

    // 3. Recovery from Damaged Binary File (Tampered Header)
    const auto temp_dir = std::filesystem::temp_directory_path() / "shunya_ph4_corrupt_test";
    std::filesystem::create_directories(temp_dir);
    const auto bad_bin_file = temp_dir / "damaged.shunya4bin";

    Shunya4SaveOptions bin_opts;
    bin_opts.binary_format = true;
    REQUIRE(ws.save_to_file(bad_bin_file, bin_opts).is_ok());

    // Corrupt binary CRC checksum field in container header (offset 32)
    {
        std::fstream fs(bad_bin_file, std::ios::in | std::ios::out | std::ios::binary);
        fs.seekp(32);
        char junk = 0x5A;
        fs.write(&junk, 1);
    }

    // Normal load must fail with CRC error
    EngineeringWorkspaceState ws_normal;
    CHECK(ws_normal.load_from_file(bad_bin_file).is_err());

    // Recovery must salvage valid entities
    EngineeringWorkspaceState ws_file_recovered;
    auto file_rec_res = ws_file_recovered.recover_from_corrupted_file(bad_bin_file);
    REQUIRE(file_rec_res.is_ok());
    CHECK(file_rec_res.value().salvaged_materials > 0);

    // Clean up
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("PH4-01.04.002: Test Framework Benchmark Timers & Memory Leak Detection Fixtures") {
    // 1. Benchmark Timer Verification
    size_t counter = 0;
    auto bench_res = BENCHMARK("IncrementBenchmark", 100, {
        for (int i = 0; i < 1000; ++i) {
            counter += static_cast<size_t>(i);
        }
    });

    CHECK(bench_res.iterations == 100);
    CHECK(bench_res.total_ms >= 0.0);
    CHECK(bench_res.avg_us >= 0.0);
    CHECK(bench_res.ops_per_sec > 0.0);
    CHECK(counter > 0);

    // 2. Scoped Timer
    {
        TIMED_SECTION("ScopedSleepTimer");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // Verified via stdout output and no exception

    // 3. Process Memory Stats
    auto mem_stats = ::shunya::test::get_process_memory_stats();
    CHECK(mem_stats.working_set_bytes > 0);

    // 4. Scoped Memory Leak Detection Fixture
    CHECK_NO_MEMORY_GROWTH(20 * 1024 * 1024, {
        std::vector<int> temp_data(10000, 42);
        CHECK(temp_data.size() == 10000);
    });

    shunya::test::ScopedMemoryLeakDetector detector("ExplicitScope", 30 * 1024 * 1024);
    CHECK_FALSE(detector.has_leak());
}

TEST_CASE("PH4-01.04.003: Headless CLI & Legacy .shunya3 Project Compatibility") {
    // 1. Load legacy .shunya3.json project
    const auto legacy_path = std::filesystem::path(SHUNYA_SOURCE_DIR) / "tests" / "data" / "legacy_v3_demo.shunya3.json";
    REQUIRE(std::filesystem::exists(legacy_path));

    EngineeringWorkspaceState ws;
    REQUIRE(ws.load_from_file(legacy_path).is_ok());

    // Verify all entity collections loaded from Phase 3 legacy format
    CHECK(ws.material_registry().material_count() >= 5);
    CHECK(ws.object_manager().count() >= 2);
    CHECK(ws.component_library().count() >= 1);
    CHECK(!ws.circuits().empty());

    // 2. Execute DC simulation on legacy circuit
    auto dc_res = CircuitSimulationRunner::run_dc(ws.active_circuit_doc());
    REQUIRE(dc_res.is_ok());
    CHECK(dc_res.value().converged);

    // 3. Execute transient simulation
    auto tr_res = CircuitSimulationRunner::run_transient(
        ws.active_circuit_doc(), 0.1, 0.001,
        &ws.component_library(), ws.environment().temperature_k);
    REQUIRE(tr_res.is_ok());
    CHECK(tr_res.value().success);
    CHECK(!tr_res.value().time_s.empty());
}

TEST_CASE("PH4-01.04.004: Mock Hardware Rasterizer Harness & Offscreen Viewport Testing") {
    // 1. Offscreen Mock Hardware Rasterizer Basic Operations
    RasterizerConfig cfg;
    cfg.width = 160;
    cfg.height = 120;
    cfg.clear_color = {0.1f, 0.15f, 0.2f, 1.0f};
    MockHardwareRasterizer rast(cfg);

    CHECK(rast.width() == 160);
    CHECK(rast.height() == 120);

    // Initial clear checksum
    uint64_t bg_checksum = rast.compute_checksum();
    CHECK(bg_checksum != 0);

    // 2. Offscreen Viewport Harness Navigation & Rendering
    OffscreenViewportTestHarness harness(160, 120);
    harness.look_at({0.0, 0.0, 5.0}, {0.0, 0.0, 0.0});

    // Render 3D cube at origin with ID 42
    harness.render_cube({0.0, 0.0, 0.0}, 2.0, {0.9f, 0.2f, 0.2f, 1.0f}, 42);

    auto stats = harness.rasterizer().stats();
    CHECK(stats.triangles_input == 12);
    CHECK(stats.fragments_shaded > 100);

    // Center pixel should hit the cube (object id 42)
    uint32_t center_id = harness.pick_object_at(80.0, 60.0);
    CHECK(center_id == 42);

    // Corner pixel should be background (object id 0)
    uint32_t corner_id = harness.pick_object_at(5.0, 5.0);
    CHECK(corner_id == 0);

    // Center depth should be in front of far-plane and behind near-plane
    float center_depth = harness.sample_depth(80, 60);
    CHECK(center_depth < 1.0f);
    CHECK(center_depth > 0.0f);

    // 3. Camera Manipulation (Orbit, Pan, Dolly)
    harness.orbit(45.0, 15.0);
    harness.dolly(2.0);
    harness.pan(0.5, 0.5);

    // Ray picking from screen
    Ray ray = harness.ray_from_screen(80.0, 60.0);
    CHECK(ray.direction.length() > 0.99);

    // Checksum determinism check
    uint64_t frame_hash1 = harness.rasterizer().compute_checksum();
    CHECK(frame_hash1 != bg_checksum);
}

TEST_CASE("PH4-01.04.005: 100,000 Project Creation & Destruction Memory Stability Stress Test") {
    constexpr size_t TOTAL_CYCLES = 100000;

    auto mem_before = shunya::test::get_process_memory_stats();

    BENCHMARK("100k_Project_Creation_Destruction", 1, {
        for (size_t i = 0; i < TOTAL_CYCLES; ++i) {
            auto proj_res = Project::create_new("StressProject", "StressTester");
            auto proj = proj_res.value();
            proj->manifest().description = "Memory churn testing";
            proj->manifest().metadata["cycle"] = std::to_string(i & 0xFF);
            // Destructor cleans up immediately at end of iteration scope
        }
    });

    auto mem_after = shunya::test::get_process_memory_stats();
    int64_t growth_bytes = static_cast<int64_t>(mem_after.private_bytes) - static_cast<int64_t>(mem_before.private_bytes);

    // Memory growth must remain bounded below 15 MB across 100,000 creation/destruction cycles
    CHECK(growth_bytes < 15 * 1024 * 1024);
}

TEST_CASE("PH4-02.01.001: PMR Linear Arena for High-Frequency Transient Step Allocations") {
    using namespace shunya::perf;

    constexpr size_t ARENA_SIZE = 4 * 1024 * 1024; // 4 MB
    constexpr size_t SIMULATION_STEPS = 10000;

    SimulationStepArena step_arena(ARENA_SIZE);
    CHECK(step_arena.capacity_bytes() == ARENA_SIZE);
    CHECK(step_arena.current_step_bytes() == 0);

    double total_sim_energy = 0.0;

    BENCHMARK("10k_Transient_Step_PMR_Allocations", 1, {
        for (size_t step = 0; step < SIMULATION_STEPS; ++step) {
            auto step_scope = step_arena.scoped_step();

            // Allocate step transient scratch vectors via PMR resource
            std::pmr::vector<double> node_voltages(step_scope.resource());
            std::pmr::vector<double> branch_currents(step_scope.resource());
            std::pmr::vector<double> state_derivatives(step_scope.resource());

            node_voltages.resize(128, 3.3);
            branch_currents.resize(64, 0.05);
            state_derivatives.resize(128, 0.0);

            // Compute mock transient numerical integration
            for (size_t i = 0; i < 64; ++i) {
                total_sim_energy += node_voltages[i] * branch_currents[i] * 0.001;
            }

            CHECK(step_arena.current_step_bytes() > 0);
            // Arena resets automatically upon step_scope destruction
        }
    });

    CHECK(step_arena.total_step_resets() >= SIMULATION_STEPS);
    CHECK(step_arena.current_step_bytes() == 0);
    // Peak memory stayed well within 32 KB across all 10,000 steps without fragmentation
    CHECK(step_arena.peak_step_bytes() < 32 * 1024);
    CHECK(total_sim_energy > 0.0);
}

TEST_CASE("PH4-02.01.002: Fixed-Size Block Memory Pools for HalfEdgeMesh") {
    using namespace shunya::cad3d;

    PooledHalfEdgeMesh pool_mesh(1024);
    CHECK(pool_mesh.active_vertex_count() == 0);
    CHECK(pool_mesh.active_half_edge_count() == 0);
    CHECK(pool_mesh.active_face_count() == 0);

    // 1. Bulk Allocation
    std::vector<uint32_t> v_ids;
    v_ids.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        v_ids.push_back(pool_mesh.allocate_vertex({static_cast<double>(i), 0.0, 0.0}));
    }
    CHECK(pool_mesh.active_vertex_count() == 1000);

    std::vector<uint32_t> he_ids;
    he_ids.reserve(3000);
    for (int i = 0; i < 3000; ++i) {
        he_ids.push_back(pool_mesh.allocate_half_edge(v_ids[static_cast<size_t>(i % 1000)]));
    }
    CHECK(pool_mesh.active_half_edge_count() == 3000);

    std::vector<uint32_t> f_ids;
    f_ids.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        f_ids.push_back(pool_mesh.allocate_face(he_ids[static_cast<size_t>(i * 3)], 3));
    }
    CHECK(pool_mesh.active_face_count() == 1000);

    // 2. Free 500 items and verify free list recycling
    for (size_t i = 0; i < 500; ++i) {
        pool_mesh.free_vertex(v_ids[i]);
        pool_mesh.free_half_edge(he_ids[i]);
        pool_mesh.free_face(f_ids[i]);
    }
    CHECK(pool_mesh.active_vertex_count() == 500);
    CHECK(pool_mesh.active_half_edge_count() == 2500);
    CHECK(pool_mesh.active_face_count() == 500);

    const size_t v_cap_before = pool_mesh.vertex_capacity();

    // Re-allocate 500 vertices: must reuse recycled slots with 0 capacity growth
    for (size_t i = 0; i < 500; ++i) {
        (void)pool_mesh.allocate_vertex({0.0, 1.0, 2.0});
    }
    CHECK(pool_mesh.active_vertex_count() == 1000);
    CHECK(pool_mesh.vertex_capacity() == v_cap_before);

    // 3. Compact Mesh Migration
    HalfEdgeMesh compact = pool_mesh.to_compact_mesh();
    CHECK(compact.vertices.size() == 1000);
    CHECK(compact.half_edges.size() == 2500);
    CHECK(compact.faces.size() == 500);

    // 4. Churn Benchmark: 50,000 allocate/free cycles
    BENCHMARK("50k_HalfEdge_Pooled_Churn", 1, {
        for (int cycle = 0; cycle < 50000; ++cycle) {
            uint32_t v = pool_mesh.allocate_vertex({1.0, 2.0, 3.0});
            uint32_t he = pool_mesh.allocate_half_edge(v);
            uint32_t f = pool_mesh.allocate_face(he, 3);

            pool_mesh.free_face(f);
            pool_mesh.free_half_edge(he);
            pool_mesh.free_vertex(v);
        }
    });

    CHECK(pool_mesh.active_vertex_count() == 1000);
    CHECK(pool_mesh.active_half_edge_count() == 2500);
    CHECK(pool_mesh.active_face_count() == 500);
}

TEST_CASE("PH4-02.01.003: RAII Smart Pointer Ownership & Lifetime Enforcement") {
    int alive_instances = 0;
    struct TestResource {
        int* counter{nullptr};
        int value{0};

        explicit TestResource(int v, int* cnt = nullptr) : counter(cnt), value(v) {
            if (counter) ++(*counter);
        }
        ~TestResource() {
            if (counter) --(*counter);
        }
    };

    // 1. Exclusive Ownership (std::unique_ptr)
    {
        ExclusiveAsset<TestResource> exclusive = make_exclusive<TestResource>(42, &alive_instances);
        CHECK(alive_instances == 1);
        CHECK(exclusive->value == 42);

        // Move semantics: transfer ownership
        ExclusiveAsset<TestResource> moved = std::move(exclusive);
        CHECK(exclusive == nullptr);
        REQUIRE(moved != nullptr);
        CHECK(moved->value == 42);
        CHECK(alive_instances == 1);
    }
    // Scope exit must destruct exclusive resource
    CHECK(alive_instances == 0);

    // 2. Shared Ownership (std::shared_ptr) & Weak References (std::weak_ptr)
    AssetLifetimeTracker<TestResource> tracker{nullptr};
    {
        SharedAsset<TestResource> shared1 = make_shared_asset<TestResource>(100, &alive_instances);
        CHECK(alive_instances == 1);
        CHECK(shared1.use_count() == 1);

        tracker = AssetLifetimeTracker<TestResource>(shared1);
        CHECK(tracker.is_alive());
        CHECK(tracker.use_count() == 1);

        {
            SharedAsset<TestResource> shared2 = shared1;
            CHECK(shared1.use_count() == 2);
            CHECK(tracker.use_count() == 2);
        }
        CHECK(shared1.use_count() == 1);
    }
    // Scope exit: refcount reaches 0, tracker reflects expiration
    CHECK(alive_instances == 0);
    CHECK_FALSE(tracker.is_alive());
    CHECK(tracker.lock() == nullptr);

    // 3. Cyclic Dependency Prevention via WeakAsset
    int node_count = 0;
    struct Node {
        int* counter{nullptr};
        std::string name;
        SharedAsset<Node> child{nullptr};
        WeakAsset<Node> parent; // Weak to prevent cycle

        explicit Node(std::string n, int* cnt = nullptr) : counter(cnt), name(std::move(n)) {
            if (counter) ++(*counter);
        }
        ~Node() {
            if (counter) --(*counter);
        }
    };

    {
        auto root = make_shared_asset<Node>("Root", &node_count);
        auto leaf = make_shared_asset<Node>("Leaf", &node_count);
        root->child = leaf;
        leaf->parent = root; // Non-owning back-pointer

        CHECK(node_count == 2);
        CHECK(root.use_count() == 1);
        CHECK(leaf.use_count() == 2);
    }
    // With weak back-pointer, both root and leaf destruct completely (zero cycle leak!)
    CHECK(node_count == 0);

    // 4. SmartAssetHandle Wrapper
    {
        Uuid asset_id = Uuid::generate_random();
        SmartAssetHandle<TestResource> handle(asset_id, make_shared_asset<TestResource>(999, &alive_instances));
        CHECK(handle.is_valid());
        CHECK(handle.id() == asset_id);
        CHECK(handle->value == 999);
        CHECK((*handle).value == 999);
        CHECK(handle.use_count() == 1);

        handle.reset();
        CHECK_FALSE(handle.is_valid());
        CHECK(alive_instances == 0);
    }
}

TEST_CASE("PH4-02.01.004: Zero-Copy Views (std::string_view & std::span) Memory Audit") {
    // 1. Zero-Copy String Trimming
    std::string_view padded = "   \t  Copper 99.9% Electrolytic \r\n  ";
    std::string_view trimmed = trim_view(padded);
    CHECK(trimmed == "Copper 99.9% Electrolytic");
    CHECK(trimmed.data() == padded.data() + 6); // Sub-view points directly into source buffer!

    // 2. Zero-Allocation StringViewTokenizer
    std::string_view csv_line = "Cu,Copper,8960.0,385.0,5.96e7,Solid";
    StringViewTokenizer tok(csv_line, ',');
    std::vector<std::string_view> tokens;
    for (std::string_view token : tok) {
        tokens.push_back(token);
    }
    REQUIRE(tokens.size() == 6);
    CHECK(tokens[0] == "Cu");
    CHECK(tokens[1] == "Copper");
    CHECK(tokens[5] == "Solid");

    // 3. Fast Zero-Copy Numeric Parsing from string_view
    double density = parse_double_view(tokens[2]);
    CHECK(std::abs(density - 8960.0) < 1e-4);
    double cond = parse_double_view(tokens[4]);
    CHECK(std::abs(cond - 5.96e7) < 1e2);

    int64_t parsed_int = parse_int64_view("  -1048576  ");
    CHECK(parsed_int == -1048576);

    // 4. Dot Product over std::span with zero allocation
    std::array<double, 8> vec_a{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    std::array<double, 8> vec_b{2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0};
    double dot = dot_product_span(std::span<const double>(vec_a), std::span<const double>(vec_b));
    CHECK(dot == 72.0);

    // 5. SolidMesh3D Span Append
    shunya::cad3d::SolidMesh3D mesh;
    std::array<shunya::cad3d::Point3D, 3> pts{{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}};
    std::array<std::array<uint32_t, 3>, 1> tris{{{0, 1, 2}}};

    mesh.append_vertices(std::span<const shunya::cad3d::Point3D>(pts));
    mesh.append_triangles(std::span<const std::array<uint32_t, 3>>(tris));
    CHECK(mesh.vertices.size() == 3);
    CHECK(mesh.triangles.size() == 1);

    // 6. Material string_view setters
    Material mat;
    mat.set_name(trimmed);
    CHECK(mat.name() == "Copper 99.9% Electrolytic");
    mat.set_description(std::string_view("High purity wire"));
    CHECK(mat.description() == "High purity wire");
}

TEST_CASE("PH4-02.01.005: LRU Asset Cache Manager and Memory Budget Clamping") {
    using namespace shunya::perf;

    struct DummyMesh {
        std::string name;
        size_t vertex_count{0};
        [[nodiscard]] size_t byte_size() const noexcept {
            return vertex_count * 32; // 32 bytes per vertex
        }
    };

    std::vector<std::pair<std::string, std::string>> evicted_log;
    auto on_evict = [&](const std::string& key, const std::shared_ptr<DummyMesh>& mesh) {
        evicted_log.emplace_back(key, mesh ? mesh->name : "");
    };

    // Cache with 100 KB max budget (102,400 bytes)
    const size_t max_budget = 100 * 1024;
    LruAssetCache<std::string, std::shared_ptr<DummyMesh>> cache(max_budget, on_evict);

    CHECK(cache.capacity_bytes() == max_budget);
    CHECK(cache.current_bytes() == 0);
    CHECK(cache.empty());

    // 1. Insert 3 meshes of 40 KB each (1,280 vertices each = 40,960 bytes)
    auto m1 = std::make_shared<DummyMesh>(DummyMesh{"mesh_chassis", 1280});
    auto m2 = std::make_shared<DummyMesh>(DummyMesh{"mesh_wheel", 1280});
    auto m3 = std::make_shared<DummyMesh>(DummyMesh{"mesh_battery_pack", 1280});

    cache.insert("chassis", m1);
    CHECK(cache.size() == 1);
    CHECK(cache.current_bytes() == 40960);
    CHECK(cache.contains("chassis"));

    cache.insert("wheel", m2);
    CHECK(cache.size() == 2);
    CHECK(cache.current_bytes() == 81920);

    // 2. Access chassis to promote it to MRU (wheel is now LRU)
    auto retrieved = cache.get("chassis");
    REQUIRE(retrieved.has_value());
    CHECK((*retrieved)->name == "mesh_chassis");
    CHECK(cache.hit_count() == 1);
    CHECK(cache.miss_count() == 0);
    CHECK(cache.hit_rate() == 1.0);

    // 3. Insert 3rd mesh (40 KB) -> Total would be 122,880 > 102,400.
    // LRU item was 'wheel', so 'wheel' must be evicted.
    cache.insert("battery", m3);
    CHECK(cache.size() == 2); // chassis and battery remain
    CHECK(cache.current_bytes() == 81920);
    CHECK(cache.eviction_count() == 1);
    CHECK(!cache.contains("wheel"));
    CHECK(cache.contains("chassis"));
    CHECK(cache.contains("battery"));

    REQUIRE(evicted_log.size() == 1);
    CHECK(evicted_log[0].first == "wheel");
    CHECK(evicted_log[0].second == "mesh_wheel");

    // 4. Cache Miss check
    auto missed = cache.get("wheel");
    CHECK(!missed.has_value());
    CHECK(cache.miss_count() == 1);
    CHECK(cache.hit_rate() == 0.5); // 1 hit, 1 miss

    // 5. Dynamic budget reduction clamping
    // Reduce budget to 50 KB -> One more mesh must be evicted (chassis was accessed before battery, so chassis is LRU)
    cache.set_capacity_bytes(50 * 1024);
    CHECK(cache.capacity_bytes() == 50 * 1024);
    CHECK(cache.size() == 1);
    CHECK(cache.contains("battery"));
    CHECK(!cache.contains("chassis"));
    CHECK(cache.eviction_count() == 2);
    CHECK(evicted_log.size() == 2);
    CHECK(evicted_log[1].first == "chassis");

    // 6. Explicit manual erase
    bool erased = cache.erase("battery");
    CHECK(erased);
    CHECK(cache.empty());
    CHECK(cache.current_bytes() == 0);
    CHECK(evicted_log.size() == 3);
}
