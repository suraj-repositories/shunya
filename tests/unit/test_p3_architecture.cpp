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
#include "shunya/perf/task_graph_engine.hpp"
#include "shunya/perf/work_stealing_pool.hpp"
#include "shunya/cad3d/bvh_tree.hpp"
#include "shunya/cad3d/rhi_renderer_engine.hpp"
#include "shunya/cad3d/hierarchical_culler.hpp"
#include "shunya/cad3d/gpu_vertex_buffer.hpp"
#include "shunya/cad3d/gpu_compute_pipeline.hpp"
#include "shunya/cad3d/shader_pipeline_cache.hpp"
#include "shunya/perf/simd_math.hpp"
#include "shunya/perf/thread_scaling_benchmark.hpp"
#include "shunya/perf/performance_hud.hpp"
#include "shunya/perf/simulation_hardware_profiler.hpp"
#include "shunya/perf/chrome_tracing_exporter.hpp"
#include "shunya/perf/hot_loop_profiler.hpp"
#include "shunya/perf/memory_budget.hpp"
#include "shunya/mat/formation_scenarios.hpp"
#include "shunya/mat/compound_validator.hpp"
#include "shunya/mat/gibbs_free_energy.hpp"
#include "shunya/mat/hume_rothery.hpp"
#include "shunya/mat/solution_solubility_engine.hpp"
#include "shunya/mat/directional_layup_engine.hpp"
#include "shunya/bench/perf_regression_gate.hpp"
#include "shunya/core/base/event_bus.hpp"

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

TEST_CASE("PH4-02.01.006: Memory Leak & Double-Free Sanitizer (ASan/LSan) CI Configuration") {
    // 1. Verify CMakeLists.txt contains SHUNYA_ENABLE_SANITIZERS option
    std::filesystem::path root_dir(SHUNYA_SOURCE_DIR);
    std::ifstream cmakelists(root_dir / "CMakeLists.txt");
    REQUIRE(cmakelists.is_open());
    std::string cmake_content((std::istreambuf_iterator<char>(cmakelists)),
                              std::istreambuf_iterator<char>());
    CHECK(cmake_content.find("SHUNYA_ENABLE_SANITIZERS") != std::string::npos);
    CHECK(cmake_content.find("-fsanitize=address,undefined") != std::string::npos);

    // 2. Verify CMakePresets.json contains ci-debug-sanitizer preset
    std::ifstream presets(root_dir / "CMakePresets.json");
    REQUIRE(presets.is_open());
    std::string presets_content((std::istreambuf_iterator<char>(presets)),
                                std::istreambuf_iterator<char>());
    CHECK(presets_content.find("ci-debug-sanitizer") != std::string::npos);
    CHECK(presets_content.find("build-sanitizer") != std::string::npos);
    CHECK(presets_content.find("test-sanitizer") != std::string::npos);

    // 3. Verify .github/workflows/ci.yml runs sanitizer-check job
    std::ifstream ci_yaml(root_dir / ".github" / "workflows" / "ci.yml");
    REQUIRE(ci_yaml.is_open());
    std::string ci_content((std::istreambuf_iterator<char>(ci_yaml)),
                           std::istreambuf_iterator<char>());
    CHECK(ci_content.find("sanitizer-check") != std::string::npos);
    CHECK(ci_content.find("ASAN_OPTIONS") != std::string::npos);

    // 4. Runtime memory leak detector fixture sanity check
    shunya::test::ScopedMemoryLeakDetector detector("SanitizerFixtureScope", 50 * 1024 * 1024);
    CHECK(!detector.has_leak());
}

TEST_CASE("PH4-02.02.001: Task-Graph Execution Engine Multi-Domain Parallelization") {
    using namespace shunya::perf;

    // 1. Cycle Detection Verification
    {
        TaskGraphExecutionEngine cyclic_engine;
        size_t t0 = cyclic_engine.add_task("Node0", TaskDomain::General, []() {});
        size_t t1 = cyclic_engine.add_task("Node1", TaskDomain::General, []() {}, {t0});
        (void)cyclic_engine.add_task("Node2_Cycle", TaskDomain::General, []() {}, {t1, t0});
        CHECK(!cyclic_engine.has_cycle());

        // Now test cycle detection when invalid dependency index or cycle is present
        TaskGraphExecutionEngine bad_engine;
        (void)bad_engine.add_task("A", TaskDomain::General, []() {});
        (void)bad_engine.add_task("B_OutOfRange", TaskDomain::General, []() {}, {999});
        CHECK(bad_engine.has_cycle());
        auto bad_sort = bad_engine.topological_sort();
        CHECK(!bad_sort.is_ok());
    }

    // 2. Multi-Domain DAG: Physics + Circuit + Mesh Tessellation
    TaskGraphExecutionEngine engine;
    std::atomic<int> exec_flags{0};
    std::atomic<int> physics_count{0};
    std::atomic<int> circuit_count{0};
    std::atomic<int> mesh_count{0};

    // Root node
    size_t n_init = engine.add_task("Init_Timestep", TaskDomain::General, [&]() {
        exec_flags.fetch_or(1);
    });

    // Decoupled Physics Bodies
    size_t n_phys1 = engine.add_task("Physics_Chassis", TaskDomain::PhysicsBody, [&]() {
        CHECK((exec_flags.load() & 1) != 0);
        physics_count.fetch_add(1);
    }, {n_init});

    size_t n_phys2 = engine.add_task("Physics_WheelLeft", TaskDomain::PhysicsBody, [&]() {
        CHECK((exec_flags.load() & 1) != 0);
        physics_count.fetch_add(1);
    }, {n_init});

    size_t n_phys3 = engine.add_task("Physics_WheelRight", TaskDomain::PhysicsBody, [&]() {
        CHECK((exec_flags.load() & 1) != 0);
        physics_count.fetch_add(1);
    }, {n_init});

    // Decoupled Circuit Matrix Solve
    size_t n_circ = engine.add_task("Circuit_MNA_Solve", TaskDomain::CircuitMatrix, [&]() {
        CHECK((exec_flags.load() & 1) != 0);
        circuit_count.fetch_add(1);
    }, {n_init});

    // Decoupled Mesh Tessellation
    size_t n_mesh1 = engine.add_task("Mesh_Tessellate_LOD0", TaskDomain::MeshTessellation, [&]() {
        CHECK((exec_flags.load() & 1) != 0);
        mesh_count.fetch_add(1);
    }, {n_init});

    size_t n_mesh2 = engine.add_task("Mesh_Tessellate_LOD1", TaskDomain::MeshTessellation, [&]() {
        CHECK((exec_flags.load() & 1) != 0);
        mesh_count.fetch_add(1);
    }, {n_init});

    // Electro-Mechanical Coupling Node (aggregates physics and circuit)
    size_t n_couple = engine.add_task("Coupling_Torque", TaskDomain::General, [&]() {
        CHECK(physics_count.load() == 3);
        CHECK(circuit_count.load() == 1);
        exec_flags.fetch_or(2);
    }, {n_phys1, n_phys2, n_phys3, n_circ});

    // Final Sync Node (depends on coupling and mesh tessellation)
    size_t n_sync = engine.add_task("Render_State_Sync", TaskDomain::General, [&]() {
        CHECK((exec_flags.load() & 2) != 0);
        CHECK(mesh_count.load() == 2);
        exec_flags.fetch_or(4);
    }, {n_couple, n_mesh1, n_mesh2});

    (void)n_sync;

    // 3. Structural & Domain Checks
    CHECK(engine.task_count() == 9);
    CHECK(engine.domain_task_count(TaskDomain::PhysicsBody) == 3);
    CHECK(engine.domain_task_count(TaskDomain::CircuitMatrix) == 1);
    CHECK(engine.domain_task_count(TaskDomain::MeshTessellation) == 2);
    CHECK(engine.domain_task_count(TaskDomain::General) == 3);
    CHECK(!engine.has_cycle());

    auto sort_res = engine.topological_sort();
    REQUIRE(sort_res.is_ok());
    const auto& order = sort_res.value();
    REQUIRE(order.size() == 9);
    CHECK(order.front() == n_init);
    CHECK(order.back() == n_sync);

    // 4. Concurrent Multi-Threaded Execution
    WorkStealingPool pool(4);
    auto run_res = engine.execute(pool);
    REQUIRE(run_res.is_ok());

    const TaskGraphMetrics& m = run_res.value();
    CHECK(m.total_tasks == 9);
    CHECK(m.completed_tasks == 9);
    CHECK(m.failed_tasks == 0);
    CHECK(m.total_wall_time_us > 0.0);
    CHECK(exec_flags.load() == 7);
    CHECK(physics_count.load() == 3);
    CHECK(circuit_count.load() == 1);
    CHECK(mesh_count.load() == 2);

    // 5. Reusable Execution Plan (Simulating Next Time Step)
    exec_flags.store(0);
    physics_count.store(0);
    circuit_count.store(0);
    mesh_count.store(0);

    auto step2_res = engine.execute(pool);
    REQUIRE(step2_res.is_ok());
    CHECK(step2_res.value().completed_tasks == 9);
    CHECK(exec_flags.load() == 7);
    CHECK(physics_count.load() == 3);
    CHECK(circuit_count.load() == 1);
    CHECK(mesh_count.load() == 2);
}

TEST_CASE("PH4-02.02.002: Parallelized BVH Reconstruction via Work-Stealing Pool") {
    // 1. Generate a procedural dense grid mesh (50x50 cells = 5,000 triangles)
    shunya::cad3d::SolidMesh3D mesh;
    constexpr int GRID_RES = 50;
    mesh.vertices.reserve((GRID_RES + 1) * (GRID_RES + 1));
    for (int y = 0; y <= GRID_RES; ++y) {
        for (int x = 0; x <= GRID_RES; ++x) {
            double xf = static_cast<double>(x) * 0.2;
            double yf = static_cast<double>(y) * 0.2;
            double zf = std::sin(xf) * std::cos(yf) * 0.5;
            mesh.vertices.push_back({xf, yf, zf});
        }
    }
    for (int y = 0; y < GRID_RES; ++y) {
        for (int x = 0; x < GRID_RES; ++x) {
            uint32_t i0 = static_cast<uint32_t>(y * (GRID_RES + 1) + x);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = static_cast<uint32_t>((y + 1) * (GRID_RES + 1) + x);
            uint32_t i3 = i2 + 1;
            mesh.triangles.push_back({i0, i1, i2});
            mesh.triangles.push_back({i1, i3, i2});
        }
    }
    REQUIRE(mesh.triangles.size() == 5000);

    // 2. Build BVH Serially
    shunya::cad3d::LinearBvh3D serial_bvh;
    serial_bvh.build(mesh, 4);
    REQUIRE(serial_bvh.is_built());
    CHECK(serial_bvh.node_count() > 0);

    // 3. Build BVH in Parallel with WorkStealingPool
    shunya::perf::WorkStealingPool pool(4);
    shunya::cad3d::LinearBvh3D parallel_bvh;
    parallel_bvh.build_parallel(mesh, pool, 4);
    REQUIRE(parallel_bvh.is_built());
    CHECK(parallel_bvh.node_count() > 0);

    // 4. Verify raycast parity across 100 sample rays
    for (int i = 0; i < 100; ++i) {
        double rx = 0.5 + (i % 10) * 0.9;
        double ry = 0.5 + (i / 10) * 0.9;
        shunya::cad3d::Ray3D ray{{rx, ry, 5.0}, {0.0, 0.0, -1.0}};

        auto hit_serial = serial_bvh.raycast(ray);
        auto hit_parallel = parallel_bvh.raycast(ray);

        CHECK(hit_serial.hit == hit_parallel.hit);
        if (hit_serial.hit && hit_parallel.hit) {
            CHECK(std::abs(hit_serial.distance - hit_parallel.distance) < 1e-4);
            CHECK(std::abs(hit_serial.point.z - hit_parallel.point.z) < 1e-4);
        }

        CHECK(serial_bvh.any_hit(ray) == parallel_bvh.any_hit(ray));
    }

    // 5. Query AABB test
    shunya::cad3d::BBox3D query_box{{2.0, 2.0, -1.0}, {4.0, 4.0, 1.0}};
    auto serial_aabb = serial_bvh.query_aabb(query_box);
    auto parallel_aabb = parallel_bvh.query_aabb(query_box);
    CHECK(serial_aabb.size() > 0);
    CHECK(parallel_aabb.size() > 0);
    CHECK(serial_aabb.size() == parallel_aabb.size());
}

