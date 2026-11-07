#include "tests/test_framework.hpp"
#include "shunya/core/domain/rendering.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// REN-01: Renderer Abstraction & Backend Strategy
// =============================================================================

TEST_CASE("REN-01: Renderer Abstraction and Null Backend Lifecycle") {
    CHECK(backend_type_to_string(RendererBackendType::Null) == "Null");
    CHECK(backend_type_to_string(RendererBackendType::HeadlessSoftware) == "HeadlessSoftware");
    CHECK(backend_type_to_string(RendererBackendType::OpenGL) == "OpenGL");
    CHECK(backend_type_to_string(RendererBackendType::Vulkan) == "Vulkan");
    CHECK(backend_type_to_string(RendererBackendType::Direct3D12) == "Direct3D12");
    CHECK(backend_type_to_string(RendererBackendType::Metal) == "Metal");

    CHECK(backend_type_from_string("OpenGL").value() == RendererBackendType::OpenGL);
    CHECK(backend_type_from_string("Vulkan").value() == RendererBackendType::Vulkan);
    CHECK_FALSE(backend_type_from_string("NonExistentAPI").has_value());

    NullRendererBackend null_backend;
    CHECK(null_backend.backend_type() == RendererBackendType::Null);
    CHECK_FALSE(null_backend.is_initialized());

    RenderBackendConfig cfg;
    cfg.preferred_backend = RendererBackendType::Null;
    auto init_res = null_backend.initialize(cfg);
    REQUIRE(init_res.is_ok());
    CHECK(null_backend.is_initialized());

    CHECK(null_backend.begin_frame().is_ok());
    SceneSubmissionQueue queue;
    RenderInstance inst;
    inst.instance_id = "test_inst_1";
    queue.submit(inst);

    RenderPassDescriptor pass;
    CHECK(null_backend.execute_pass(pass, queue).is_ok());
    CHECK(null_backend.end_frame().is_ok());

    auto stats = null_backend.current_frame_stats();
    CHECK(stats.draw_calls == 1u);
    CHECK(stats.instances_rendered == 1u);

    auto caps = null_backend.capabilities();
    CHECK_FALSE(caps.is_hardware_accelerated);

    null_backend.shutdown();
    CHECK_FALSE(null_backend.is_initialized());

    // Test RenderEngine wrapper
    RenderEngine engine;
    CHECK_FALSE(engine.is_initialized());
    REQUIRE(engine.initialize(cfg).is_ok());
    CHECK(engine.is_initialized());
    CHECK(engine.begin_frame().is_ok());
    CHECK(engine.render_pass(pass, queue).is_ok());
    CHECK(engine.end_frame().is_ok());
    engine.shutdown();
    CHECK_FALSE(engine.is_initialized());
}

// =============================================================================
// REN-02: Viewport Camera, Scene Submission, Lighting & Materials
// =============================================================================

