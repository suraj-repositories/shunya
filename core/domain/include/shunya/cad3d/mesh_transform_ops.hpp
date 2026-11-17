#pragma once

#include "shunya/cad3d/mesh_selection_engine.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <vector>

namespace shunya::cad3d {

enum class TransformAxisLock {
    None,
    X,
    Y,
    Z,
    PlaneXY,
    PlaneXZ,
    PlaneYZ
};

enum class TransformPivot {
    MedianPoint,
    ActiveElement,
    BoundingBoxCenter,
    WorldOrigin
};

class MeshTransformOps {
public:
    /**
     * @brief Computes the pivot point for the given selection on the mesh.
     */
    [[nodiscard]] static Point3D compute_pivot(
        const SolidMesh3D& mesh,
        const MeshSelection& selection,
        TransformPivot pivot_mode = TransformPivot::MedianPoint,
        const Point3D& custom_pivot = {0.0, 0.0, 0.0}
    );

    /**
     * @brief Filters or constrains a 3D vector according to the axis-lock mode.
     */
    [[nodiscard]] static Vec3d apply_axis_lock(
        const Vec3d& v,
        TransformAxisLock axis_lock
    ) noexcept;

    /**
     * @brief Blender-style Grab/Translate (G) operation with axis-locking.
     */
    static void translate(
        SolidMesh3D& mesh,
        const MeshSelection& selection,
        const Vec3d& delta,
        TransformAxisLock axis_lock = TransformAxisLock::None
    );

    /**
     * @brief Blender-style Rotate (R) operation with axis-locking and pivot selection.
     */
    static void rotate(
        SolidMesh3D& mesh,
        const MeshSelection& selection,
        double angle_radians,
        TransformAxisLock axis_lock = TransformAxisLock::Z,
        const Vec3d& custom_axis = {0.0, 0.0, 1.0},
        TransformPivot pivot_mode = TransformPivot::MedianPoint,
        const Point3D& custom_pivot = {0.0, 0.0, 0.0}
    );

    /**
     * @brief Blender-style Scale (S) operation with axis-locking and pivot selection.
     */
    static void scale(
        SolidMesh3D& mesh,
        const MeshSelection& selection,
        const Vec3d& scale_factors,
        TransformAxisLock axis_lock = TransformAxisLock::None,
        TransformPivot pivot_mode = TransformPivot::MedianPoint,
        const Point3D& custom_pivot = {0.0, 0.0, 0.0}
    );
};

} // namespace shunya::cad3d