TEST_CASE("PH4-02.02.003: Vectorized Constitutive Material Loops & Geometric AABB Queries") {
    using namespace shunya::perf;

    // 1. Vectorized Constitutive TCR Resistivity Loop
    constexpr size_t N = 64;
    std::vector<float> temps(N);
    std::vector<float> out_rho(N);
    for (size_t i = 0; i < N; ++i) {
        temps[i] = 273.15f + static_cast<float>(i) * 10.0f; // 0 C to 630 C
    }
    const float rho0 = 1.68e-8f; // Copper resistivity
    const float alpha = 0.00386f;
    const float t0 = 293.15f;
    SimdMath::constitutive_resistivity_tcr_f32(rho0, alpha, t0, temps, out_rho);

    for (size_t i = 0; i < N; ++i) {
        float expected_rho = rho0 * (1.0f + alpha * (temps[i] - t0));
        CHECK(std::abs(out_rho[i] - expected_rho) < 1e-12f);
    }

    // 2. Vectorized Joule Heating: P = J^2 * rho
    std::vector<float> j_sq(N);
    std::vector<float> out_power(N);
    for (size_t i = 0; i < N; ++i) {
        j_sq[i] = 1e10f + static_cast<float>(i) * 1e8f;
    }
    SimdMath::constitutive_joule_heating_f32(j_sq, out_rho, out_power);
    for (size_t i = 0; i < N; ++i) {
        float expected_power = j_sq[i] * out_rho[i];
        CHECK(std::abs(out_power[i] - expected_power) < 1e-3f);
    }

    // 3. Vectorized Thermal Expansion: dL = L0 * alpha * dT
    std::vector<float> dt(N);
    std::vector<float> l0(N);
    std::vector<float> out_dl(N);
    for (size_t i = 0; i < N; ++i) {
        dt[i] = static_cast<float>(i) * 2.0f;
        l0[i] = 0.1f + static_cast<float>(i) * 0.01f;
    }
    const float linear_alpha = 16.5e-6f; // Copper alpha
    SimdMath::constitutive_thermal_expansion_f32(linear_alpha, dt, l0, out_dl);
    for (size_t i = 0; i < N; ++i) {
        float expected_dl = l0[i] * linear_alpha * dt[i];
        CHECK(std::abs(out_dl[i] - expected_dl) < 1e-9f);
    }

    // 4. Vectorized Elastic Hookean Stress: sigma = E * epsilon
    std::vector<float> strains(N);
    std::vector<float> out_stresses(N);
    for (size_t i = 0; i < N; ++i) {
        strains[i] = 0.0001f * static_cast<float>(i);
    }
    const float E = 117e9f; // Copper Young's modulus
    SimdMath::constitutive_elastic_stress_f32(E, strains, out_stresses);
    for (size_t i = 0; i < N; ++i) {
        float expected_sigma = E * strains[i];
        CHECK(std::abs(out_stresses[i] - expected_sigma) < 1e-1f);
    }

    // 5. Vectorized Geometric Bounding Box Queries (SoA layout)
    constexpr size_t NUM_BOXES = 128;
    std::vector<float> b_min_x(NUM_BOXES), b_min_y(NUM_BOXES), b_min_z(NUM_BOXES);
    std::vector<float> b_max_x(NUM_BOXES), b_max_y(NUM_BOXES), b_max_z(NUM_BOXES);

    for (size_t i = 0; i < NUM_BOXES; ++i) {
        float cx = static_cast<float>(i % 16) * 2.0f;
        float cy = static_cast<float>((i / 16) % 8) * 2.0f;
        float cz = 0.0f;
        b_min_x[i] = cx - 0.5f;
        b_max_x[i] = cx + 0.5f;
        b_min_y[i] = cy - 0.5f;
        b_max_y[i] = cy + 0.5f;
        b_min_z[i] = cz - 0.5f;
        b_max_z[i] = cz + 0.5f;
    }

    BBox3DSoA soa_boxes{b_min_x, b_min_y, b_min_z, b_max_x, b_max_y, b_max_z};

    // AABB Overlap Query: box covering [1.0, 1.0, -1.0] to [5.0, 5.0, 1.0]
    std::vector<uint32_t> matched_indices(NUM_BOXES);
    size_t matches = SimdMath::batch_aabb_overlap_query(
        soa_boxes, 1.0f, 1.0f, -1.0f, 5.0f, 5.0f, 1.0f, matched_indices);

    CHECK(matches > 0);
    for (size_t m = 0; m < matches; ++m) {
        uint32_t idx = matched_indices[m];
        CHECK(b_max_x[idx] >= 1.0f);
        CHECK(b_min_x[idx] <= 5.0f);
        CHECK(b_max_y[idx] >= 1.0f);
        CHECK(b_min_y[idx] <= 5.0f);
        CHECK(b_max_z[idx] >= -1.0f);
        CHECK(b_min_z[idx] <= 1.0f);
    }

    // Raycast query: ray originating at (2.0, 2.0, 10.0) aimed along -Z with small angle
    // direction = (0.01, 0.01, -1.0) -> inv = (100.0, 100.0, -1.0)
    std::vector<uint32_t> ray_hits(NUM_BOXES);
    size_t num_ray_hits = SimdMath::batch_aabb_ray_intersect_f32(
        soa_boxes, 2.0f, 2.0f, 10.0f, 100.0f, 100.0f, -1.0f, 20.0f, ray_hits);
    CHECK(num_ray_hits >= 1);
    for (size_t h = 0; h < num_ray_hits; ++h) {
        uint32_t idx = ray_hits[h];
        CHECK(b_min_x[idx] <= 2.0f);
        CHECK(b_max_x[idx] >= 2.0f);
        CHECK(b_min_y[idx] <= 2.0f);
        CHECK(b_max_y[idx] >= 2.0f);
    }
}