TEST_CASE("REN-02: Viewport Camera, Frustum, Lighting and Scene Submission") {
    ViewportCamera camera;
    camera.look_at({0.0, 0.0, 10.0}, {0.0, 0.0, 0.0});
    camera.fov_degrees = 90.0;
    camera.aspect_ratio = 1.0;
    camera.near_plane = 1.0;
    camera.far_plane = 100.0;

    auto fwd = camera.forward();
    CHECK(std::abs(fwd.z - (-1.0)) < 1e-4);

    auto r = camera.right();
    CHECK(std::abs(r.x - 1.0) < 1e-4);

    auto view = camera.view_matrix();
    CHECK(std::abs(view.m[11] - (-10.0)) < 1e-4);

    auto proj = camera.projection_matrix();
    CHECK(proj.m[0] > 0.0);

    // Frustum culling check
    Frustum frustum = camera.extract_frustum();
    // Sphere at origin (inside view)
    CHECK(frustum.intersects_sphere({0.0, 0.0, 0.0}, 2.0));
    // Sphere far behind camera
    CHECK_FALSE(frustum.intersects_sphere({0.0, 0.0, 20.0}, 1.0));

    // AABB tests
    CHECK(frustum.intersects_aabb({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0}));
    CHECK_FALSE(frustum.intersects_aabb({-1.0, -1.0, 20.0}, {1.0, 1.0, 25.0}));

    // Camera navigation helpers
    camera.pan(1.0, 2.0);
    CHECK(camera.position.x > 0.0);
    camera.zoom(1.0);
    CHECK(camera.position.z < 10.0);

    // Lighting environment
    LightingEnvironment env;
    LightSource sun{.id = "sun", .type = LightType::Directional, .intensity = 2.0};
    env.add_light(sun);
    CHECK(env.lights.size() == 1u);
    CHECK(env.find_light("sun") != nullptr);
    CHECK(env.find_light("sun")->intensity == 2.0);
    CHECK(env.remove_light("sun"));
    CHECK_FALSE(env.remove_light("sun"));

    // Material & Scene Submission
    RenderMaterial mat{.material_id = "gold", .shader_model = ShaderModel::PBRMetallicRoughness, .roughness = 0.2, .metallic = 0.9};
    SceneSubmissionQueue queue;
    queue.register_material(mat);
    CHECK(queue.find_material("gold") != nullptr);
    CHECK(queue.find_material("non_existent") == nullptr);

    RenderInstance inst1;
    inst1.instance_id = "box_inside";
    inst1.transform.position = {0.0, 0.0, 0.0};
    inst1.bounds_min = {-0.5, -0.5, -0.5};
    inst1.bounds_max = {0.5, 0.5, 0.5};

    RenderInstance inst2;
    inst2.instance_id = "box_behind";
    inst2.transform.position = {0.0, 0.0, 50.0}; // behind camera
    inst2.bounds_min = {-0.5, -0.5, -0.5};
    inst2.bounds_max = {0.5, 0.5, 0.5};

    queue.submit(inst1);
    queue.submit(inst2);
    CHECK(queue.instance_count() == 2u);

    uint32_t culled = 0;
    auto visible = queue.cull_and_sort(camera, culled);
    CHECK(culled == 1u);
    CHECK(visible.size() == 1u);
    CHECK(visible.front().instance_id == "box_inside");
}

// =============================================================================
// REN-03: 2D Schematic vs 3D Scene Presentation Modes
// =============================================================================

TEST_CASE("REN-03: 2D Schematic vs 3D Scene Presentation Modes") {
    SchematicRenderPipeline pipeline_2d;
    pipeline_2d.set_viewport(100.0, 50.0, 2.0);

    base::math::Vector2<double> canvas_pt{10.0, 20.0};
    auto screen_pt = pipeline_2d.canvas_to_screen(canvas_pt);
    CHECK(screen_pt.x == (10.0 + 100.0) * 2.0);
    CHECK(screen_pt.y == (20.0 + 50.0) * 2.0);

    auto unproj = pipeline_2d.screen_to_canvas(screen_pt);
    CHECK(std::abs(unproj.x - canvas_pt.x) < 1e-6);
    CHECK(std::abs(unproj.y - canvas_pt.y) < 1e-6);

    Schematic2DPrimitive wire;
    wire.type = Schematic2DPrimitive::Type::Wire;
    wire.id = "net_vdd";
    wire.points = {{0.0, 0.0}, {100.0, 0.0}, {100.0, 50.0}};
    pipeline_2d.submit_primitive(wire);

    Schematic2DPrimitive sym;
    sym.type = Schematic2DPrimitive::Type::SymbolBox;
    sym.id = "resistor_r1";
    sym.position = {120.0, 40.0};
    sym.size = {60.0, 30.0};
    pipeline_2d.submit_primitive(sym);

    CHECK(pipeline_2d.primitive_count() == 2u);
    auto [bmin, bmax] = pipeline_2d.bounding_box();
    CHECK(bmin.x == 0.0);
    CHECK(bmin.y == 0.0);
    CHECK(bmax.x >= 180.0);

    // Presentation controller
    ViewportPresentationController controller;
    CHECK(controller.mode() == PresentationMode::Mode3D_Scene);
    controller.set_mode(PresentationMode::Split_Hybrid);
    CHECK(controller.mode() == PresentationMode::Split_Hybrid);

    controller.set_split_ratio(0.4);
    CHECK(controller.split_ratio() == 0.4);
    controller.set_split_ratio(1.5); // clamps to 0.95
    CHECK(controller.split_ratio() == 0.95);
}

