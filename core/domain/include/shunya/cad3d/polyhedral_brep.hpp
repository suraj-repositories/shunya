#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <cstdint>
#include <vector>

namespace shunya::cad3d {

struct BrepFace {
    uint32_t id{0};
    Vec3d normal{0.0, 0.0, 1.0};
    Point3D origin{0.0, 0.0, 0.0};
    Vec3d axis_u{1.0, 0.0, 0.0};
    Vec3d axis_v{0.0, 1.0, 0.0};
    std::vector<uint32_t> outer_loop; // Ordered CCW vertex indices when viewed from +normal
    std::vector<size_t> source_triangles;
};

struct BrepEdge {
    uint32_t id{0};
    uint32_t v0{0}; // directed along face_left
    uint32_t v1{0};
    uint32_t face_left{0};
    uint32_t face_right{0};
    double dihedral_angle_rad{0.0}; // angle between outward normals [0, PI]
};

/**
 * @brief Lightweight polyhedral B-Rep extracted from a closed SolidMesh3D via coplanar face merging.
 */
struct PolyhedralBRep {
    std::vector<Point3D> vertices;
    std::vector<BrepFace> faces;
    std::vector<BrepEdge> edges;

    [[nodiscard]] static PolyhedralBRep from_mesh(
        const SolidMesh3D& mesh,
        double coplanar_angle_tol_rad = 0.015
    );
};

} // namespace shunya::cad3d