TEST_CASE("PH4-02.02.004: EventBus Read-Write Locks & Wait-Free Lockless Telemetry") {
    struct TestTelemetryTick {
        int step_id;
    };

    EventBus bus;

    // 1. Concurrent Shared-Lock Publishing across 8 worker threads
    std::atomic<int> received_count{0};
    auto sub_id = bus.subscribe<TestTelemetryTick>([&](const TestTelemetryTick& ev) {
        (void)ev;
        received_count.fetch_add(1, std::memory_order_relaxed);
    });
    CHECK(sub_id > 0);

    constexpr int THREADS = 8;
    constexpr int EVENTS_PER_THREAD = 500;
    std::vector<std::thread> workers;
    workers.reserve(THREADS);

    for (int t = 0; t < THREADS; ++t) {
        workers.emplace_back([&, t]() {
            for (int e = 0; e < EVENTS_PER_THREAD; ++e) {
                bus.publish_immediate(TestTelemetryTick{t * EVENTS_PER_THREAD + e});
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }
    workers.clear();

    CHECK(received_count.load() == THREADS * EVENTS_PER_THREAD);

    // 2. Unsubscribe under write lock
    CHECK(bus.unsubscribe(sub_id));
    bus.publish_immediate(TestTelemetryTick{999999});
    CHECK(received_count.load() == THREADS * EVENTS_PER_THREAD);

    // 3. High-Throughput Wait-Free Lockless Telemetry Streaming
    // Producer thread: streams 2,000 telemetry metrics without locking
    std::thread producer([&]() {
        for (uint32_t i = 0; i < 2000; ++i) {
            bus.emit_telemetry(1, static_cast<double>(i) * 0.1, i * 1000);
        }
    });

    producer.join();

    CHECK(bus.telemetry_queued_count() > 0);

    std::vector<TelemetryMetric> batch(3000);
    size_t flushed = bus.flush_telemetry(batch);
    CHECK(flushed == 2000);
    CHECK(batch[0].channel_id == 1);
    CHECK(std::abs(batch[0].value - 0.0) < 1e-6);
    CHECK(std::abs(batch[1999].value - 199.9) < 1e-4);
    CHECK(bus.telemetry_queued_count() == 0);

    bus.clear();
    CHECK(bus.telemetry_queued_count() == 0);
}

TEST_CASE("PH4-02.02.005: Background File Serialization, Parsing & Cancellation") {
    EngineeringWorkspaceState ws_source;
    REQUIRE(ws_source.seed_lithium_battery_workflow().is_ok());

    const auto temp_dir = std::filesystem::temp_directory_path() / "shunya_test_bg_io";
    std::filesystem::create_directories(temp_dir);
    const auto save_path = temp_dir / "async_test.shunya4";

    // 1. Asynchronous Save with Progress Monitoring
    std::vector<double> save_progresses;
    std::vector<std::string> save_stages;
    auto save_fut = ws_source.save_to_file_async(
        save_path,
        {},
        [&](const ProjectSaveProgress& p) {
            save_progresses.push_back(p.fraction);
            save_stages.push_back(p.stage);
        }
    );

    REQUIRE(save_fut.valid());
    auto save_res = save_fut.get();
    REQUIRE(save_res.is_ok());
    CHECK(std::filesystem::exists(save_path));
    CHECK_FALSE(save_progresses.empty());
    CHECK(save_progresses.back() == 1.0);

    // 2. Asynchronous Load into Clean Workspace with Progress Monitoring
    EngineeringWorkspaceState ws_dest;
    std::vector<double> load_progresses;
    std::vector<std::string> load_stages;
    auto load_fut = ws_dest.load_from_file_async(
        save_path,
        [&](const ProjectLoadProgress& p) {
            load_progresses.push_back(p.fraction);
            load_stages.push_back(p.stage);
        }
    );

    REQUIRE(load_fut.valid());
    auto load_res = load_fut.get();
    REQUIRE(load_res.is_ok());
    CHECK_FALSE(load_progresses.empty());
    CHECK(load_progresses.back() == 1.0);

    // Verify entity fidelity between source and dest
    CHECK(ws_dest.mixture_experiments().size() == ws_source.mixture_experiments().size());
    CHECK(ws_dest.object_manager().objects().size() == ws_source.object_manager().objects().size());
    CHECK(ws_dest.circuits().size() == ws_source.circuits().size());

    // 3. Cooperative Cancellation during Save
    CancellationToken cancel_tok;
    cancel_tok.cancel(); // Pre-cancel
    const auto cancelled_path = temp_dir / "cancelled.shunya4";
    auto cancelled_save_fut = ws_source.save_to_file_async(
        cancelled_path,
        {},
        nullptr,
        cancel_tok
    );
    auto cancelled_save_res = cancelled_save_fut.get();
    CHECK(cancelled_save_res.is_err());
    CHECK_FALSE(std::filesystem::exists(cancelled_path));

    // 4. Cooperative Cancellation during Load
    auto cancelled_load_fut = ws_dest.load_from_file_async(
        save_path,
        nullptr,
        cancel_tok
    );
    auto cancelled_load_res = cancelled_load_fut.get();
    CHECK(cancelled_load_res.is_err());

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("PH4-02.02.006: Multi-Threaded Scaling (1 to 32 Threads) & False Sharing Elimination") {
    using namespace shunya::perf;

    // 1. Benchmark scaling from 1 to 32 worker threads
    const std::vector<size_t> thread_configs = {1, 2, 4, 8, 16, 32};
    auto report = ThreadScalingBenchmark::benchmark_parallel_for(50000, thread_configs);

    REQUIRE(report.results.size() == 6);
    CHECK(report.meets_scaling_floor);

    // Assert bit-for-bit mathematical consistency across all thread counts
    const uint64_t golden_checksum = report.results[0].checksum;
    for (const auto& res : report.results) {
        CHECK(res.checksum == golden_checksum);
        CHECK(res.duration_us > 0.0);
    }

    // Assert speedup / scaling resilience on multi-core
    CHECK(report.results[1].speedup > 0.75); // 2 threads vs 1 thread (tolerant to transient background CPU contention)

    // 2. Evaluate false sharing cache line isolation
    CHECK(ThreadScalingBenchmark::evaluate_false_sharing_isolation(100000));
}

TEST_CASE("PH4-02.03.001: Qt Quick RHI Viewport Hardware Rendering Pipeline") {
    using namespace shunya::cad3d;
    using shunya::core::base::math::Matrix4x4;

    // 1. Generate test geometry (box)
    SolidMesh3D box_mesh;
    box_mesh.vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
    };
    box_mesh.triangles = {
        {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4}, {2, 3, 7}, {2, 7, 6},
        {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}
    };

    ViewportCamera cam;
    cam.position = {3.0, 3.0, 4.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.aspect_ratio = 4.0 / 3.0;
    cam.fov_degrees = 45.0;

    std::vector<RhiDrawItem> items{
        {&box_mesh, Matrix4x4<double>::identity(), {0.8, 0.3, 0.2, 1.0}, 0.3, 0.2}
    };

    // 2. Initialize and render on Vulkan backend
    RhiRendererEngine vk_engine;
    REQUIRE(vk_engine.initialize({RhiBackendApi::Vulkan, 320, 240, true}));
    CHECK(vk_engine.is_initialized());
    CHECK(vk_engine.active_backend() == RhiBackendApi::Vulkan);

    vk_engine.begin_frame({0.1, 0.1, 0.1, 1.0});
    vk_engine.render_scene(cam, items);
    vk_engine.end_frame();

    CHECK(vk_engine.frame_metrics().draw_calls == 1);
    CHECK(vk_engine.frame_metrics().triangles_submitted == 12);
    CHECK(vk_engine.frame_metrics().pixels_shaded > 0);

    // 3. Initialize and render on Direct3D12 backend
    RhiRendererEngine d3d12_engine;
    REQUIRE(d3d12_engine.initialize({RhiBackendApi::Direct3D12, 320, 240, true}));
    d3d12_engine.begin_frame({0.1, 0.1, 0.1, 1.0});
    d3d12_engine.render_scene(cam, items);
    d3d12_engine.end_frame();

    CHECK(d3d12_engine.frame_metrics().draw_calls == 1);
    CHECK(d3d12_engine.frame_metrics().pixels_shaded > 0);

    // Assert rendering parity between backends
    CHECK(vk_engine.frame_metrics().pixels_shaded == d3d12_engine.frame_metrics().pixels_shaded);
    CHECK(vk_engine.framebuffer().quantized_checksum() == d3d12_engine.framebuffer().quantized_checksum());
}

TEST_CASE("PH4-02.03.002: GPU Instanced Rendering for Repetitive Pins and Fasteners") {
    using namespace shunya::cad3d;
    using shunya::core::base::math::Matrix4x4;

    // 1. Create a base pin/fastener mesh (12 triangles)
    SolidMesh3D pin_mesh;
    pin_mesh.vertices = {
        {-0.1, -0.5, -0.1}, {0.1, -0.5, -0.1}, {0.1, 0.5, -0.1}, {-0.1, 0.5, -0.1},
        {-0.1, -0.5,  0.1}, {0.1, -0.5,  0.1}, {0.1, 0.5,  0.1}, {-0.1, 0.5,  0.1}
    };
    pin_mesh.triangles = {
        {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4}, {2, 3, 7}, {2, 7, 6},
        {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}
    };

    // 2. Build instanced batch representing a 64-pin IC package (8x8 pin grid)
    RhiInstancedBatch ic_pins_batch;
    ic_pins_batch.mesh = &pin_mesh;
    ic_pins_batch.base_color = {0.95, 0.85, 0.2, 1.0}; // Gold plated pins
    ic_pins_batch.reserve(64);

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            const double x = (c - 3.5) * 0.4;
            const double z = (r - 3.5) * 0.4;
            ic_pins_batch.add_instance(Matrix4x4<double>::translation({x, 0.0, z}));
        }
    }
    CHECK(ic_pins_batch.size() == 64);

    // 3. Build instanced batch representing 4 corner mounting bolts
    RhiInstancedBatch bolts_batch;
    bolts_batch.mesh = &pin_mesh;
    bolts_batch.base_color = {0.7, 0.72, 0.75, 1.0}; // Steel bolts
    bolts_batch.add_instance(Matrix4x4<double>::translation({-2.0, 0.2, -2.0}));
    bolts_batch.add_instance(Matrix4x4<double>::translation({ 2.0, 0.2, -2.0}));
    bolts_batch.add_instance(Matrix4x4<double>::translation({-2.0, 0.2,  2.0}));
    bolts_batch.add_instance(Matrix4x4<double>::translation({ 2.0, 0.2,  2.0}));
    CHECK(bolts_batch.size() == 4);

    ViewportCamera cam;
    cam.position = {0.0, 6.0, 6.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.aspect_ratio = 4.0 / 3.0;
    cam.fov_degrees = 50.0;

    // Single IC pin batch render test
    RhiRendererEngine vk_engine;
    REQUIRE(vk_engine.initialize({RhiBackendApi::Vulkan, 320, 240, true}));
    vk_engine.begin_frame();
    vk_engine.render_scene(cam, {ic_pins_batch.build()});
    vk_engine.end_frame();

    // Verify GPU instancing metric guarantees: exactly 1 draw call for all 64 pins!
    CHECK(vk_engine.frame_metrics().draw_calls == 1);
    CHECK(vk_engine.frame_metrics().instances_rendered == 64);
    CHECK(vk_engine.frame_metrics().triangles_submitted == 64 * 12);
    CHECK(vk_engine.frame_metrics().pixels_shaded > 0);

    // Combined multi-batch render test (IC pins + bolts)
    vk_engine.begin_frame();
    vk_engine.render_scene(cam, {ic_pins_batch.build(), bolts_batch.build()});
    vk_engine.end_frame();

    // Exactly 2 draw calls for 68 distinct 3D parts
    CHECK(vk_engine.frame_metrics().draw_calls == 2);
    CHECK(vk_engine.frame_metrics().instances_rendered == 68);
    CHECK(vk_engine.frame_metrics().triangles_submitted == 68 * 12);
    CHECK(vk_engine.frame_metrics().pixels_shaded > 0);

    // Backend parity verification with Direct3D12
    RhiRendererEngine d3d12_engine;
    REQUIRE(d3d12_engine.initialize({RhiBackendApi::Direct3D12, 320, 240, true}));
    d3d12_engine.begin_frame();
    d3d12_engine.render_scene(cam, {ic_pins_batch.build(), bolts_batch.build()});
    d3d12_engine.end_frame();

    CHECK(d3d12_engine.frame_metrics().draw_calls == 2);
    CHECK(d3d12_engine.frame_metrics().instances_rendered == 68);
    CHECK(d3d12_engine.frame_metrics().pixels_shaded == vk_engine.frame_metrics().pixels_shaded);
    CHECK(d3d12_engine.framebuffer().quantized_checksum() == vk_engine.framebuffer().quantized_checksum());
}

TEST_CASE("PH4-02.03.003: Hierarchical View-Frustum Culling & Dynamic LOD Switching") {
    using namespace shunya::cad3d;
    using shunya::core::base::math::Matrix4x4;

    // 1. Create 3 distinct LOD meshes representing a mechanical bracket
    SolidMesh3D lod0_mesh; // 12 triangles (full detail)
    lod0_mesh.vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
    };
    lod0_mesh.triangles = {
        {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4}, {2, 3, 7}, {2, 7, 6},
        {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}
    };

    SolidMesh3D lod1_mesh; // 4 triangles (pyramid proxy)
    lod1_mesh.vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}, {0, 1, 0}
    };
    lod1_mesh.triangles = {
        {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}
    };

    SolidMesh3D lod2_mesh; // 2 triangles (billboard quad proxy)
    lod2_mesh.vertices = {
        {-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}
    };
    lod2_mesh.triangles = {
        {0, 1, 2}, {0, 2, 3}
    };

    // 2. Setup camera at (0, 0, 10) looking at (0, 0, 0)
    ViewportCamera cam;
    cam.position = {0.0, 0.0, 10.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.aspect_ratio = 1.0;
    cam.fov_degrees = 60.0;
    cam.near_plane = 0.5;
    cam.far_plane = 100.0;

    const auto frustum = cam.extract_frustum();

    // 3. Test FrustumContainment primitive queries
    CHECK(HierarchicalCuller::test_containment(frustum, {-0.5, -0.5, -0.5}, {0.5, 0.5, 0.5}) != FrustumContainment::Outside);
    CHECK(HierarchicalCuller::test_containment(frustum, {50.0, 50.0, 50.0}, {60.0, 60.0, 60.0}) == FrustumContainment::Outside);
    CHECK(HierarchicalCuller::test_containment(frustum, {-10.0, -10.0, 20.0}, {-5.0, -5.0, 25.0}) == FrustumContainment::Outside);

    // 4. Build a hierarchical assembly tree
    // Root Assembly
    //   |- Subassembly Visible (at origin, z = 0, distance ~ 10 from camera)
    //   |     |- Part A (at z = 7, dist ~ 3 -> LOD 0)
    //   |     |- Part B (at z = 0, dist ~ 10 -> LOD 1)
    //   |     |- Part C (at z = -12, dist ~ 22 -> LOD 2)
    //   |     |- Part D (at z = -50, dist ~ 60 -> Culled beyond max LOD distance 30)
    //   |- Subassembly Culled (at x = 200, z = 0 -> Far outside view frustum)
    //         |- Part E
    //         |- Part F
    AssemblyCullNode root;
    root.name = "RootAssembly";

    auto visible_subassembly = std::make_unique<AssemblyCullNode>();
    visible_subassembly->name = "VisibleSubassembly";

    auto make_lod_part = [&](std::string name, double z_pos) {
        LodPart part;
        part.name = std::move(name);
        part.transform = Matrix4x4<double>::translation({0.0, 0.0, z_pos});
        part.local_bbox_min = {-1, -1, -1};
        part.local_bbox_max = {1, 1, 1};
        part.lods = {
            {&lod0_mesh, 5.0, "LOD0"},   // dist <= 5: LOD0
            {&lod1_mesh, 15.0, "LOD1"},  // 5 < dist <= 15: LOD1
            {&lod2_mesh, 30.0, "LOD2"}   // 15 < dist <= 30: LOD2 (dist > 30 is culled)
        };
        return part;
    };

    visible_subassembly->parts.push_back(make_lod_part("Part_LOD0", 7.0));  // dist = 3.0
    visible_subassembly->parts.push_back(make_lod_part("Part_LOD1", 0.0));  // dist = 10.0
    visible_subassembly->parts.push_back(make_lod_part("Part_LOD2", -12.0)); // dist = 22.0
    visible_subassembly->parts.push_back(make_lod_part("Part_Culled_Dist", -50.0)); // dist = 60.0

    auto culled_subassembly = std::make_unique<AssemblyCullNode>();
    culled_subassembly->name = "CulledSubassembly";
    culled_subassembly->local_transform = Matrix4x4<double>::translation({200.0, 0.0, 0.0});
    culled_subassembly->parts.push_back(make_lod_part("Part_Offscreen1", 0.0));
    culled_subassembly->parts.push_back(make_lod_part("Part_Offscreen2", 5.0));

    root.children.push_back(std::move(visible_subassembly));
    root.children.push_back(std::move(culled_subassembly));

    // Update hierarchical world bounding boxes
    root.update_bounds();

    // 5. Execute Hierarchical Frustum Culling and LOD selection
    HierarchicalCuller culler;
    CullingMetrics metrics{};
    std::vector<RhiDrawItem> items = culler.cull_and_select_lod(root, cam, &metrics);

    // Verify hierarchical node culling:
    // Root tested (1), VisibleSubassembly tested (1), CulledSubassembly tested and culled (1)
    CHECK(metrics.nodes_tested == 3);
    CHECK(metrics.nodes_culled == 1);

    // Verify part level LOD metrics:
    // Exactly 3 parts rendered (dist 3 -> LOD0, dist 10 -> LOD1, dist 22 -> LOD2)
    CHECK(metrics.parts_rendered == 3);
    CHECK(metrics.lod_counts[0] == 1); // LOD 0 (12 tris)
    CHECK(metrics.lod_counts[1] == 1); // LOD 1 (4 tris)
    CHECK(metrics.lod_counts[2] == 1); // LOD 2 (2 tris)

    // Culled part at dist 60 was culled due to distance
    CHECK(metrics.parts_culled == 1);

    // Verify resulting draw items list
    REQUIRE(items.size() == 3);
    CHECK(items[0].mesh == &lod0_mesh);
    CHECK(items[1].mesh == &lod1_mesh);
    CHECK(items[2].mesh == &lod2_mesh);

    // 6. Feed culled and LOD-swapped draw items to RhiRendererEngine
    RhiRendererEngine engine;
    REQUIRE(engine.initialize({RhiBackendApi::Vulkan, 320, 240, true}));
    engine.begin_frame();
    engine.render_scene(cam, items);
    engine.end_frame();

    CHECK(engine.frame_metrics().draw_calls == 3);
    CHECK(engine.frame_metrics().triangles_submitted == 12 + 4 + 2);
    CHECK(engine.frame_metrics().pixels_shaded > 0);
}

TEST_CASE("PH4-02.03.004: Optimized 32-Byte Aligned GPU Vertex Buffer Layouts") {
    using namespace shunya::cad3d;

    // 1. Static compile-time alignment and size guarantees
    CHECK(sizeof(GpuVertex32) == 32);
    CHECK(alignof(GpuVertex32) == 32);

    CHECK(sizeof(GpuVertexTangent32) == 32);
    CHECK(alignof(GpuVertexTangent32) == 32);

    CHECK(sizeof(GpuVertexFull64) == 64);
    CHECK(alignof(GpuVertexFull64) == 32);

    // 2. High-precision Octahedral normal vector quantization test
    const std::vector<std::array<float, 3>> test_vectors = {
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, -1.0f},
        {0.57735f, 0.57735f, 0.57735f},
        {-0.57735f, 0.57735f, -0.57735f}
    };

    for (const auto& v : test_vectors) {
        const auto oct = GpuVertexBufferBuilder::encode_octahedral(v[0], v[1], v[2]);
        const auto decoded = GpuVertexBufferBuilder::decode_octahedral(oct[0], oct[1]);
        const float dot_prod = v[0] * decoded[0] + v[1] * decoded[1] + v[2] * decoded[2];
        CHECK(dot_prod > 0.999f); // Under 0.05 degrees angular error
    }

    // 3. Create test mesh (box)
    SolidMesh3D box_mesh;
    box_mesh.vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
    };
    box_mesh.triangles = {
        {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4}, {2, 3, 7}, {2, 7, 6},
        {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}
    };

    // 4. Test 32-byte standard interleaved vertex buffer build
    auto buf32 = GpuVertexBufferBuilder::build_interleaved_32(box_mesh);
    CHECK(buf32.vertex_count() == 8);
    CHECK(buf32.index_count() == 36);
    CHECK(buf32.vertex_buffer_bytes() == 8 * 32);
    CHECK(buf32.is_32byte_aligned());
    CHECK(GpuVertexBufferBuilder::verify_buffer_alignment(buf32.vertices.data(), sizeof(GpuVertex32), buf32.vertex_buffer_bytes()));

    // Verify stride alignment across elements
    for (size_t i = 0; i < buf32.vertices.size(); ++i) {
        CHECK(reinterpret_cast<uintptr_t>(&buf32.vertices[i]) % 32 == 0);
    }

    // 5. Test 32-byte packed normal+tangent interleaved vertex buffer build
    auto buf_tan32 = GpuVertexBufferBuilder::build_packed_tangents_32(box_mesh);
    CHECK(buf_tan32.vertex_count() == 8);
    CHECK(buf_tan32.index_count() == 36);
    CHECK(buf_tan32.vertex_buffer_bytes() == 8 * 32);
    CHECK(buf_tan32.is_32byte_aligned());
    CHECK(GpuVertexBufferBuilder::verify_buffer_alignment(buf_tan32.vertices.data(), sizeof(GpuVertexTangent32), buf_tan32.vertex_buffer_bytes()));

    // 6. Test 64-byte full uncompressed interleaved vertex buffer build
    auto buf_full64 = GpuVertexBufferBuilder::build_full_64(box_mesh);
    CHECK(buf_full64.vertex_count() == 8);
    CHECK(buf_full64.index_count() == 36);
    CHECK(buf_full64.vertex_buffer_bytes() == 8 * 64);
    CHECK(buf_full64.is_32byte_aligned());
    CHECK(GpuVertexBufferBuilder::verify_buffer_alignment(buf_full64.vertices.data(), sizeof(GpuVertexFull64), buf_full64.vertex_buffer_bytes()));
}

