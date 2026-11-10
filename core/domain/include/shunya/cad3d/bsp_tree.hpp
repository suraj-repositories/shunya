#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <memory>
#include <vector>

namespace shunya::cad3d {

struct Plane3D {
    Vec3d normal{0.0, 0.0, 1.0};
    double w{0.0}; // plane equation: normal . p = w

    Plane3D() = default;
    Plane3D(const Vec3d& n, double dist) noexcept : normal(n), w(dist) {}

    [[nodiscard]] static Plane3D from_points(const Point3D& a, const Point3D& b, const Point3D& c) noexcept;
    void flip() noexcept;
};

struct BspPolygon {
    std::vector<Point3D> vertices;
    Plane3D plane;

    BspPolygon() = default;
    explicit BspPolygon(std::vector<Point3D> verts);
    void flip() noexcept;
};

/**
 * @brief Binary Space Partitioning (BSP) tree for exact 3D solid CSG plane splitting & classification.
 */
class BspNode {
public:
    BspNode() = default;
    explicit BspNode(const std::vector<BspPolygon>& polygons);

    void invert();
    void clip_to(const BspNode& bsp);
    void build(const std::vector<BspPolygon>& polygons, int depth = 0);

    [[nodiscard]] std::vector<BspPolygon> clip_polygons(const std::vector<BspPolygon>& polygons) const;
    [[nodiscard]] std::vector<BspPolygon> all_polygons() const;

    [[nodiscard]] static std::vector<BspPolygon> mesh_to_polygons(const SolidMesh3D& mesh);
    [[nodiscard]] static SolidMesh3D polygons_to_mesh(const std::vector<BspPolygon>& polygons);

private:
    Plane3D plane_;
    bool has_plane_{false};
    std::vector<BspPolygon> coplanar_;
    std::unique_ptr<BspNode> front_;
    std::unique_ptr<BspNode> back_;
};

} // namespace shunya::cad3d
