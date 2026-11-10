#include "tests/test_framework.hpp"

#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/cad3d/rhi_renderer_engine.hpp"
#include "shunya/cad3d/shadow_pass.hpp"
#include "shunya/cad3d/subdivision.hpp"
#include "shunya/cad3d/wboit_pass.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <vector>

using namespace shunya::cad3d;
using shunya::cad2d::Path2D;
using shunya::core::base::math::Matrix4x4;
using shunya::core::domain::ViewportCamera;

namespace {

SolidMesh3D make_box(double sx, double sy, double sz) {
    const double hx = sx * 0.5;
    const double hy = sy * 0.5;
    const Path2D rect{{-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy}};
    return ExtrusionEngine::extrude(rect, {}, {sz, 100.0, 0.0, EndCondition::Symmetric, SketchPlane3D::xy()});
}

SolidMesh3D make_sphere(double radius, size_t subd = 2) {
    SolidMesh3D m;
    m.vertices = {
        { radius, 0, 0}, {-radius, 0, 0},
        {0,  radius, 0}, {0, -radius, 0},
        {0, 0,  radius}, {0, 0, -radius}
    };
    m.triangles = {
        {0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4},
        {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}
    };
    SolidMesh3D s = SubdivisionEngine::subdivide_loop(m, subd);
    for (auto& v : s.vertices) {
        if (v.length_squared() > 1e-18) v = v.normalized() * radius;
    }
    return s;
}

} // namespace

