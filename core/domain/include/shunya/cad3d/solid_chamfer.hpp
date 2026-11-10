#pragma once

#include "shunya/cad3d/polyhedral_brep.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <cstdint>
#include <vector>

namespace shunya::cad3d {

struct ChamferParams {
    double distance{0.2};       // perpendicular bevel setback along adjacent faces
    double min_angle_rad{0.15}; // minimum dihedral angle for chamfer selection (~8.6 deg)
    std::vector<uint32_t> edge_indices; // empty = chamfer all sharp edges >= min_angle_rad
};

/**
 * @brief Generates planar/conical 3D chamfers across polyhedral B-Rep sharp edges and 3+ edge corners.
 */
class SolidChamferEngine {
public:
    [[nodiscard]] static SolidMesh3D chamfer(
        const SolidMesh3D& mesh,
        const ChamferParams& params = {}
    );
};

} // namespace shunya::cad3d
