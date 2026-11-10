#pragma once

#include "shunya/cad3d/polyhedral_brep.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <cstdint>
#include <vector>

namespace shunya::cad3d {

/**
 * @brief Generates thin-walled hollow shells and uniform/directional solid offsets.
 */
class ShellEngine {
public:
    /**
     * @brief Creates a watertight hollow shell with uniform wall_thickness and optional open faces removed.
     *
     * @param mesh Closed solid input mesh.
     * @param wall_thickness Positive thickness inward along face normals.
     * @param open_face_indices B-Rep face indices (from PolyhedralBRep::from_mesh) left open with rim walls.
     */
    [[nodiscard]] static SolidMesh3D shell(
        const SolidMesh3D& mesh,
        double wall_thickness,
        const std::vector<uint32_t>& open_face_indices = {}
    );

    /**
     * @brief Convenience helper opening all planar faces whose normal dots positively above min_normal_dot with target_normal.
     */
    [[nodiscard]] static SolidMesh3D shell_open_by_normal(
        const SolidMesh3D& mesh,
        double wall_thickness,
        const Vec3d& target_normal = {0.0, 0.0, 1.0},
        double min_normal_dot = 0.9
    );

    /**
     * @brief Uniformly offsets every B-Rep face of a closed solid outward (>0) or inward (<0).
     */
    [[nodiscard]] static SolidMesh3D offset_solid(
        const SolidMesh3D& mesh,
        double offset_distance
    );

    /**
     * @brief Computes per-vertex offset shifts by solving local incident-plane equations cleanly.
     */
    [[nodiscard]] static std::vector<Point3D> compute_offset_vertices(
        const PolyhedralBRep& brep,
        double outward_offset,
        const std::vector<uint32_t>& zero_offset_faces = {}
    );
};

} // namespace shunya::cad3d
