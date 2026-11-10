#pragma once

#include "shunya/cad3d/csm_pipeline.hpp"
#include "shunya/cad3d/rhi_renderer_engine.hpp"

#include <vector>

namespace shunya::cad3d {

/**
 * @brief 4-split Cascaded Shadow Maps (CSM) with texel snapping & 3x3 PCF filtering.
 */
class ShadowPass {
public:
    void configure(const CsmConfig& config);

    [[nodiscard]] std::vector<CascadeSplitInfo> compute_cascades(
        const core::domain::ViewportCamera& camera,
        const Vec3d& light_dir
    ) const;

    void render_shadow_maps(
        const core::domain::ViewportCamera& camera,
        const Vec3d& light_dir,
        const std::vector<RhiDrawItem>& casters
    );

    /**
     * @brief Samples PCF shadow factor [0 = full shadow, 1 = fully lit] for world position & surface normal.
     */
    [[nodiscard]] double sample_shadow_pcf(
        const Point3D& world_pos,
        const Vec3d& world_normal,
        const Vec3d& light_dir,
        double view_depth
    ) const noexcept;

    [[nodiscard]] const std::vector<CascadeSplitInfo>& cascades() const noexcept { return cascades_; }
    [[nodiscard]] const std::vector<std::vector<double>>& depth_maps() const noexcept { return depth_maps_; }

private:
    CsmConfig config_{};
    std::vector<CascadeSplitInfo> cascades_;
    std::vector<std::vector<double>> depth_maps_;
};

/**
 * @brief Forward+ tiled compute light culler partitioning 64+ point lights into screen tiles.
 */
class TiledLightCuller {
public:
    void cull_lights(
        const core::domain::ViewportCamera& camera,
        uint32_t screen_width,
        uint32_t screen_height,
        const std::vector<double>& depth_buffer,
        const std::vector<PointLight3D>& lights,
        uint32_t tile_size = 16
    );

    [[nodiscard]] uint32_t tiles_x() const noexcept { return tiles_x_; }
    [[nodiscard]] uint32_t tiles_y() const noexcept { return tiles_y_; }
    [[nodiscard]] const std::vector<uint32_t>& tile_lights(uint32_t tx, uint32_t ty) const noexcept;

private:
    uint32_t tiles_x_{0};
    uint32_t tiles_y_{0};
    uint32_t tile_size_{16};
    std::vector<std::vector<uint32_t>> per_tile_indices_;
    static inline const std::vector<uint32_t> empty_list_{};
};

} // namespace shunya::cad3d