// =============================================================================
// REN-04: Object Picking, Ray Casting, Gizmos and Debug Draw
// =============================================================================

TEST_CASE("REN-04: Ray Casting, Picking, Transform Gizmo and Debug Draw") {
    ViewportCamera camera;
    camera.look_at({0.0, 0.0, 5.0}, {0.0, 0.0, 0.0});
    camera.fov_degrees = 60.0;
    camera.aspect_ratio = 1.0;

    // Center screen ray should aim directly forward (-Z)
    Ray center_ray = RayCaster::screen_point_to_ray(camera, 400.0, 300.0, 800.0, 600.0);
    CHECK(center_ray.direction.z < -0.9);

    // AABB intersection
    double t_min = 0.0, t_max = 0.0;
    Ray ray{.origin = {0.0, 0.0, 5.0}, .direction = {0.0, 0.0, -1.0}};
    bool aabb_hit = RayCaster::intersect_ray_aabb(ray, {-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0}, t_min, t_max);
    CHECK(aabb_hit);
    CHECK(std::abs(t_min - 4.0) < 1e-4);

    // Triangle intersection (Möller-Trumbore)
    base::math::Vector3<double> v0{-1.0, -1.0, 0.0};
    base::math::Vector3<double> v1{1.0, -1.0, 0.0};
    base::math::Vector3<double> v2{0.0, 1.0, 0.0};
    double t_tri = 0.0;
    base::math::Vector3<double> normal;
    bool tri_hit = RayCaster::intersect_ray_triangle(ray, v0, v1, v2, t_tri, normal);
    CHECK(tri_hit);
    CHECK(std::abs(t_tri - 5.0) < 1e-4);

    // Mesh picking
    TriangleMesh mesh;
    mesh.vertices = {v0, v1, v2};
    mesh.normals = {{0,0,1}, {0,0,1}, {0,0,1}};
    mesh.faces = {MeshFace{0, 1, 2}};

    std::unordered_map<std::string, TriangleMesh> mesh_assets;
    mesh_assets["triangle_mesh"] = mesh;

    SceneSubmissionQueue queue;
    RenderInstance inst;
    inst.instance_id = "inst_tri";
    inst.mesh_id = "triangle_mesh";
    inst.bounds_min = {-1.0, -1.0, -0.1};
    inst.bounds_max = {1.0, 1.0, 0.1};
    queue.submit(inst);

    auto hit = RayCaster::pick_closest(ray, queue, mesh_assets);
    REQUIRE(hit.has_value());
    CHECK(hit->instance_id == "inst_tri");
    CHECK(std::abs(hit->distance - 5.0) < 1e-4);

    // Transform Gizmo test
    TransformGizmo gizmo;
    gizmo.position = {0.0, 0.0, 0.0};
    gizmo.axis_length = 2.0;
    gizmo.handle_radius = 0.2;

    // Ray passing very close to X axis (y=0.05, z=0.0)
    Ray ray_near_x{.origin = {1.0, 0.05, 5.0}, .direction = {0.0, 0.0, -1.0}};
    CHECK(gizmo.test_hover(ray_near_x) == GizmoAxis::X);

    Ray ray_near_y{.origin = {0.05, 1.0, 5.0}, .direction = {0.0, 0.0, -1.0}};
    CHECK(gizmo.test_hover(ray_near_y) == GizmoAxis::Y);

    // Debug Draw Queue
    DebugDrawQueue debug;
    debug.add_line({0, 0, 0}, {1, 1, 1}, {1, 0, 0, 1}, 2);
    debug.add_box({-1, -1, -1}, {1, 1, 1}, {0, 1, 0, 1}, 1);
    CHECK(debug.lines().size() == 1u);
    CHECK(debug.boxes().size() == 1u);

    debug.tick_frame();
    // Line had duration 2, should still be alive; box had duration 1, should be gone
    CHECK(debug.lines().size() == 1u);
    CHECK(debug.boxes().empty());

    debug.tick_frame();
    CHECK(debug.lines().empty());
}

