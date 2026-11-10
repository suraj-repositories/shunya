#pragma once

#include "shunya/cad3d/3d_texture_cache.hpp"
#include "shunya/cad3d/raycast_query.hpp"
#include "shunya/cad3d/render_pass.hpp"
#include "shunya/core/domain/rendering.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>

namespace shunya::cad3d {

struct TransferSample {
    Vec3d emission{0.0, 0.0, 0.0};
    double absorption{0.0};
};

struct VolumeRenderConfig {
    uint32_t width{96};
    uint32_t height{72};
    double step_size{0.15};
    double density_scale{1.0};
    double min_value{0.0};
    double max_value{1.0};
};

struct VolumeRenderResult {
    RhiRenderTarget target;
    std::vector<double> opacity_buffer;
    double render_time_ms{0.0};
};

/**
 * @brief Emission-absorption volumetric raymarcher with blackbody/colormap transfer functions.
 */
class VolumeRaymarcher {
public:
    [[nodiscard]] static TransferSample blackbody_transfer(
        double normalized_scalar,
        double density_scale = 1.0
    ) noexcept;

    [[nodiscard]] static VolumeRenderResult render(
        const core::domain::ViewportCamera& camera,
        const VolumeGrid3D& volume,
        const VolumeRenderConfig& config = {},
        const std::function<TransferSample(double scalar)>& transfer_fn = nullptr
    );

    [[nodiscard]] static std::array<double, 4> integrate_ray(
        const Ray3D& ray,
        const VolumeGrid3D& volume,
        const VolumeRenderConfig& config,
        const std::function<TransferSample(double scalar)>& transfer_fn = nullptr
    );

    [[nodiscard]] static std::string_view shader_source() noexcept;
};

} // namespace shunya::cad3d