TEST_CASE("RhiRendererEngine: cross-backend Vulkan vs D3D12 parity, shader cache, and PSOs") {
    const SolidMesh3D box = make_box(2.0, 2.0, 2.0);
    ViewportCamera cam;
    cam.position = {3.5, 2.5, 5.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.aspect_ratio = 160.0 / 120.0;

    std::vector<RhiDrawItem> items{
        {&box, Matrix4x4<double>::identity(), {0.8, 0.3, 0.2, 1.0}, 0.4, 0.0}
    };

    RhiRendererEngine vk_eng;
    REQUIRE(vk_eng.initialize({RhiBackendApi::Vulkan, 160, 120, true}));
    const uint32_t s_id = vk_eng.get_or_create_sampler({"shadow", true, true, true});
    REQUIRE(vk_eng.get_or_create_sampler({"shadow", true, true, true}) == s_id);
    const auto ubo_id = vk_eng.allocate_uniform_buffer(std::array<double, 4>{1.0, 2.0, 3.0, 4.0});
    REQUIRE(vk_eng.read_uniform_buffer(ubo_id) != nullptr);

    // Shader compiler reflection + cache hit verification
    const auto cmod1 = vk_eng.shader_compiler().compile(ShaderCompiler::builtin_light_culler_comp(), ShaderStage::Compute, ShaderTargetApi::SpirV_Vulkan);
    const auto cmod2 = vk_eng.shader_compiler().compile(ShaderCompiler::builtin_light_culler_comp(), ShaderStage::Compute, ShaderTargetApi::SpirV_Vulkan);
    REQUIRE(cmod1.valid);
    REQUIRE(cmod1.bindings.size() == 2);
    REQUIRE(cmod2.valid);
    REQUIRE(vk_eng.shader_compiler().cache_hits() >= 1);

    vk_eng.begin_frame();
    vk_eng.render_scene(cam, items);
    vk_eng.end_frame();
    REQUIRE(vk_eng.frame_metrics().pixels_shaded > 100);
    const uint64_t vk_checksum = vk_eng.framebuffer().quantized_checksum();

    RhiRendererEngine dx_eng;
    REQUIRE(dx_eng.initialize({RhiBackendApi::Direct3D12, 160, 120, true}));
    dx_eng.begin_frame();
    dx_eng.render_scene(cam, items);
    dx_eng.end_frame();
    const uint64_t dx_checksum = dx_eng.framebuffer().quantized_checksum();

    REQUIRE(vk_checksum == dx_checksum);
}

TEST_CASE("ShadowPass & TiledLightCuller: 4-split CSM occlusion, PCF anti-acne, and 80+ dynamic lights") {
    const SolidMesh3D pillar = make_box(1.5, 4.0, 1.5);
    const SolidMesh3D floor_box = make_box(20.0, 0.5, 20.0);

    ViewportCamera cam;
    cam.position = {0.0, 6.0, 14.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.near_plane = 0.5;
    cam.far_plane = 80.0;
    cam.aspect_ratio = 1.0;

    // Floor sits at y = -2.25..-1.75; pillar sits on y = -2..+2 at origin
    const auto floor_xf = Matrix4x4<double>::translation({0.0, -2.0, 0.0});
    std::vector<RhiDrawItem> casters{
        {&pillar, Matrix4x4<double>::identity(), {0.8, 0.8, 0.8, 1.0}, 0.5, 0.0},
        {&floor_box, floor_xf, {0.5, 0.5, 0.5, 1.0}, 0.8, 0.0}
    };

    const Vec3d sun_dir = Vec3d{0.6, 0.8, 0.0}.normalized(); // light shines from +X/+Y down onto -X floor
    ShadowPass csm;
    csm.configure({4, 128, 0.75, 60.0, 0.005, 0.015});
    csm.render_shadow_maps(cam, sun_dir, casters);
    REQUIRE(csm.cascades().size() == 4);
    REQUIRE(csm.depth_maps().size() == 4);

    // Point at (-1.5, -1.75, 0) lies inside pillar's shadow along -sun_dir; point (+4.5, -1.75, 0) is cleanly sunlit
    const double shadow_factor = csm.sample_shadow_pcf({-1.2, -1.75, 0.0}, {0.0, 1.0, 0.0}, sun_dir, 14.0);
    const double sunlit_factor = csm.sample_shadow_pcf({4.5, -1.75, 0.0}, {0.0, 1.0, 0.0}, sun_dir, 14.0);
    REQUIRE(shadow_factor < 0.35);
    REQUIRE(sunlit_factor > 0.9);

    // Forward+ Tiled Light Culler with 80 lights (center lights vs off-frustum lights)
    std::vector<PointLight3D> lights;
    lights.reserve(80);
    // Light 0 right at origin visible to center tile
    lights.push_back({{0.0, 0.0, 0.0}, 4.0, {1, 1, 1}, 2.0});
    // Lights 1..79 placed far outside camera frustum (x = 500+)
    for (int i = 1; i < 80; ++i) {
        lights.push_back({{500.0 + i * 10.0, 0.0, 0.0}, 2.0, {1, 0, 0}, 1.0});
    }

    TiledLightCuller culler;
    std::vector<double> dummy_depth(128 * 128, 0.185); // linear z ~ 15.2 matching origin distance from camera
    culler.cull_lights(cam, 128, 128, dummy_depth, lights, 16);
    REQUIRE(culler.tiles_x() == 8);
    REQUIRE(culler.tiles_y() == 8);
    const auto& center_tile = culler.tile_lights(4, 4);
    REQUIRE(center_tile.size() == 1);
    REQUIRE(center_tile[0] == 0);
    REQUIRE(culler.tile_lights(0, 0).empty());
}

TEST_CASE("WboitPass: 5 overlapping spheres yield order-independent output & exact analytical single blend") {
    const SolidMesh3D sph = make_sphere(1.0, 2);
    ViewportCamera cam;
    cam.position = {0.0, 0.0, 6.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.aspect_ratio = 1.0;

    // 5 concentric spheres with distinct radii & colors
    std::vector<RhiDrawItem> forward_order;
    const std::array<std::array<double, 4>, 5> colors{{
        {0.9, 0.2, 0.2, 0.25},
        {0.2, 0.9, 0.2, 0.25},
        {0.2, 0.4, 0.9, 0.25},
        {0.9, 0.8, 0.2, 0.25},
        {0.8, 0.3, 0.9, 0.25}
    }};
    for (size_t i = 0; i < 5; ++i) {
        const double s = 1.6 - static_cast<double>(i) * 0.22;
        forward_order.push_back({&sph, Matrix4x4<double>::scaling({s, s, s}), colors[i], 0.3, 0.0});
    }
    std::vector<RhiDrawItem> reverse_order(forward_order.rbegin(), forward_order.rend());

    RhiRenderTarget rt_fwd, rt_rev;
    rt_fwd.resize(64, 64);
    rt_rev.resize(64, 64);
    rt_fwd.clear({0.1, 0.1, 0.15, 1.0}, 1.0);
    rt_rev.clear({0.1, 0.1, 0.15, 1.0}, 1.0);

    WboitPass pass_fwd, pass_rev;
    pass_fwd.resize(64, 64);
    pass_rev.resize(64, 64);

    pass_fwd.accumulate(cam, forward_order, rt_fwd.depth);
    pass_fwd.composite(rt_fwd);

    pass_rev.accumulate(cam, reverse_order, rt_rev.depth);
    pass_rev.composite(rt_rev);

    // Order independence check: pixel colors match within floating-point accumulation accuracy
    double max_diff = 0.0;
    for (size_t i = 0; i < rt_fwd.color_rgba.size(); ++i) {
        for (int c = 0; c < 3; ++c) {
            max_diff = std::max(max_diff, std::abs(rt_fwd.color_rgba[i][c] - rt_rev.color_rgba[i][c]));
        }
    }
    REQUIRE(max_diff < 1e-12);

    // Verify single transparent layer matches exact analytical alpha blend: alpha * fg + (1 - alpha) * bg
    RhiRenderTarget rt_single;
    rt_single.resize(64, 64);
    rt_single.clear({0.2, 0.4, 0.8, 1.0}, 1.0);
    WboitPass pass_single;
    pass_single.resize(64, 64);
    pass_single.accumulate(cam, {{&sph, Matrix4x4<double>::identity(), {0.9, 0.3, 0.1, 0.4}, 0.5, 0.0}}, rt_single.depth);
    pass_single.composite(rt_single);

    const size_t center_px = 32 * 64 + 32;
    REQUIRE(std::abs(rt_single.color_rgba[center_px][0] - (0.4 * 0.9 + 0.6 * 0.2)) < 1e-6);
    REQUIRE(std::abs(rt_single.color_rgba[center_px][1] - (0.4 * 0.3 + 0.6 * 0.4)) < 1e-6);
    REQUIRE(std::abs(rt_single.color_rgba[center_px][2] - (0.4 * 0.1 + 0.6 * 0.8)) < 1e-6);
}
