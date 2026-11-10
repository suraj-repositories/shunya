#pragma once

#include "shunya/cad3d/half_edge_mesh.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <cstddef>

namespace shunya::cad3d {

/**
 * @brief Surface subdivision algorithms supporting Loop (triangle) and Catmull-Clark (quad/polygon) schemes.
 */
class SubdivisionEngine {
public:
    /**
     * @brief Performs Loop subdivision on a triangular HalfEdgeMesh with optional crease angle preservation.
     *
     * Edges whose adjacent face dihedral angle >= crease_angle_rad are treated as sharp creases.
     * Pass crease_angle_rad >= PI for fully smooth C2 subdivision everywhere.
     */
    [[nodiscard]] static HalfEdgeMesh subdivide_loop(
        const HalfEdgeMesh& mesh,
        size_t levels = 1,
        double crease_angle_rad = core::base::math::PI + 1.0
    );

    [[nodiscard]] static SolidMesh3D subdivide_loop(
        const SolidMesh3D& solid,
        size_t levels = 1,
        double crease_angle_rad = core::base::math::PI + 1.0
    );

    /**
     * @brief Performs Catmull-Clark quad/polygon subdivision on an arbitrary manifold HalfEdgeMesh.
     */
    [[nodiscard]] static HalfEdgeMesh subdivide_catmull_clark(
        const HalfEdgeMesh& mesh,
        size_t levels = 1,
        double crease_angle_rad = core::base::math::PI + 1.0
    );
};

} // namespace shunya::cad3d