TEST_CASE("PH4-02.03.005: Compute Shaders for Parallel Particles & Thermal Field Diffusion") {
    using namespace shunya::cad3d;

    // 1. Data layout & alignment guarantees
    CHECK(sizeof(ComputeParticle) == 32);
    CHECK(alignof(ComputeParticle) == 16);

    // 2. Compute Shader Compilation & Storage Buffer Reflection
    ParticleComputeSimulator particle_sim;
    REQUIRE(particle_sim.initialize(ShaderTargetApi::SpirV_Vulkan));
    CHECK(particle_sim.pipeline().is_ready());
    CHECK(particle_sim.pipeline().shader_module().valid);
    CHECK(particle_sim.pipeline().shader_module().stage == ShaderStage::Compute);

    ThermalDiffusionComputeSimulator thermal_sim;
    REQUIRE(thermal_sim.initialize(ShaderTargetApi::SpirV_Vulkan));
    CHECK(thermal_sim.pipeline().is_ready());
    CHECK(thermal_sim.pipeline().shader_module().valid);

    // 3. Parallel Particle Dynamics Simulation (1024 particles, 100 timesteps)
    const size_t particle_count = 1024;
    std::vector<ComputeParticle> particles_gpu(particle_count);
    std::vector<ComputeParticle> particles_cpu(particle_count);

    for (size_t i = 0; i < particle_count; ++i) {
        ComputeParticle p;
        p.pos[0] = static_cast<float>(i % 32) * 0.1f - 1.6f;
        p.pos[1] = 5.0f + static_cast<float>(i / 32) * 0.1f; // Dropping from height 5.0m
        p.pos[2] = 0.0f;
        p.vel[0] = 0.5f;
        p.vel[1] = 0.0f;
        p.vel[2] = 0.0f;
        p.mass = 1.0f;
        p.lifetime = 10.0f;

        particles_gpu[i] = p;
        particles_cpu[i] = p;
    }

    ParticleSimConfig p_cfg;
    p_cfg.dt = 0.016667f;
    p_cfg.gravity[1] = -9.81f;
    p_cfg.floor_y = 0.0f;
    p_cfg.restitution = 0.75f;
    p_cfg.drag_coefficient = 0.02f;

    // Simulate 60 frames (1 second of physics)
    for (int frame = 0; frame < 60; ++frame) {
        particle_sim.step_compute(particles_gpu, p_cfg);
        ParticleComputeSimulator::step_cpu_reference(particles_cpu, p_cfg);
    }

    // Verify particle physics metrics
    CHECK(particle_sim.pipeline().metrics().dispatch_count == 60);
    CHECK(particle_sim.pipeline().metrics().workgroups_dispatched == 60 * (particle_count / 256));
    CHECK(particle_sim.pipeline().metrics().threads_invoked == 60 * particle_count);

    // Parity verification: GPU compute matches CPU reference
    for (size_t i = 0; i < particle_count; ++i) {
        CHECK(std::abs(particles_gpu[i].pos[0] - particles_cpu[i].pos[0]) < 1e-4f);
        CHECK(std::abs(particles_gpu[i].pos[1] - particles_cpu[i].pos[1]) < 1e-4f);
        CHECK(std::abs(particles_gpu[i].pos[2] - particles_cpu[i].pos[2]) < 1e-4f);
        CHECK(std::abs(particles_gpu[i].vel[1] - particles_cpu[i].vel[1]) < 1e-4f);
        CHECK(particles_gpu[i].pos[1] >= p_cfg.floor_y); // Preserved floor collision
    }

    // 4. Large-Scale Thermal Field Diffusion (32x32 = 1024 cells)
    ThermalGridConfig2D t_cfg;
    t_cfg.width = 32;
    t_cfg.height = 32;
    t_cfg.dx = 0.005f; // 5 mm grid spacing
    t_cfg.dy = 0.005f;
    t_cfg.thermal_diffusivity = 1.11e-4f; // Copper
    t_cfg.dt = 0.01f;
    t_cfg.ambient_temp = 293.15f; // 20 C

    const size_t grid_cells = t_cfg.width * t_cfg.height;
    std::vector<float> grid_gpu(grid_cells, t_cfg.ambient_temp);
    std::vector<float> next_gpu(grid_cells, t_cfg.ambient_temp);
    std::vector<float> grid_cpu(grid_cells, t_cfg.ambient_temp);
    std::vector<float> next_cpu(grid_cells, t_cfg.ambient_temp);

    // Place 500 W heat source at center cell (16, 16)
    std::vector<float> heat_sources(grid_cells, 0.0f);
    const size_t center_idx = 16 * t_cfg.width + 16;
    heat_sources[center_idx] = 500.0f;

    // Simulate 30 diffusion steps
    for (int step = 0; step < 30; ++step) {
        thermal_sim.step_compute(grid_gpu, next_gpu, t_cfg, heat_sources);
        ThermalDiffusionComputeSimulator::step_cpu_reference(grid_cpu, next_cpu, t_cfg, heat_sources);
        grid_gpu = next_gpu;
        grid_cpu = next_cpu;
    }

    // Verify thermal diffusion metrics
    CHECK(thermal_sim.pipeline().metrics().dispatch_count == 30);
    CHECK(thermal_sim.pipeline().metrics().workgroups_dispatched == 30 * 4); // (32/16)*(32/16) = 4 groups per step

    // Parity verification & physical temperature propagation
    CHECK(grid_gpu[center_idx] > 300.0f); // Center heated up above ambient
    CHECK(grid_gpu[center_idx + 1] > t_cfg.ambient_temp); // Neighbor received diffused heat
    CHECK(grid_gpu[center_idx - 1] > t_cfg.ambient_temp);

    // Assert exact parity across all 1024 thermal cells
    for (size_t i = 0; i < grid_cells; ++i) {
        CHECK(std::abs(grid_gpu[i] - grid_cpu[i]) < 1e-4f);
    }
}

