#pragma once

#include "shunya/core/base/math.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace shunya::cad3d {

using Vec3d = core::base::math::Vector3<double>;
using Point3D = core::base::math::Vector3<double>;

struct BBox3D {
    Point3D min{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    Point3D max{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    [[nodiscard]] bool is_valid() const noexcept;
    void expand(const Point3D& pt) noexcept;
    void expand(const BBox3D& other) noexcept;
    [[nodiscard]] Vec3d size() const noexcept;
};

struct ManifoldReport {
    size_t vertex_count{0};
    size_t edge_count{0};
    size_t face_count{0};
    int64_t euler_characteristic{0}; // V - E + F
    int boundary_edge_count{0};
    int non_manifold_edge_count{0};
    bool is_watertight{false};
    bool is_2_manifold{false};
};

/**
 * @brief Watertight-capable indexed 3D triangle mesh for B-Rep & solid feature modeling.
 */
class SolidMesh3D {
public:
    std::vector<Point3D> vertices;
    std::vector<std::array<uint32_t, 3>> triangles;

    uint32_t add_vertex(const Point3D& pt);
    void add_triangle(uint32_t i0, uint32_t i1, uint32_t i2);

    void clear() noexcept;

    /**
     * @brief Merges vertices within tol and drops degenerate zero-area/collapsed triangles.
     */
    void weld_vertices(double tol = 1e-9);

    /**
     * @brief Reverses triangle winding if net signed volume is negative so normals face outward.
     */
    void ensure_outward_winding();

    [[nodiscard]] double compute_signed_volume() const noexcept;
    [[nodiscard]] double compute_volume() const noexcept;
    [[nodiscard]] double compute_surface_area() const noexcept;
    [[nodiscard]] BBox3D compute_bbox() const noexcept;
    [[nodiscard]] Vec3d triangle_normal(size_t tri_idx) const noexcept;
    [[nodiscard]] ManifoldReport analyze_manifold() const;
};

} // namespace shunya::cad3d