// =============================================================================
// REN-05: GPU Resource Lifetime, Asset Cache & Asynchronous Loading
// =============================================================================

TEST_CASE("REN-05: GPU Resource Lifetime, Asset Cache and Async Loader") {
    // GPU resource RAII
    {
        GpuResource res(base::Uuid::generate_random(), "vbo_test", GpuResourceType::VertexBuffer, 1024);
        CHECK(res.is_resident());
        CHECK(res.size_bytes() == 1024u);
        CHECK(res.type() == GpuResourceType::VertexBuffer);

        // Move semantics
        GpuResource moved(std::move(res));
        CHECK(moved.is_resident());
        CHECK(moved.size_bytes() == 1024u);
        CHECK_FALSE(res.is_resident());
    }

    // Asset Cache with small memory budget
    AssetCache cache(500); // 500 bytes budget
    TriangleMesh m1;
    m1.vertices.assign(10, {0,0,0}); // 10 * 24 = 240 bytes
    cache.insert_mesh("mesh_1", m1);
    CHECK(cache.mesh_count() == 1u);
    CHECK(cache.find_mesh("mesh_1") != nullptr);

    TriangleMesh m2;
    m2.vertices.assign(15, {1,1,1}); // 15 * 24 = 360 bytes -> total 600 > 500
    cache.insert_mesh("mesh_2", m2);

    // Should have evicted mesh_1 as oldest LRU
    CHECK(cache.mesh_count() == 1u);
    CHECK(cache.find_mesh("mesh_1") == nullptr);
    CHECK(cache.find_mesh("mesh_2") != nullptr);

    // Async loader
    AsyncAssetLoader loader;
    loader.queue_mesh_load("async_mesh", []() -> base::Result<TriangleMesh> {
        TriangleMesh loaded;
        loaded.vertices = {{0,0,0}, {1,0,0}, {0,1,0}};
        return base::Result<TriangleMesh>::ok(loaded);
    });

    CHECK(loader.status("async_mesh") == AssetLoadStatus::Queued);
    loader.update_progress();
    CHECK(loader.status("async_mesh") == AssetLoadStatus::Loading);
    loader.update_progress();
    CHECK(loader.status("async_mesh") == AssetLoadStatus::Ready);
    CHECK(loader.progress("async_mesh") == 1.0);
    REQUIRE(loader.get_loaded_mesh("async_mesh") != nullptr);
    CHECK(loader.get_loaded_mesh("async_mesh")->vertices.size() == 3u);
}

// =============================================================================
// REN-06: Scientific Visualization Overlays (Non-Invasive)
// =============================================================================

TEST_CASE("REN-06: Scientific Visualization Layers for Solver State") {
    // Colormaps
    auto c_low = Colormap::map_value(0.0, 0.0, 100.0, ColormapType::Viridis);
    auto c_high = Colormap::map_value(100.0, 0.0, 100.0, ColormapType::Viridis);
    CHECK(c_low[0] < c_high[0]); // Viridis red channel increases with value

    auto c_jet = Colormap::map_value(0.0, 0.0, 1.0, ColormapType::Jet);
    CHECK(c_jet[2] >= 0.5f); // Jet starts in blue range
    CHECK(c_jet[0] < 0.1f);

    // Scalar field contour layer
    ScalarFieldContourLayer contour;
    std::vector<ScalarFieldSample> samples = {
        {{0,0,0}, 10.0},
        {{1,0,0}, 50.0},
        {{2,0,0}, 90.0}
    };
    contour.set_samples(samples);
    contour.auto_fit_range();
    CHECK(contour.min_value() == 10.0);
    CHECK(contour.max_value() == 90.0);

    auto colors = contour.generate_sample_colors();
    CHECK(colors.size() == 3u);

    // Vector field streamlines
    VectorFieldStreamlineLayer stream_layer;
    std::vector<VectorFieldSample> v_samples = {
        {{0,0,0}, {1.0, 0.0, 0.0}},
        {{1,0,0}, {1.0, 0.0, 0.0}}
    };
    stream_layer.set_field(v_samples);
    auto arrows = stream_layer.generate_arrow_glyphs();
    CHECK(arrows.size() == 2u);

    auto lines = stream_layer.trace_streamlines({{0.0, 0.0, 0.0}}, 0.5, 5);
    REQUIRE(lines.size() == 1u);
    CHECK(lines.front().size() > 1u);
    CHECK(lines.front().back().x > 0.0); // Moving in +X direction

    // Probe marker overlay
    ProbeMarkerOverlay probes;
    probes.add_probe(ProbeMarker{.probe_id = "p1", .position = {1, 2, 3}, .label = "Temp sensor", .scalar_value = 300.0, .unit = "K"});
    CHECK(probes.probes().size() == 1u);
    probes.update_value("p1", 350.0);
    CHECK(probes.find_probe("p1")->scalar_value == 350.0);

    // Non-invasive deformation overlay
    TriangleMesh physics_mesh;
    physics_mesh.vertices = {{0,0,0}, {1,0,0}, {0,1,0}};
    physics_mesh.faces = {MeshFace{0, 1, 2}};

    DeformationMeshOverlay deform;
    deform.set_magnification_factor(10.0);
    std::vector<base::math::Vector3<double>> nodal_disp = {{0, 0.1, 0}, {0, 0.1, 0}, {0, 0.1, 0}};

    TriangleMesh deformed_mesh = deform.compute_deformed_mesh(physics_mesh, nodal_disp);
    // Base mesh is unchanged
    CHECK(physics_mesh.vertices[0].y == 0.0);
    // Visual mesh is magnified by factor 10.0 (0.1 * 10 = 1.0)
    CHECK(deformed_mesh.vertices[0].y == 1.0);
}