TEST_CASE("PH4-02.03.006: Shader Pipeline Caching & Mode Switch Hitch Elimination") {
    using namespace shunya::cad3d;

    // 1. Prewarm standard pipelines
    ShaderCompiler compiler;
    ShaderPipelineCache cache;
    cache.prewarm_standard_pipelines(compiler, ShaderTargetApi::SpirV_Vulkan);

    CHECK(cache.size() >= 7);
    CHECK(cache.metrics().prewarmed_count >= 7);

    // 2. Querying prewarmed modes guarantees immediate cache hits with 0 compile latency
    PipelineCacheKey opaque_key{};
    opaque_key.pso_desc = PipelineStateDesc{"opaque_pbr", true, true, RhiCompareOp::Less, RhiBlendMode::Opaque, RhiCullMode::Back};
    opaque_key.target_api = ShaderTargetApi::SpirV_Vulkan;
    opaque_key.vs_hash = 0x12345678ULL;
    opaque_key.fs_hash = 0x87654321ULL;

    const auto* opaque_blob = cache.find(opaque_key);
    REQUIRE(opaque_blob != nullptr);
    CHECK(opaque_blob->pipeline_name == "Opaque_PBR");

    PipelineCacheKey wboit_key{};
    wboit_key.pso_desc = PipelineStateDesc{"transparent_wboit", true, false, RhiCompareOp::Less, RhiBlendMode::AlphaBlend, RhiCullMode::None};
    wboit_key.target_api = ShaderTargetApi::SpirV_Vulkan;
    wboit_key.vs_hash = 0x12345678ULL;
    wboit_key.fs_hash = 0x87654321ULL;

    const auto* wboit_blob = cache.find(wboit_key);
    REQUIRE(wboit_blob != nullptr);
    CHECK(wboit_blob->pipeline_name == "Transparent_WBOIT");

    CHECK(cache.metrics().lookups == 2);
    CHECK(cache.metrics().hits == 2);
    CHECK(cache.metrics().misses == 0);
    CHECK(cache.metrics().hit_ratio() == 1.0);

    // 3. Binary serialization & persistent disk round-trip
    const auto exported = cache.export_blob();
    CHECK(!exported.empty());

    ShaderPipelineCache imported_cache;
    REQUIRE(imported_cache.import_blob(exported));
    CHECK(imported_cache.size() == cache.size());

    // Verify imported cache resolves keys accurately
    const auto* imported_opaque = imported_cache.find(opaque_key);
    REQUIRE(imported_opaque != nullptr);
    CHECK(imported_opaque->pipeline_name == "Opaque_PBR");

    // 4. Data corruption rejection test (flip bit in payload -> CRC32 failure)
    auto corrupted_bytes = exported;
    if (!corrupted_bytes.empty()) {
        corrupted_bytes.back() ^= 0xFF;
    }
    ShaderPipelineCache corrupt_test_cache;
    CHECK(!corrupt_test_cache.import_blob(corrupted_bytes));

    // 5. Integration with RhiRendererEngine
    RhiRendererEngine engine;
    REQUIRE(engine.initialize({RhiBackendApi::Vulkan, 320, 240, true}));
    CHECK(engine.pipeline_cache().size() >= 7);
}

TEST_CASE("PH4-02.04.001: In-App Real-Time Performance HUD Telemetry") {
    using namespace shunya::perf;

    // 1. Initialize Performance HUD model
    PerformanceHudConfig config;
    config.history_capacity = 60;
    config.target_fps = 60.0;
    config.low_fps_threshold = 30.0;
    config.ram_warning_mb = 800.0;
    config.vram_warning_mb = 1024.0;
    config.dock_position = HudDockPosition::TopRight;
    config.visible = true;

    PerformanceHudModel hud(config);
    CHECK(hud.is_visible());
    CHECK(hud.dock_position() == HudDockPosition::TopRight);

    // 2. Record 60 smooth frames (4ms CPU + 8ms GPU = 12ms frame time -> ~83.3 FPS)
    for (int i = 0; i < 60; ++i) {
        hud.record_frame(4.0, 8.0, 24, 15000, 2);
    }

    CHECK(hud.history().size() == 60);
    CHECK(hud.current_fps() > 80.0);
    CHECK(hud.average_fps() > 80.0);
    CHECK(hud.cpu_frame_time_ms() == 4.0);
    CHECK(hud.gpu_render_time_ms() == 8.0);
    CHECK(hud.draw_calls() == 24);
    CHECK(hud.triangles_submitted() == 15000);
    CHECK(hud.compute_dispatches() == 2);
    CHECK(!hud.is_low_fps());

    // 3. Low FPS trigger evaluation (e.g. heavy physics solve frame: 25ms CPU + 25ms GPU = 50ms -> 20 FPS)
    hud.record_frame(25.0, 25.0, 100, 80000, 10);
    CHECK(hud.current_fps() == 20.0);
    CHECK(hud.is_low_fps());

    // 4. Memory consumption & warning thresholds
    CHECK(hud.ram_usage_mb() > 0.0);
    CHECK(hud.vram_usage_mb() > 0.0);
    CHECK(!hud.is_ram_warning());
    CHECK(!hud.is_vram_warning());

    // Trigger RAM warning limit
    hud.record_frame(4.0, 8.0, 10, 5000, 0, 950.0, 500.0);
    CHECK(hud.is_ram_warning());
    CHECK(!hud.is_vram_warning());

    // Trigger VRAM warning limit
    hud.record_frame(4.0, 8.0, 10, 5000, 0, 400.0, 1200.0);
    CHECK(!hud.is_ram_warning());
    CHECK(hud.is_vram_warning());

    // 5. Formatted HUD Text strings for UI/QML overlay
    const std::string fps_text = hud.formatted_fps_text();
    CHECK(fps_text.find("FPS:") != std::string::npos);

    const std::string cpu_text = hud.formatted_cpu_time_text();
    CHECK(cpu_text.find("CPU:") != std::string::npos);

    const std::string gpu_text = hud.formatted_gpu_time_text();
    CHECK(gpu_text.find("GPU:") != std::string::npos);

    const std::string mem_text = hud.formatted_memory_text();
    CHECK(mem_text.find("RAM:") != std::string::npos);
    CHECK(mem_text.find("VRAM:") != std::string::npos);

    const std::string draw_text = hud.formatted_draw_calls_text();
    CHECK(draw_text.find("Draw Calls:") != std::string::npos);

    // 6. Controls: toggle visibility & dock positioning
    hud.toggle_visible();
    CHECK(!hud.is_visible());
    hud.set_visible(true);
    CHECK(hud.is_visible());

    hud.set_dock_position(HudDockPosition::BottomLeft);
    CHECK(hud.dock_position() == HudDockPosition::BottomLeft);
}

