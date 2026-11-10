#include "tests/test_framework.hpp"

#include "shunya/cad3d/ibl_processor.hpp"
#include "shunya/cad3d/material_library.hpp"
#include "shunya/cad3d/pbr_material.hpp"
#include "shunya/cad3d/shader_compiler.hpp"
#include "shunya/cad3d/volume_renderer.hpp"

#include <cmath>
#include <vector>

using namespace shunya::cad3d;
using shunya::core::domain::ViewportCamera;

TEST_CASE("PbrBrdf: Cook-Torrance energy conservation across roughness/metallic grid & material presets") {
    const auto lib = PbrMaterialLibrary::create_default();
    REQUIRE(lib.count() >= 6);
    const auto* copper = lib.find("copper");
    const auto* plastic = lib.find("abs_plastic");
    REQUIRE(copper != nullptr);
    REQUIRE(plastic != nullptr);

    // Dielectric F0 = 0.04 at normal incidence
    const Vec3d f0_dielec = PbrBrdf::compute_f0(*plastic);
    REQUIRE(std::abs(f0_dielec.x - 0.04) < 1e-9);
    REQUIRE(std::abs(f0_dielec.y - 0.04) < 1e-9);
    REQUIRE(std::abs(f0_dielec.z - 0.04) < 1e-9);

    // Metals take albedo tint
    const Vec3d f0_copper = PbrBrdf::compute_f0(*copper);
    REQUIRE(f0_copper.x > 0.9 && f0_copper.z < 0.6);

    // Verify energy conservation <= 1.0 across full parameter matrix
    for (double roughness : {0.10, 0.30, 0.60, 1.00}) {
        for (double metallic : {0.0, 0.5, 1.0}) {
            for (double ndv : {0.20, 0.50, 0.90}) {
                const PbrMaterialParams mat{"test", {0.95, 0.95, 0.95}, metallic, roughness, 1.0, {0, 0, 0}};
                const double refl = PbrBrdf::integrate_hemisphere_reflectance(mat, ndv, 24, 48);
                REQUIRE(refl > 0.0);
                REQUIRE(refl <= 1.015); // <= 1.0 within Riemann quadrature discretization
            }
        }
    }

    ShaderCompiler sc;
    REQUIRE(sc.compile(PbrBrdf::vert_shader_source(), ShaderStage::Vertex, ShaderTargetApi::SpirV_Vulkan).valid);
    REQUIRE(sc.compile(PbrBrdf::frag_shader_source(), ShaderStage::Fragment, ShaderTargetApi::SpirV_Vulkan).valid);
}

TEST_CASE("IblProcessor: Split-sum IBL matches Monte Carlo ground truth & LUT furnace conservation") {
    const auto lut = IblProcessor::integrate_brdf_lut(32, 32, 128);
    // White furnace check: scale + bias <= 1.01 across LUT
    for (double ndv : {0.15, 0.5, 0.9}) {
        for (double r : {0.1, 0.4, 0.8}) {
            const auto sb = lut.sample(ndv, r);
            REQUIRE(sb[0] >= 0.0);
            REQUIRE(sb[1] >= 0.0);
            REQUIRE(sb[0] + sb[1] <= 1.01);
        }
    }

    const EnvironmentMap studio = EnvironmentMap::studio_hdr();
    const Vec3d N{0.0, 1.0, 0.0};
    const Vec3d V = Vec3d{0.3, 0.9, 0.2}.normalized();

    const PbrMaterialParams gold{"gold", {1.0, 0.766, 0.336}, 1.0, 0.25, 1.0, {0, 0, 0}};
    const Vec3d split_gold = IblProcessor::evaluate_split_sum(gold, N, V, studio, lut, 256);
    const Vec3d mc_gold = IblProcessor::integrate_monte_carlo_ground_truth(gold, N, V, studio, 1024);

    // Validate split-sum approximates Monte Carlo integral accurately across RGB channels
    const Vec3d diff_gold = split_gold - mc_gold;
    REQUIRE(diff_gold.length() / std::max(0.1, mc_gold.length()) < 0.08);

    const PbrMaterialParams plastic{"plastic", {0.2, 0.5, 0.8}, 0.0, 0.5, 1.0, {0, 0, 0}};
    const Vec3d split_pl = IblProcessor::evaluate_split_sum(plastic, N, V, studio, lut, 256);
    const Vec3d mc_pl = IblProcessor::integrate_monte_carlo_ground_truth(plastic, N, V, studio, 1024);
    const Vec3d diff_pl = split_pl - mc_pl;
    REQUIRE(diff_pl.length() / std::max(0.1, mc_pl.length()) < 0.08);
}

TEST_CASE("VolumeRaymarcher: Beer-Lambert analytical opacity correctness & thermal plume field rendering") {
    // 1. Uniform slab [-1, 1]^3 with constant scalar 1.0 -> optical depth = sigma * L = 0.75 * 2.0 = 1.5
    VolumeGrid3D uniform_slab;
    uniform_slab.allocate(4, 4, 4, {{-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0}}, 1.0);
    const double exact_opacity = 1.0 - std::exp(-0.75 * 2.0);

    const Ray3D center_ray{{0.0, 0.0, 5.0}, {0.0, 0.0, -1.0}};
    const auto res_coarse = VolumeRaymarcher::integrate_ray(center_ray, uniform_slab, {1, 1, 0.20, 0.75, 0.0, 1.0});
    const auto res_fine   = VolumeRaymarcher::integrate_ray(center_ray, uniform_slab, {1, 1, 0.04, 0.75, 0.0, 1.0});

    REQUIRE(std::abs(res_coarse[3] - exact_opacity) < 1e-6);
    REQUIRE(std::abs(res_fine[3] - exact_opacity) < 1e-6);

    // 2. Smooth 3D spherical thermal plume field inside furnace bounds
    VolumeGrid3D plume;
    plume.allocate(16, 16, 16, {{-2.0, -2.0, -2.0}, {2.0, 2.0, 2.0}}, 0.0);
    for (uint32_t z = 0; z < 16; ++z) {
        const double wz = -2.0 + 4.0 * (z / 15.0);
        for (uint32_t y = 0; y < 16; ++y) {
            const double wy = -2.0 + 4.0 * (y / 15.0);
            for (uint32_t x = 0; x < 16; ++x) {
                const double wx = -2.0 + 4.0 * (x / 15.0);
                const double r2 = wx * wx + wy * wy + wz * wz;
                plume.set_voxel(x, y, z, std::exp(-r2 * 0.8));
            }
        }
    }

    ViewportCamera cam;
    cam.position = {0.0, 0.0, 6.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.aspect_ratio = 64.0 / 48.0;

    const auto vol_img = VolumeRaymarcher::render(cam, plume, {64, 48, 0.15, 1.5, 0.0, 1.0});
    REQUIRE(vol_img.render_time_ms >= 0.0);
    const size_t center_idx = 24 * 64 + 32;
    const size_t corner_idx = 0;
    REQUIRE(vol_img.opacity_buffer[center_idx] > 0.5);
    REQUIRE(vol_img.target.color_rgba[center_idx][0] > vol_img.target.color_rgba[corner_idx][0]);
}
