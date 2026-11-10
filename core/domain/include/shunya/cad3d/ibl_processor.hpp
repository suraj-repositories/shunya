#pragma once

#include "shunya/cad3d/pbr_material.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace shunya::cad3d {

struct BrdfLut2D {
    uint32_t width{64};
    uint32_t height{64};
    std::vector<std::array<double, 2>> rg_scale_bias; // row: roughness, col: NdotV

    [[nodiscard]] std::array<double, 2> sample(double n_dot_v, double roughness) const noexcept;
};

struct EnvironmentMap {
    std::function<Vec3d(const Vec3d& dir)> radiance_fn;

    [[nodiscard]] Vec3d sample(const Vec3d& dir) const {
        return radiance_fn ? radiance_fn(dir.normalized()) : Vec3d{1.0, 1.0, 1.0};
    }

    [[nodiscard]] static EnvironmentMap studio_hdr();
    [[nodiscard]] static EnvironmentMap uniform_white(double intensity = 1.0);
};

/**
 * @brief Split-sum Image-Based Lighting (IBL) pre-filter, irradiance convolution, and BRDF LUT generator.
 */
class IblProcessor {
public:
    [[nodiscard]] static BrdfLut2D integrate_brdf_lut(
        uint32_t width = 64,
        uint32_t height = 64,
        uint32_t sample_count = 128
    );

    [[nodiscard]] static Vec3d convolve_diffuse_irradiance(
        const EnvironmentMap& env,
        const Vec3d& normal,
        uint32_t sample_count = 256
    );

    [[nodiscard]] static Vec3d prefilter_specular_env(
        const EnvironmentMap& env,
        const Vec3d& reflection_dir,
        double roughness,
        uint32_t sample_count = 256
    );

    /**
     * @brief Evaluates full split-sum IBL lighting for a PBR surface point.
     */
    [[nodiscard]] static Vec3d evaluate_split_sum(
        const PbrMaterialParams& mat,
        const Vec3d& N,
        const Vec3d& V,
        const EnvironmentMap& env,
        const BrdfLut2D& lut,
        uint32_t env_samples = 128
    );

    /**
     * @brief Reference ground-truth Monte Carlo hemispherical integration for split-sum validation.
     */
    [[nodiscard]] static Vec3d integrate_monte_carlo_ground_truth(
        const PbrMaterialParams& mat,
        const Vec3d& N,
        const Vec3d& V,
        const EnvironmentMap& env,
        uint32_t sample_count = 1024
    );
};

} // namespace shunya::cad3d