TEST_CASE("PH4-02.04.002: Microsecond Hardware Profiler for Simulation Loops") {
    using namespace shunya::perf;

    // 1. Hardware CPU Cycle counter verification
    const uint64_t c1 = SimulationHardwareProfiler::read_cpu_cycles();
    const uint64_t c2 = SimulationHardwareProfiler::read_cpu_cycles();
    CHECK(c2 >= c1);

    SimulationHardwareProfiler profiler;

    // 2. Instrument simulated 100-step Circuit Simulation Loop
    for (int step = 0; step < 100; ++step) {
        ScopedHardwareStageTimer total_timer(profiler, SimulationStage::Total_Step);

        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Circuit_MatrixAssembly);
            volatile double acc = 0.0;
            for (int k = 0; k < 200; ++k) acc += k * 0.001;
            (void)acc;
        }

        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Circuit_Factorization);
            volatile double acc = 1.0;
            for (int k = 1; k < 150; ++k) acc *= 1.0001;
            (void)acc;
        }

        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Circuit_TriangularSolve);
            volatile double acc = 5.0;
            for (int k = 0; k < 100; ++k) acc -= 0.01;
            (void)acc;
        }

        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Circuit_ConvergenceCheck);
            volatile double acc = 0.0001;
            (void)acc;
        }
    }

    CHECK(profiler.stage_metrics(SimulationStage::Circuit_MatrixAssembly).call_count == 100);
    CHECK(profiler.stage_metrics(SimulationStage::Circuit_Factorization).call_count == 100);
    CHECK(profiler.stage_metrics(SimulationStage::Circuit_TriangularSolve).call_count == 100);
    CHECK(profiler.stage_metrics(SimulationStage::Circuit_ConvergenceCheck).call_count == 100);

    CHECK(profiler.stage_avg_us(SimulationStage::Circuit_MatrixAssembly) > 0.0);
    CHECK(profiler.stage_avg_us(SimulationStage::Circuit_Factorization) > 0.0);
    CHECK(profiler.stage_last_us(SimulationStage::Circuit_TriangularSolve) > 0.0);
    CHECK(profiler.stage_metrics(SimulationStage::Circuit_MatrixAssembly).total_cpu_cycles > 0);

    // 3. Instrument simulated 100-step 3D Physics Simulation Loop
    for (int step = 0; step < 100; ++step) {
        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Physics_BroadPhase);
            volatile double acc = 0.0;
            for (int k = 0; k < 100; ++k) acc += k;
            (void)acc;
        }

        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Physics_NarrowPhase);
            volatile double acc = 0.0;
            for (int k = 0; k < 150; ++k) acc += k * 0.5;
            (void)acc;
        }

        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Physics_ConstraintSolver);
            volatile double acc = 100.0;
            for (int k = 0; k < 250; ++k) acc -= 0.1;
            (void)acc;
        }

        {
            ScopedHardwareStageTimer stage_timer(profiler, SimulationStage::Physics_PositionIntegration);
            volatile double acc = 0.0;
            for (int k = 0; k < 80; ++k) acc += 1.0;
            (void)acc;
        }
    }

    CHECK(profiler.stage_metrics(SimulationStage::Physics_BroadPhase).call_count == 100);
    CHECK(profiler.stage_metrics(SimulationStage::Physics_NarrowPhase).call_count == 100);
    CHECK(profiler.stage_metrics(SimulationStage::Physics_ConstraintSolver).call_count == 100);
    CHECK(profiler.stage_metrics(SimulationStage::Physics_PositionIntegration).call_count == 100);

    const auto& pgs_metrics = profiler.stage_metrics(SimulationStage::Physics_ConstraintSolver);
    CHECK(pgs_metrics.min_time_us <= pgs_metrics.max_time_us);
    CHECK(pgs_metrics.ema_time_us > 0.0);

    // 4. Summary report generation
    const std::string summary = profiler.format_profile_summary();
    CHECK(summary.find("Circuit: Matrix Assembly") != std::string::npos);
    CHECK(summary.find("Physics: Constraint PGS Solve") != std::string::npos);
    CHECK(summary.find("Overall Simulation Step") != std::string::npos);
}

TEST_CASE("PH4-02.04.003: Automated Performance Regression Test Suite (>10% Threshold Gating)") {
    using namespace shunya::bench;

    PerformanceRegressionTestSuite suite(0.10); // 10% threshold limit
    CHECK(std::abs(suite.threshold() - 0.10) < 1e-6);

    // Register baselines for frame time and circuit solve time
    suite.register_baseline("cad_render_frame_time_ms", {16.0, 16.2, 15.9, 16.1, 16.0, 15.8, 16.2, 16.0});
    suite.register_baseline("circuit_dc_solve_time_ms", {0.50, 0.49, 0.51, 0.50, 0.52, 0.48, 0.50});
    suite.register_baseline("physics_step_time_ms", {1.20, 1.22, 1.19, 1.21, 1.20, 1.18, 1.23});

    // 1. Candidate within allowable variation (< 5% difference)
    suite.record_candidate("cad_render_frame_time_ms", {16.2, 16.3, 16.1, 16.2, 16.0, 16.1});
    suite.record_candidate("circuit_dc_solve_time_ms", {0.51, 0.50, 0.52, 0.51, 0.50});
    suite.record_candidate("physics_step_time_ms", {1.22, 1.21, 1.23, 1.20, 1.22});

    RegressionSuiteResult res_ok = suite.run_evaluation();
    CHECK(res_ok.passed);
    CHECK(res_ok.regressed_count == 0);
    CHECK(res_ok.failure_reasons.empty());
    CHECK(suite.check_and_gate_build(/*throw_on_failure=*/false));

    // 2. Candidate with severe regression on circuit_dc_solve_time_ms (> 25% slowdown)
    suite.record_candidate("circuit_dc_solve_time_ms", {0.68, 0.70, 0.69, 0.71, 0.67, 0.72});

    RegressionSuiteResult res_fail = suite.run_evaluation();
    CHECK_FALSE(res_fail.passed);
    CHECK(res_fail.regressed_count == 1);
    CHECK(res_fail.failure_reasons.size() == 1);
    CHECK(res_fail.failure_reasons[0].find("circuit_dc_solve_time_ms") != std::string::npos);
    CHECK_FALSE(suite.check_and_gate_build(/*throw_on_failure=*/false));

    bool threw = false;
    try {
        (void)suite.check_and_gate_build(/*throw_on_failure=*/true);
    } catch (const std::runtime_error& ex) {
        threw = true;
        CHECK(std::string(ex.what()).find("circuit_dc_solve_time_ms") != std::string::npos);
    }
    CHECK(threw);

    // 3. Serialization and Deserialization of Baselines JSON
    std::string json_baselines = suite.serialize_baselines_json();
    CHECK(json_baselines.find("\"cad_render_frame_time_ms\"") != std::string::npos);
    CHECK(json_baselines.find("\"circuit_dc_solve_time_ms\"") != std::string::npos);

    PerformanceRegressionTestSuite restored_suite(0.10);
    CHECK(restored_suite.deserialize_baselines_json(json_baselines));

    restored_suite.record_candidate("cad_render_frame_time_ms", {16.1, 16.0, 16.2});
    auto restored_eval = restored_suite.run_evaluation();
    CHECK(restored_eval.passed);
}

TEST_CASE("PH4-02.04.004: Frame-by-Frame Timeline Capture to Chrome Tracing / Perfetto JSON") {
    using namespace shunya::perf;

    ChromeTracingExporter exporter;
    CHECK(exporter.is_capturing());
    CHECK(exporter.event_count() == 0);

    // 1. Record complete events using explicit recording and ScopedTraceEvent
    exporter.record_complete_event("RenderFrame", "cad3d", 1000, 16000, 1, {{"fps", 62.5}, {"draw_calls", 14.0}});

    {
        ScopedTraceEvent scoped(exporter, "CircuitSolve_MNA", "circuit", 2, {{"nodes", 48.0}, {"iterations", 3.0}});
        volatile int dummy = 0;
        for (int i = 0; i < 1000; ++i) dummy += i;
        (void)dummy;
    }

    {
        ScopedTraceEvent scoped(exporter, "PhysicsStep_PGS", "physics", 3, {{"bodies", 12.0}});
        volatile double acc = 0.0;
        for (int i = 0; i < 500; ++i) acc += i * 0.1;
        (void)acc;
    }

    // 2. Record instant markers and memory/vram counters
    exporter.record_instant("VSyncSignal", "display", 17000, 1);
    exporter.record_counter("RAM_Usage_MB", "memory", "allocated_mb", 185.4, 17050);
    exporter.record_counter("VRAM_Usage_MB", "memory", "vram_mb", 240.2, 17050);

    CHECK(exporter.event_count() == 6);

    // 3. Export to Chrome Tracing / Perfetto JSON format
    const std::string json_trace = exporter.export_json();
    CHECK(json_trace.find("\"traceEvents\": [") != std::string::npos);
    CHECK(json_trace.find("\"name\": \"RenderFrame\"") != std::string::npos);
    CHECK(json_trace.find("\"cat\": \"cad3d\"") != std::string::npos);
    CHECK(json_trace.find("\"ph\": \"X\"") != std::string::npos);
    CHECK(json_trace.find("\"dur\": 16000") != std::string::npos);
    CHECK(json_trace.find("\"name\": \"CircuitSolve_MNA\"") != std::string::npos);
    CHECK(json_trace.find("\"name\": \"PhysicsStep_PGS\"") != std::string::npos);
    CHECK(json_trace.find("\"name\": \"VSyncSignal\"") != std::string::npos);
    CHECK(json_trace.find("\"ph\": \"I\"") != std::string::npos);
    CHECK(json_trace.find("\"name\": \"RAM_Usage_MB\"") != std::string::npos);
    CHECK(json_trace.find("\"ph\": \"C\"") != std::string::npos);
    CHECK(json_trace.find("\"allocated_mb\": 185.4") != std::string::npos);

    // 4. File export test
    const std::string test_file = "build/test_chrome_trace.json";
    CHECK(exporter.export_to_file(test_file));
    std::ifstream in(test_file);
    CHECK(in.is_open());
    std::string file_content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(file_content == json_trace);
    in.close();
    std::filesystem::remove(test_file);

    // 5. Start/stop capture verification
    exporter.stop_capture();
    CHECK_FALSE(exporter.is_capturing());
    exporter.record_instant("IgnoredEvent", "test");
    CHECK(exporter.event_count() == 6);

    exporter.clear();
    CHECK(exporter.event_count() == 0);
}

TEST_CASE("PH4-02.04.005: Hot Loop Optimization & Sub-Millisecond Material/Circuit Solves") {
    using namespace shunya::perf;

    double mixing_mean_us = 0.0;
    double circuit_mean_us = 0.0;

    const bool passes_guarantees = HotLoopProfiler::verify_sub_millisecond_guarantees(
        &mixing_mean_us, &circuit_mean_us);

    CHECK(passes_guarantees);
    CHECK(mixing_mean_us < 1000.0); // Strict sub-millisecond requirement (< 1000 us)
    CHECK(circuit_mean_us < 1000.0); // Strict sub-millisecond requirement (< 1000 us)
    CHECK(mixing_mean_us > 0.0);
    CHECK(circuit_mean_us > 0.0);
}

