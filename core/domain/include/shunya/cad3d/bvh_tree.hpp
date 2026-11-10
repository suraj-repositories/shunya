#pragma once

#include "shunya/cad3d/raycast_query.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <cstdint>
#include <vector>

namespace shunya::cad3d {

struct LinearBvhNode {
    BBox3D bounds;
    uint32_t second_child_or_prim{0}; // leaf: start index in prim_indices; interior: right child node index
    uint16_t prim_count{0};           // 0 = interior node, > 0 = leaf
    uint8_t axis{0};
    uint8_t pad{0};
};

/**
 * @brief Surface Area Heuristic (SAH) Linear Bounding Volume Hierarchy for O(log N) 3D raycasting.
 */
class LinearBvh3D {
public:
    void build(const SolidMesh3D& mesh, uint32_t max_leaf_tris = 4);
    void clear() noexcept;

    [[nodiscard]] bool is_built() const noexcept { return !nodes_.empty(); }
    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }

    [[nodiscard]] RayHit3D raycast(
        const Ray3D& ray,
        double max_distance = 1e30
    ) const noexcept;

    [[nodiscard]] bool any_hit(
        const Ray3D& ray,
        double max_distance = 1e30
    ) const noexcept;

    /**
     * @brief Exact parity / odd-even ray test determining if query point is inside a closed watertight mesh.
     */
    [[nodiscard]] bool contains_point(const Point3D& pt) const noexcept;

    [[nodiscard]] std::vector<uint32_t> query_aabb(const BBox3D& query_box) const;

private:
    const SolidMesh3D* mesh_{nullptr};
    std::vector<LinearBvhNode> nodes_;
    std::vector<uint32_t> prim_indices_;
};

/**
 * @brief Adaptive 3D spatial Octree partitioning triangle indices for spatial range and ray queries.
 */
class Octree3D {
public:
    void build(const SolidMesh3D& mesh, uint32_t max_depth = 8, uint32_t max_leaf_tris = 8);
    [[nodiscard]] std::vector<uint32_t> query_aabb(const BBox3D& query_box) const;
    [[nodiscard]] RayHit3D raycast(const Ray3D& ray, double max_distance = 1e30) const;
    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }

private:
    struct OctNode {
        BBox3D bounds;
        uint32_t children[8]{0, 0, 0, 0, 0, 0, 0, 0};
        bool is_leaf{true};
        std::vector<uint32_t> tri_indices;
    };

    void insert_recursive(
        uint32_t node_idx,
        const std::vector<BBox3D>& tri_boxes,
        uint32_t depth,
        uint32_t max_depth,
        uint32_t max_leaf_tris
    );

    const SolidMesh3D* mesh_{nullptr};
    std::vector<OctNode> nodes_;
};

} // namespace shunya::cad3d
