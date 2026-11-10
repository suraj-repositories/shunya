#pragma once

#include "shunya/cad3d/bsp_tree.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

namespace shunya::cad3d {

enum class CsgOp {
    Union,
    Subtract,
    Intersect
};

/**
 * @brief Constructive Solid Geometry (CSG) 3D Boolean engine over closed SolidMesh3D bodies.
 */
class CsgBooleanEngine {
public:
    [[nodiscard]] static SolidMesh3D execute(
        CsgOp op,
        const SolidMesh3D& solid_a,
        const SolidMesh3D& solid_b
    );

    /**
     * @brief Repairs T-junctions along boundary edges so adjacent split triangles share identical vertices.
     */
    static void repair_tjunctions(SolidMesh3D& mesh, double tol = 1e-6);
};

} // namespace shunya::cad3d