TEST_CASE("PH4-02.04.006: Memory Footprint Limits (Idle < 250 MB, 100k Scene < 800 MB)") {
    using namespace shunya::perf;

    MemoryFootprintEnforcer enforcer;
    CHECK(enforcer.limits().base_studio_idle_limit_mb == 250.0);
    CHECK(enforcer.limits().complex_scene_100k_limit_mb == 800.0);

    // 1. Validate idle memory footprint
    const double current_rss_mb = MemoryFootprintEnforcer::query_process_rss_mb();
    CHECK(current_rss_mb > 0.0);
    CHECK(enforcer.validate_idle_budget(current_rss_mb)); // Must be < 250 MB

    // 2. Evaluate current live process metrics
    auto metrics = enforcer.evaluate_current_footprint(0);
    CHECK(metrics.is_within_idle_budget);
    CHECK(metrics.is_within_scene_budget);
    CHECK(metrics.diagnostic_summary.find("PASSED") != std::string::npos);

    // 3. Analytical estimation for a 100,000-vertex, 200,000-triangle CAD 3D scene
    constexpr size_t kVertices = 100000;
    constexpr size_t kTriangles = 200000;
    const double estimated_100k_scene_mb = enforcer.estimate_scene_footprint_mb(kVertices, kTriangles, 4);

    // Ensure estimated 100k-vertex footprint is well within the 800 MB budget
    CHECK(estimated_100k_scene_mb < 800.0);
    CHECK(enforcer.validate_scene_budget(kVertices, estimated_100k_scene_mb));

    // 4. Boundary checks on oversized / regressed memory allocations
    CHECK_FALSE(enforcer.validate_idle_budget(251.0));
    CHECK_FALSE(enforcer.validate_scene_budget(kVertices, 805.0));
}

TEST_CASE("PH4-03.01.001: Modular Material Formation Scenario Dispatcher") {
    using namespace shunya::mat;

    // 1. Verify all 6 formation scenarios are registered and available
    const auto scenarios = FormationScenarioDispatcher::available_scenarios();
    CHECK(scenarios.size() == 6);

    for (auto sc : scenarios) {
        CHECK(FormationScenarioDispatcher::is_scenario_supported(sc));
        const auto& handler = FormationScenarioDispatcher::get_handler(sc);
        CHECK(handler.scenario() == sc);
        CHECK_FALSE(handler.name().empty());
        CHECK_FALSE(handler.description().empty());
    }

    // 2. Validate Precondition enforcement: Rejection of noble gas in Alloy and Chemical Compound
    MixtureExperiment noble_exp;
    noble_exp.name = "Argon Brass Test";
    noble_exp.scenario = FormationScenario::Alloy;
    noble_exp.inputs.push_back({"Cu", QuantityInputBasis::MassFractionPct, 70.0, 1e-3, false});
    noble_exp.inputs.push_back({"Ar", QuantityInputBasis::MassFractionPct, 30.0, 1e-3, false});

    auto alloy_eval = MaterialMixingLab::evaluate(noble_exp);
    REQUIRE(alloy_eval.is_ok());
    CHECK_FALSE(alloy_eval.value().is_chemically_valid);
    CHECK_FALSE(alloy_eval.value().validation_errors.empty());
    bool found_alloy_noble = false;
    for (const auto& err : alloy_eval.value().validation_errors) {
        if (err.find("noble gas") != std::string::npos) found_alloy_noble = true;
    }
    CHECK(found_alloy_noble);

    // 3. Chemical Compound Precondition: Rejection of noble gas
    noble_exp.scenario = FormationScenario::ChemicalCompound;
    auto compound_eval = MaterialMixingLab::evaluate(noble_exp);
    REQUIRE(compound_eval.is_ok());
    CHECK_FALSE(compound_eval.value().is_chemically_valid);
    CHECK_FALSE(compound_eval.value().validation_errors.empty());
    bool found_compound_noble = false;
    for (const auto& err : compound_eval.value().validation_errors) {
        if (err.find("noble gas") != std::string::npos || err.find("noble gases") != std::string::npos) {
            found_compound_noble = true;
        }
    }
    CHECK(found_compound_noble);

    // 4. Valid Alloy evaluation dispatches Hume-Rothery notes
    MixtureExperiment valid_brass;
    valid_brass.name = "Valid Cu-Zn Brass";
    valid_brass.scenario = FormationScenario::Alloy;
    valid_brass.inputs.push_back({"Cu", QuantityInputBasis::MassFractionPct, 70.0, 1e-3, false});
    valid_brass.inputs.push_back({"Zn", QuantityInputBasis::MassFractionPct, 30.0, 1e-3, false});

    auto brass_res = MaterialMixingLab::evaluate(valid_brass);
    REQUIRE(brass_res.is_ok());
    bool found_hume_rothery = false;
    for (const auto& note : brass_res.value().scientific_notes) {
        if (note.find("Hume-Rothery") != std::string::npos) found_hume_rothery = true;
    }
    CHECK(found_hume_rothery);

    // 5. Valid Chemical Compound evaluation dispatches electronegativity delta notes
    MixtureExperiment nacl;
    nacl.name = "Sodium Chloride";
    nacl.scenario = FormationScenario::ChemicalCompound;
    nacl.inputs.push_back({"Na", QuantityInputBasis::MoleCount, 1.0, 1e-3, false});
    nacl.inputs.push_back({"Cl", QuantityInputBasis::MoleCount, 1.0, 1e-3, false});

    auto nacl_res = MaterialMixingLab::evaluate(nacl);
    REQUIRE(nacl_res.is_ok());
    bool found_en_delta = false;
    for (const auto& note : nacl_res.value().scientific_notes) {
        if (note.find("Electronegativity delta") != std::string::npos) found_en_delta = true;
    }
    CHECK(found_en_delta);
}

TEST_CASE("PH4-03.01.002: Chemical Compound Stoichiometric Balance & Pauling Electronegativity") {
    using namespace shunya::mat;
    using namespace shunya::matdb;

    // 1. Empirical Formula Synthesis
    NormalizedConstituent h_const;
    h_const.symbol = "H";
    h_const.atomic_number = 1;
    h_const.moles = 2.0;
    h_const.mole_fraction = 2.0 / 3.0;

    NormalizedConstituent o_const;
    o_const.symbol = "O";
    o_const.atomic_number = 8;
    o_const.moles = 1.0;
    o_const.mole_fraction = 1.0 / 3.0;

    std::vector<NormalizedConstituent> h2o_constituents = {h_const, o_const};
    CHECK(CompoundValidator::synthesize_empirical_formula(h2o_constituents) == "H2O");

    NormalizedConstituent na_const;
    na_const.symbol = "Na";
    na_const.atomic_number = 11;
    na_const.moles = 1.0;
    na_const.mole_fraction = 0.5;

    NormalizedConstituent cl_const;
    cl_const.symbol = "Cl";
    cl_const.atomic_number = 17;
    cl_const.moles = 1.0;
    cl_const.mole_fraction = 0.5;

    std::vector<NormalizedConstituent> nacl_constituents = {na_const, cl_const};
    CHECK(CompoundValidator::synthesize_empirical_formula(nacl_constituents) == "NaCl");

    // 2. Pauling Ionic Character Equation: P = (1 - e^(-0.25 * (Delta Chi)^2)) * 100%
    // NaCl: Na = 0.93, Cl = 3.16 -> Delta Chi = 2.23
    const double delta_chi_nacl = 2.23;
    const double ionic_nacl = CompoundValidator::compute_pauling_ionic_pct(delta_chi_nacl);
    // 1 - exp(-0.25 * 2.23^2) = 1 - exp(-1.243225) = 1 - 0.28845 = 0.7115 -> ~71.15%
    CHECK(ionic_nacl > 70.0);
    CHECK(ionic_nacl < 72.0);

    // Covalent H2: Delta Chi = 0.0 -> 0% ionic
    CHECK(CompoundValidator::compute_pauling_ionic_pct(0.0) == 0.0);

    // 3. Validation with Element Database
    const auto& db = ElementDatabase::global();
    const auto* na_rec = db.find_by_symbol("Na");
    const auto* cl_rec = db.find_by_symbol("Cl");
    REQUIRE(na_rec != nullptr);
    REQUIRE(cl_rec != nullptr);

    std::vector<const ElementRecord*> nacl_recs = {na_rec, cl_rec};
    auto val_res = CompoundValidator::validate_compound(nacl_constituents, nacl_recs);
    CHECK(val_res.is_stoichiometrically_balanced);
    CHECK(val_res.is_valence_saturated);
    CHECK(val_res.bond_character == BondCharacter::Ionic);
    CHECK(val_res.pauling_electronegativity_delta > 2.0);
    CHECK(val_res.empirical_formula == "NaCl");

    // 4. Reject Noble Gas in valence saturation
    const auto* he_rec = db.find_by_symbol("He");
    REQUIRE(he_rec != nullptr);
    NormalizedConstituent he_const;
    he_const.symbol = "He";
    he_const.atomic_number = 2;
    he_const.moles = 1.0;
    he_const.mole_fraction = 0.5;

    std::vector<NormalizedConstituent> fake_he_constituents = {he_const, cl_const};
    std::vector<const ElementRecord*> fake_recs = {he_rec, cl_rec};
    auto fake_res = CompoundValidator::validate_compound(fake_he_constituents, fake_recs);
    CHECK_FALSE(fake_res.is_stoichiometrically_balanced);
    CHECK_FALSE(fake_res.is_valence_saturated);
}

TEST_CASE("PH4-03.01.003: Thermodynamic Gibbs Free Energy & Spontaneity") {
    using namespace shunya::mat;

    // 1. Ideal Molar Entropy of Mixing: Delta S_mix = -R * sum(x_i * ln(x_i))
    // For equimolar binary mixture (x1 = 0.5, x2 = 0.5):
    // Delta S = R * ln(2) = 8.3144626 * 0.69314718 = 5.7631 J/(mol*K)
    const std::vector<double> binary_equimolar = {0.5, 0.5};
    const double delta_s = GibbsFreeEnergyEngine::compute_entropy_of_mixing(binary_equimolar);
    CHECK(std::abs(delta_s - 5.7631) < 0.01);

    // 2. Regular Solution Enthalpy of Mixing: Delta H_mix = Omega * x1 * x2
    const double omega_positive = 10000.0; // J/mol (endothermic unmixing tendency)
    const double delta_h = GibbsFreeEnergyEngine::compute_enthalpy_of_mixing(binary_equimolar, omega_positive);
    CHECK(std::abs(delta_h - 2500.0) < 0.01);

    // 3. Gibbs Free Energy Minimization at low temperature T = 300 K:
    // Delta G = Delta H - T * Delta S = 2500 - 300 * 5.7631 = 2500 - 1728.93 = +771.07 J/mol (> 0)
    // Non-spontaneous (Endergonic), miscibility gap expected because T < Tc (Tc = 10000 / (2*R) = 601.36 K)
    auto thermo_low_t = GibbsFreeEnergyEngine::evaluate_mixing_thermodynamics(binary_equimolar, 300.0, omega_positive);
    CHECK_FALSE(thermo_low_t.is_spontaneous);
    CHECK(thermo_low_t.delta_g_j_mol > 0.0);
    CHECK(thermo_low_t.has_miscibility_gap);
    CHECK(thermo_low_t.critical_temperature_k > 600.0);
    CHECK(thermo_low_t.thermodynamic_regime == "Non-Spontaneous (Endergonic)");

    // 4. Elevated temperature T = 1000 K (entropy dominates -T * Delta S = -5763.1 J/mol):
    // Delta G = 2500 - 5763.1 = -3263.1 J/mol (< 0) -> Spontaneous single-phase solution
    auto thermo_high_t = GibbsFreeEnergyEngine::evaluate_mixing_thermodynamics(binary_equimolar, 1000.0, omega_positive);
    CHECK(thermo_high_t.is_spontaneous);
    CHECK(thermo_high_t.delta_g_j_mol < 0.0);
    CHECK_FALSE(thermo_high_t.has_miscibility_gap);
    CHECK(thermo_high_t.thermodynamic_regime == "Spontaneous (Exergonic)");

    // 5. Exothermic Chemical Compound Formation (Omega < 0, e.g. -40000 J/mol):
    // Strongly negative Delta H, spontaneous at all practical temperatures
    const double omega_exothermic = -40000.0;
    auto thermo_compound = GibbsFreeEnergyEngine::evaluate_mixing_thermodynamics(binary_equimolar, 298.15, omega_exothermic);
    CHECK(thermo_compound.is_spontaneous);
    CHECK(thermo_compound.delta_h_j_mol == -10000.0);
    CHECK(thermo_compound.delta_g_j_mol < -11000.0);
    CHECK_FALSE(thermo_compound.has_miscibility_gap);
}

