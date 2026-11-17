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

    [[nodiscard]] static SolidMesh3D boolean_union(
        const SolidMesh3D& solid_a,
        const SolidMesh3D& solid_b
    ) {
        return execute(CsgOp::Union, solid_a, solid_b);
    }

    [[nodiscard]] static SolidMesh3D boolean_difference(
        const SolidMesh3D& solid_a,
        const SolidMesh3D& solid_b
    ) {
        return execute(CsgOp::Subtract, solid_a, solid_b);
    }

    [[nodiscard]] static SolidMesh3D boolean_intersection(
        const SolidMesh3D& solid_a,
        const SolidMesh3D& solid_b
    ) {
        return execute(CsgOp::Intersect, solid_a, solid_b);
    }

    [[nodiscard]] static SolidMesh3D boolean_union_all(
        const std::vector<SolidMesh3D>& solids
    );

    /**
     * @brief Repairs T-junctions along boundary edges so adjacent split triangles share identical vertices.
     */
    static void repair_tjunctions(SolidMesh3D& mesh, double tol = 1e-6);
};

} // namespace shunya::cad3d
