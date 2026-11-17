#include "shunya/cad3d/hierarchical_culler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shunya::cad3d {

using core::base::math::Vector3;

namespace {

Point3D transform_point(const core::base::math::Matrix4x4<double>& m, const Point3D& p) noexcept {
    const auto& mat = m.m;
    const double w = mat[12] * p.x + mat[13] * p.y + mat[14] * p.z + mat[15];
    const double inv_w = (std::abs(w) > 1e-9) ? 1.0 / w : 1.0;
    return {
        (mat[0] * p.x + mat[1] * p.y + mat[2] * p.z + mat[3]) * inv_w,
        (mat[4] * p.x + mat[5] * p.y + mat[6] * p.z + mat[7]) * inv_w,
        (mat[8] * p.x + mat[9] * p.y + mat[10] * p.z + mat[11]) * inv_w
    };
}

} // namespace

const SolidMesh3D* LodPart::select_lod(double distance) const noexcept {
    for (const auto& lod : lods) {
        if (distance <= lod.max_distance) {
            return lod.mesh;
        }
    }
    return nullptr;
}

int LodPart::select_lod_index(double distance) const noexcept {
    for (size_t i = 0; i < lods.size(); ++i) {
        if (distance <= lods[i].max_distance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AssemblyCullNode::update_bounds(const core::base::math::Matrix4x4<double>& parent_transform) {
    world_transform = parent_transform * local_transform;

    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double min_z = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    double max_z = -std::numeric_limits<double>::infinity();

    auto expand = [&](const Point3D& pt) {
        min_x = std::min(min_x, pt.x);
        min_y = std::min(min_y, pt.y);
        min_z = std::min(min_z, pt.z);
        max_x = std::max(max_x, pt.x);
        max_y = std::max(max_y, pt.y);
        max_z = std::max(max_z, pt.z);
    };

    bool has_geometry = false;

    for (const auto& part : parts) {
        const auto part_xf = world_transform * part.transform;
        const Point3D corners[8] = {
            {part.local_bbox_min.x, part.local_bbox_min.y, part.local_bbox_min.z},
            {part.local_bbox_max.x, part.local_bbox_min.y, part.local_bbox_min.z},
            {part.local_bbox_max.x, part.local_bbox_max.y, part.local_bbox_min.z},
            {part.local_bbox_min.x, part.local_bbox_max.y, part.local_bbox_min.z},
            {part.local_bbox_min.x, part.local_bbox_min.y, part.local_bbox_max.z},
            {part.local_bbox_max.x, part.local_bbox_min.y, part.local_bbox_max.z},
            {part.local_bbox_max.x, part.local_bbox_max.y, part.local_bbox_max.z},
            {part.local_bbox_min.x, part.local_bbox_max.y, part.local_bbox_max.z}
        };
        for (const auto& corner : corners) {
            expand(transform_point(part_xf, corner));
        }
        has_geometry = true;
    }

    for (auto& child : children) {
        if (child) {
            child->update_bounds(world_transform);
            expand(child->world_bbox_min);
            expand(child->world_bbox_max);
            has_geometry = true;
        }
    }

    if (!has_geometry) {
        world_bbox_min = {-0.5, -0.5, -0.5};
        world_bbox_max = {0.5, 0.5, 0.5};
    } else {
        world_bbox_min = {min_x, min_y, min_z};
        world_bbox_max = {max_x, max_y, max_z};
    }

    world_center = (world_bbox_min + world_bbox_max) * 0.5;
    world_radius = (world_bbox_max - world_center).length();
}

FrustumContainment HierarchicalCuller::test_containment(
    const core::domain::Frustum& frustum,
    const Point3D& bbox_min,
    const Point3D& bbox_max
) noexcept {
    bool all_inside = true;
    for (const auto& plane : frustum.planes) {
        const Vector3<double> p{
            (plane.normal.x >= 0.0) ? bbox_max.x : bbox_min.x,
            (plane.normal.y >= 0.0) ? bbox_max.y : bbox_min.y,
            (plane.normal.z >= 0.0) ? bbox_max.z : bbox_min.z
        };
        const Vector3<double> n{
            (plane.normal.x >= 0.0) ? bbox_min.x : bbox_max.x,
            (plane.normal.y >= 0.0) ? bbox_min.y : bbox_max.y,
            (plane.normal.z >= 0.0) ? bbox_min.z : bbox_max.z
        };

        if (plane.signed_distance(p) < 0.0) {
            return FrustumContainment::Outside;
        }
        if (plane.signed_distance(n) < 0.0) {
            all_inside = false;
        }
    }
    return all_inside ? FrustumContainment::Inside : FrustumContainment::Intersects;
}

FrustumContainment HierarchicalCuller::test_sphere_containment(
    const core::domain::Frustum& frustum,
    const Point3D& center,
    double radius
) noexcept {
    bool all_inside = true;
    const Vector3<double> c{center.x, center.y, center.z};
    for (const auto& plane : frustum.planes) {
        const double d = plane.signed_distance(c);
        if (d < -radius) {
            return FrustumContainment::Outside;
        }
        if (d < radius) {
            all_inside = false;
        }
    }
    return all_inside ? FrustumContainment::Inside : FrustumContainment::Intersects;
}

std::vector<RhiDrawItem> HierarchicalCuller::cull_and_select_lod(
    const AssemblyCullNode& root,
    const core::domain::ViewportCamera& camera,
    CullingMetrics* out_metrics
) const {
    const auto frustum = camera.extract_frustum();
    const Point3D cam_pos{camera.position.x, camera.position.y, camera.position.z};

    CullingMetrics metrics{};
    std::vector<RhiDrawItem> items;

    cull_node(root, frustum, cam_pos, FrustumContainment::Intersects, items, metrics);

    if (out_metrics) {
        *out_metrics = metrics;
    }
    return items;
}

void HierarchicalCuller::cull_node(
    const AssemblyCullNode& node,
    const core::domain::Frustum& frustum,
    const Point3D& cam_pos,
    FrustumContainment parent_containment,
    std::vector<RhiDrawItem>& out_items,
    CullingMetrics& metrics
) const {
    metrics.nodes_tested++;

    FrustumContainment containment = parent_containment;
    if (containment != FrustumContainment::Inside) {
        containment = test_containment(frustum, node.world_bbox_min, node.world_bbox_max);
        if (containment == FrustumContainment::Outside) {
            metrics.nodes_culled++;
            return;
        }
    }

    // Process parts in this node
    for (const auto& part : node.parts) {
        if (!part.visible) continue;
        metrics.parts_tested++;

        const auto part_world_xf = node.world_transform * part.transform;

        if (containment != FrustumContainment::Inside) {
            // Compute part bounding box in world space
            const Point3D corners[8] = {
                {part.local_bbox_min.x, part.local_bbox_min.y, part.local_bbox_min.z},
                {part.local_bbox_max.x, part.local_bbox_min.y, part.local_bbox_min.z},
                {part.local_bbox_max.x, part.local_bbox_max.y, part.local_bbox_min.z},
                {part.local_bbox_min.x, part.local_bbox_max.y, part.local_bbox_min.z},
                {part.local_bbox_min.x, part.local_bbox_min.y, part.local_bbox_max.z},
                {part.local_bbox_max.x, part.local_bbox_min.y, part.local_bbox_max.z},
                {part.local_bbox_max.x, part.local_bbox_max.y, part.local_bbox_max.z},
                {part.local_bbox_min.x, part.local_bbox_max.y, part.local_bbox_max.z}
            };
            Point3D pmin{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
            Point3D pmax{-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
            for (const auto& c : corners) {
                const auto tp = transform_point(part_world_xf, c);
                pmin.x = std::min(pmin.x, tp.x);
                pmin.y = std::min(pmin.y, tp.y);
                pmin.z = std::min(pmin.z, tp.z);
                pmax.x = std::max(pmax.x, tp.x);
                pmax.y = std::max(pmax.y, tp.y);
                pmax.z = std::max(pmax.z, tp.z);
            }

            if (test_containment(frustum, pmin, pmax) == FrustumContainment::Outside) {
                metrics.parts_culled++;
                continue;
            }
        }

        // Distance-based LOD selection
        const Point3D part_world_pos = transform_point(part_world_xf, {0, 0, 0});
        const double distance = (part_world_pos - cam_pos).length();

        const int lod_idx = part.select_lod_index(distance);
        if (lod_idx < 0) {
            metrics.parts_culled++; // Beyond all LOD draw distances
            continue;
        }

        const SolidMesh3D* selected_mesh = part.select_lod(distance);
        if (!selected_mesh) {
            metrics.parts_culled++;
            continue;
        }

        metrics.parts_rendered++;
        if (static_cast<size_t>(lod_idx) < metrics.lod_counts.size()) {
            metrics.lod_counts[lod_idx]++;
        } else {
            metrics.lod_counts.back()++;
        }

        RhiDrawItem item;
        item.mesh = selected_mesh;
        item.model_matrix = part_world_xf;
        item.base_color = part.base_color;
        item.roughness = part.roughness;
        item.metallic = part.metallic;
        out_items.push_back(std::move(item));
    }

    // Traverse children hierarchically
    for (const auto& child : node.children) {
        if (child) {
            cull_node(*child, frustum, cam_pos, containment, out_items, metrics);
        }
    }
}

} // namespace shunya::cad3d