TEST_CASE("PH4-03.01.004: Extended Hume-Rothery Rules for Alloy Formation") {
    using namespace shunya::mat;
    using namespace shunya::matdb;

    const auto& db = ElementDatabase::global();
    const auto* cu = db.find_by_symbol("Cu");
    const auto* ni = db.find_by_symbol("Ni");
    const auto* al = db.find_by_symbol("Al");
    const auto* na = db.find_by_symbol("Na");
    const auto* pb = db.find_by_symbol("Pb");

    REQUIRE(cu != nullptr);
    REQUIRE(ni != nullptr);
    REQUIRE(al != nullptr);
    REQUIRE(na != nullptr);
    REQUIRE(pb != nullptr);

    // 1. Classic Cu-Ni Complete Isomorphous Solid Solution (Cupronickel/Monel)
    // Both FCC, size difference ~2.7% (< 15%), electronegativities nearly identical (Delta chi = 0.01), valences 1 & 2
    auto cuni_eval = HumeRotheryEngine::evaluate_pair(*cu, *ni);
    CHECK(cuni_eval.size_factor_passed);
    CHECK(cuni_eval.atomic_radius_mismatch_pct < 10.0);
    CHECK(cuni_eval.crystal_structure_matched);
    CHECK(cuni_eval.solvent_crystal_structure == "FCC");
    CHECK(cuni_eval.solute_crystal_structure == "FCC");
    CHECK(cuni_eval.valence_factor_favorable);
    CHECK(cuni_eval.electrochemical_factor_passed);
    CHECK_FALSE(cuni_eval.intermetallic_tendency);
    CHECK(cuni_eval.overall_solid_solubility_favored);
    CHECK(cuni_eval.rule_evaluations.size() == 4);

    // 2. Cu-Al (Aluminum Bronze)
    // Both FCC, but valence difference (Al=3 vs Cu=1 -> Delta V = 2) and radius mismatch ~18.6% (> 15%)
    auto cual_eval = HumeRotheryEngine::evaluate_pair(*cu, *al);
    CHECK(cual_eval.crystal_structure_matched); // both FCC
    CHECK_FALSE(cual_eval.valence_factor_favorable); // Delta V = 2
    CHECK_FALSE(cual_eval.size_factor_passed); // 18.6% mismatch
    CHECK_FALSE(cual_eval.overall_solid_solubility_favored);

    // 3. Cu-Na (Immense radius mismatch ~31% > 15%, crystal mismatch BCC vs FCC)
    auto cuna_eval = HumeRotheryEngine::evaluate_pair(*cu, *na);
    CHECK_FALSE(cuna_eval.size_factor_passed);
    CHECK(cuna_eval.atomic_radius_mismatch_pct > 15.0);
    CHECK_FALSE(cuna_eval.overall_solid_solubility_favored);

    // 4. Cu-Pb (Leaded bronze: Delta Chi = 0.43 > 0.4, valence difference = 2)
    auto cupb_eval = HumeRotheryEngine::evaluate_pair(*cu, *pb);
    CHECK(cupb_eval.intermetallic_tendency);
    CHECK_FALSE(cupb_eval.overall_solid_solubility_favored);

    // 5. Batch alloy evaluation
    auto alloy_evals = HumeRotheryEngine::evaluate_alloy({cu, ni, al});
    CHECK(alloy_evals.size() == 2);
    CHECK(alloy_evals[0].overall_solid_solubility_favored); // Cu-Ni
    CHECK_FALSE(alloy_evals[1].overall_solid_solubility_favored); // Cu-Al
}

TEST_CASE("PH4-03.01.005: Solution Solubility Limits & Electrolytic Dissociation") {
    using namespace shunya::mat;

    SolutionThermodynamicModel nacl_model;
    nacl_model.base_solubility_g_per_100g = 36.0;
    nacl_model.delta_h_dissolution_j_mol = 3880.0;
    nacl_model.reference_temp_k = 293.15;
    nacl_model.electrolyte_type = ElectrolyteType::StrongElectrolyte;
    nacl_model.ion_count_per_formula = 2;
    nacl_model.limiting_molar_conductivity_s_m2_mol = 0.0126;

    // 1. Temperature-dependent solubility curve S(T)
    const double s_ref = SolutionSolubilityEngine::compute_solubility_limit(nacl_model, 293.15);
    CHECK(std::abs(s_ref - 36.0) < 1e-4);

    const double s_hot = SolutionSolubilityEngine::compute_solubility_limit(nacl_model, 350.0);
    CHECK(s_hot > s_ref); // Endothermic dissolution increases solubility with temperature

    // 2. Electrolytic Dissociation Comparison
    // Strong electrolyte: complete dissociation alpha = 1.0, van 't Hoff i = 2.0
    auto [alpha_strong, i_strong] = SolutionSolubilityEngine::compute_electrolytic_dissociation(
        ElectrolyteType::StrongElectrolyte, 2, 0.1, 298.15);
    CHECK(alpha_strong == 1.0);
    CHECK(i_strong == 2.0);

    // Non-electrolyte: alpha = 0.0, i = 1.0
    auto [alpha_non, i_non] = SolutionSolubilityEngine::compute_electrolytic_dissociation(
        ElectrolyteType::NonElectrolyte, 1, 0.1, 298.15);
    CHECK(alpha_non == 0.0);
    CHECK(i_non == 1.0);

    // Weak electrolyte: Ostwald dilution alpha << 1.0, 1.0 < i < 1.1
    auto [alpha_weak, i_weak] = SolutionSolubilityEngine::compute_electrolytic_dissociation(
        ElectrolyteType::WeakElectrolyte, 2, 0.1, 298.15);
    CHECK(alpha_weak > 0.0);
    CHECK(alpha_weak < 0.05);
    CHECK(i_weak > 1.0);
    CHECK(i_weak < 1.05);

    // 3. Full Solution Saturation and Precipitation
    // Unsaturated scenario: 20g NaCl in 100g H2O (below 36.0g limit)
    auto unsat_res = SolutionSolubilityEngine::evaluate_solution(nacl_model, 20.0, 100.0, 293.15);
    CHECK(unsat_res.state == SaturationState::Unsaturated);
    CHECK(unsat_res.dissolved_solute_mass_g == 20.0);
    CHECK(unsat_res.precipitated_mass_g == 0.0);
    CHECK(unsat_res.vant_hoff_factor == 2.0);
    CHECK(unsat_res.ionic_conductivity_s_m > 0.0);

    // Supersaturated scenario: 50g NaCl in 100g H2O (exceeds 36.0g limit by 14g)
    auto supersat_res = SolutionSolubilityEngine::evaluate_solution(nacl_model, 50.0, 100.0, 293.15);
    CHECK(supersat_res.state == SaturationState::Supersaturated);
    CHECK(std::abs(supersat_res.dissolved_solute_mass_g - 36.0) < 1e-4);
    CHECK(std::abs(supersat_res.precipitated_mass_g - 14.0) < 1e-4);
}

TEST_CASE("PH4-03.01.006: Multi-Axial Layup CLT & Orientation Tensors") {
    using namespace shunya::mat;

    // 1. Symmetric & Balanced Cross-Ply Laminate [0 / 90 / 90 / 0]
    std::vector<LaminatePly> cross_ply = {
        {"Carbon-Prepreg", 0.0, 0.000125, 140.0, 10.0, 0.30, 5.0},
        {"Carbon-Prepreg", 90.0, 0.000125, 140.0, 10.0, 0.30, 5.0},
        {"Carbon-Prepreg", 90.0, 0.000125, 140.0, 10.0, 0.30, 5.0},
        {"Carbon-Prepreg", 0.0, 0.000125, 140.0, 10.0, 0.30, 5.0}
    };

    auto res = DirectionalLayupEngine::evaluate_laminate(cross_ply);
    CHECK(std::abs(res.total_thickness_m - 0.0005) < 1e-7); // 0.5 mm
    CHECK(res.is_symmetric);
    CHECK(res.is_balanced);

    // Coupling matrix B must be strictly zero for symmetric layups
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            CHECK(res.b_matrix_gpa_m2[r][c] == 0.0);
        }
    }

    // Effective cross-ply moduli must be symmetric in x and y
    CHECK(std::abs(res.effective_ex_gpa - res.effective_ey_gpa) < 1.0);
    CHECK(res.effective_ex_gpa > 70.0);
    CHECK(res.effective_ex_gpa < 80.0);

    // 2. Advani-Tucker Orientation Tensor
    const auto a = DirectionalLayupEngine::compute_second_order_orientation_tensor(cross_ply);
    // Tr(a) = a_xx + a_yy = 0.5 + 0.5 = 1.0
    CHECK(std::abs(a[0][0] - 0.5) < 1e-6);
    CHECK(std::abs(a[1][1] - 0.5) < 1e-6);
    CHECK(std::abs(a[0][1]) < 1e-6);
    CHECK(std::abs(a[0][0] + a[1][1] - 1.0) < 1e-6);

    // 3. Unsymmetric Angle-Ply Laminate [0 / 45]
    std::vector<LaminatePly> unsymmetric = {
        {"Carbon-Prepreg", 0.0, 0.000125, 140.0, 10.0, 0.30, 5.0},
        {"Carbon-Prepreg", 45.0, 0.000125, 140.0, 10.0, 0.30, 5.0}
    };
    auto unsym_res = DirectionalLayupEngine::evaluate_laminate(unsymmetric);
    CHECK_FALSE(unsym_res.is_symmetric);
    CHECK(unsym_res.orientation_tensor[0][1] > 0.0); // Non-zero off-diagonal shear alignment
}