// =============================================================================
// REN-07: Render Profiling
// =============================================================================

TEST_CASE("REN-07: Render Profiling (Draw Calls, Memory, Shader Compilations)") {
    RenderProfiler profiler;
    RenderFrameStats f1{.draw_calls = 50, .cpu_frame_time_ms = 16.0};
    RenderFrameStats f2{.draw_calls = 120, .cpu_frame_time_ms = 24.0};

    profiler.record_frame(f1);
    profiler.record_frame(f2);

    CHECK(profiler.total_frames() == 2u);
    CHECK(profiler.average_cpu_frame_time_ms() == 20.0);
    CHECK(profiler.peak_cpu_frame_time_ms() == 24.0);
    CHECK(profiler.peak_draw_calls() == 120u);

    profiler.record_shader_compilation(15.5, false);
    profiler.record_shader_compilation(0.1, true);
    CHECK(profiler.compiled_shader_count() == 1u);
    CHECK(profiler.shader_cache_hits() == 1u);

    profiler.update_lod_metrics(500, 200, 50);
    auto lods = profiler.lod_counts();
    CHECK(lods[0] == 500u);
    CHECK(lods[1] == 200u);
    CHECK(lods[2] == 50u);

    std::string report = profiler.to_json_report();
    CHECK(report.find("\"total_frames\": 2") != std::string::npos);
    CHECK(report.find("\"peak_draw_calls\": 120") != std::string::npos);
}

// =============================================================================
// REN-08: Headless Software Mode & Framebuffer
// =============================================================================

TEST_CASE("REN-08: Headless Software Mode and Offscreen Framebuffer") {
    HeadlessFramebuffer fb(100, 100);
    CHECK(fb.width() == 100u);
    CHECK(fb.height() == 100u);

    fb.clear(0xFF000000, 1.0f);
    CHECK(fb.get_pixel(50, 50) == 0xFF000000);
    CHECK(fb.get_depth(50, 50) == 1.0f);

    // Draw line
    fb.draw_line(10, 10, 20, 10, 0xFFFF0000);
    CHECK(fb.get_pixel(15, 10) == 0xFFFF0000);

    // Draw filled rect
    fb.draw_filled_rect(30, 30, 10, 10, 0xFF00FF00);
    CHECK(fb.get_pixel(35, 35) == 0xFF00FF00);

    uint64_t hash = fb.compute_buffer_checksum();
    CHECK(hash != 0u);

    std::string ppm = fb.export_ppm_ascii();
    CHECK(ppm.rfind("P3", 0) == 0); // starts with P3

    // Headless Software Backend
    HeadlessSoftwareBackend backend(200, 150);
    RenderBackendConfig cfg;
    cfg.initial_width = 200;
    cfg.initial_height = 150;
    REQUIRE(backend.initialize(cfg).is_ok());

    SceneSubmissionQueue queue;
    RenderInstance inst;
    inst.instance_id = "headless_cube";
    queue.submit(inst);

    RenderPassDescriptor pass;
    pass.clear_color = {0.2f, 0.2f, 0.2f, 1.0f};
    REQUIRE(backend.begin_frame().is_ok());
    REQUIRE(backend.execute_pass(pass, queue).is_ok());
    REQUIRE(backend.end_frame().is_ok());

    auto stats = backend.current_frame_stats();
    CHECK(stats.draw_calls == 1u);
    CHECK(stats.instances_rendered == 1u);
}

