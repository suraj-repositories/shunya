#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace shunya::rig3d {

using namespace cad3d;

/**
 * @brief Generator for polar-singularity-free Quad Spheres (Cube Sphere).
 *
 * Subdivides the 6 faces of a cube into an N x N grid of quads and normalizes
 * all vertices onto a sphere of radius R, ensuring uniform quad density across the surface.
 */
class QuadSphereBuilder {
public:
    struct Params {
        double radius{1.0};
        uint32_t subdivisions{4};         ///< Subdivisions per cube edge (e.g. 1..16)
        Point3D center{0.0, 0.0, 0.0};
    };

    [[nodiscard]] static SolidMesh3D build();
    [[nodiscard]] static SolidMesh3D build(const Params& params);
};

/**
 * @brief Generator for geodesic Icospheres with multi-resolution levels.
 *
 * Begins with a regular icosahedron (12 vertices, 20 equilateral faces, golden ratio)
 * and iteratively subdivides each triangle into 4 smaller triangles, projecting
 * midpoints onto the sphere surface of radius R.
 */
class IcosphereBuilder {
public:
    struct Params {
        double radius{1.0};
        uint32_t resolution_level{2};     ///< Geodesic subdivision level (0..6)
        Point3D center{0.0, 0.0, 0.0};
    };

    [[nodiscard]] static SolidMesh3D build();
    [[nodiscard]] static SolidMesh3D build(const Params& params);
};

} // namespace shunya::rig3d
