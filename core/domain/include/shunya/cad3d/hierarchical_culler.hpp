#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/cad3d/rhi_renderer_engine.hpp"
#include "shunya/core/domain/rendering.hpp"
#include "shunya/core/base/math.hpp"

#include <vector>
#include <memory>
#include <array>
#include <string>

namespace shunya::cad3d {

enum class FrustumContainment {
    Outside,
    Intersects,
    Inside
};

struct LodLevel {
    const SolidMesh3D* mesh{nullptr};
    double max_distance{100.0}; // When camera distance <= max_distance, this LOD is selected
    std::string lod_tag{"LOD0"};
};

struct LodPart {
    std::string name;
    core::base::math::Matrix4x4<double> transform{core::base::math::Matrix4x4<double>::identity()};
    Point3D local_bbox_min{-0.5, -0.5, -0.5};
    Point3D local_bbox_max{0.5, 0.5, 0.5};
    std::vector<LodLevel> lods; // Sorted by increasing max_distance
    std::array<double, 4> base_color{0.8, 0.8, 0.8, 1.0};
    double roughness{0.4};
    double metallic{0.0};
    bool visible{true};

    [[nodiscard]] const SolidMesh3D* select_lod(double distance) const noexcept;
    [[nodiscard]] int select_lod_index(double distance) const noexcept;
};

struct AssemblyCullNode {
    std::string name;
    core::base::math::Matrix4x4<double> local_transform{core::base::math::Matrix4x4<double>::identity()};
    core::base::math::Matrix4x4<double> world_transform{core::base::math::Matrix4x4<double>::identity()};
    Point3D world_bbox_min{-1, -1, -1};
    Point3D world_bbox_max{1, 1, 1};
    Point3D world_center{0, 0, 0};
    double world_radius{1.0};

    std::vector<LodPart> parts;
    std::vector<std::unique_ptr<AssemblyCullNode>> children;

    void update_bounds(const core::base::math::Matrix4x4<double>& parent_transform = core::base::math::Matrix4x4<double>::identity());
};

struct CullingMetrics {
    uint32_t nodes_tested{0};
    uint32_t nodes_culled{0};
    uint32_t parts_tested{0};
    uint32_t parts_culled{0};
    uint32_t parts_rendered{0};
    std::array<uint32_t, 4> lod_counts{0, 0, 0, 0}; // Count of parts rendered at LOD 0, 1, 2, 3+
};

class HierarchicalCuller {
public:
    static FrustumContainment test_containment(
        const core::domain::Frustum& frustum,
        const Point3D& bbox_min,
        const Point3D& bbox_max
    ) noexcept;

    static FrustumContainment test_sphere_containment(
        const core::domain::Frustum& frustum,
        const Point3D& center,
        double radius
    ) noexcept;

    [[nodiscard]] std::vector<RhiDrawItem> cull_and_select_lod(
        const AssemblyCullNode& root,
        const core::domain::ViewportCamera& camera,
        CullingMetrics* out_metrics = nullptr
    ) const;

private:
    void cull_node(
        const AssemblyCullNode& node,
        const core::domain::Frustum& frustum,
        const Point3D& cam_pos,
        FrustumContainment parent_containment,
        std::vector<RhiDrawItem>& out_items,
        CullingMetrics& metrics
    ) const;
};

} // namespace shunya::cad3d