// =============================================================================
// REN-09: Renderer Capability Detection & Fallback Path
// =============================================================================

TEST_CASE("REN-09: Capabilities Detection and Fallback Handling") {
    // Failing hardware backend
    RenderDeviceManager mgr(RenderFallbackPolicy::FallbackToSoftware);
    CHECK(mgr.fallback_policy() == RenderFallbackPolicy::FallbackToSoftware);

    RenderBackendConfig cfg;
    cfg.initial_width = 800;
    cfg.initial_height = 600;

    // Direct creation of mock hardware backend
    MockHardwareRendererBackend hw_backend(RendererBackendType::Vulkan, false);
    REQUIRE(hw_backend.initialize(cfg).is_ok());
    auto caps = hw_backend.capabilities();
    CHECK(caps.is_hardware_accelerated);
    CHECK(caps.supports_ray_tracing);
    CHECK(caps.supports_mesh_shaders);

    // Test device manager fallback when device fails initialization
    RenderDeviceManager fail_mgr(RenderFallbackPolicy::FailIfUnavailable);
    // Requesting a failing hardware device should fail if policy is FailIfUnavailable
    MockHardwareRendererBackend failing_backend(RendererBackendType::Vulkan, true);
    auto fail_res = failing_backend.initialize(cfg);
    CHECK_FALSE(fail_res.is_ok());

    // When manager has FallbackToSoftware policy
    auto created = mgr.create_backend(RendererBackendType::HeadlessSoftware, cfg);
    REQUIRE(created.is_ok());
    CHECK(created.value()->backend_type() == RendererBackendType::HeadlessSoftware);
}

// =============================================================================
// REN-10: Independent Rendering Test Suite & Validation
// =============================================================================

TEST_CASE("REN-10: Independent Rendering Validation and Physics State Decoupling") {
    // Framebuffer MSE comparison
    HeadlessFramebuffer fb1(50, 50);
    HeadlessFramebuffer fb2(50, 50);
    fb1.clear(0xFFFFFFFF);
    fb2.clear(0xFFFFFFFF);
    CHECK(RenderTestHarness::compute_framebuffer_mse(fb1, fb2) == 0.0);

    fb2.clear(0xFF000000);
    CHECK(RenderTestHarness::compute_framebuffer_mse(fb1, fb2) > 0.0);

    // Frustum culling invariance
    ViewportCamera camera;
    SceneSubmissionQueue queue;
    for (int i = 0; i < 10; ++i) {
        RenderInstance inst;
        inst.instance_id = "node_" + std::to_string(i);
        inst.transform.position = {static_cast<double>(i * 5), 0.0, -10.0};
        queue.submit(inst);
    }
    CHECK(RenderTestHarness::verify_frustum_culling_invariance(queue, camera));

    // Physics state decoupling
    TriangleMesh phys_mesh;
    phys_mesh.vertices = {{0,0,0}, {1,0,0}, {0,1,0}};
    phys_mesh.faces = {MeshFace{0, 1, 2}};

    DeformationMeshOverlay overlay;
    overlay.set_magnification_factor(50.0);
    std::vector<base::math::Vector3<double>> disp = {{0.01, 0.02, 0.0}, {0.0, 0.01, 0.0}, {0.02, 0.0, 0.0}};

    CHECK(RenderTestHarness::verify_physics_state_decoupling(phys_mesh, overlay, disp));
}
