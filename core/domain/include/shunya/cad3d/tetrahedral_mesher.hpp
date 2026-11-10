#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/cad3d/tet_quality.hpp"

namespace shunya::cad3d {

struct TetMeshParams {
    double interior_spacing{0.0}; // 0 = auto from bounding box diagonal / 6
    bool generate_interior_points{true};
};

/**
 * @brief Volumetric Tet4 finite-element mesher via 3D Bowyer-Watson Delaunay & BVH interior filtering.
 */
class TetrahedralMesher {
public:
    [[nodiscard]] static TetraMesh3D generate(
        const SolidMesh3D& boundary_mesh,
        const TetMeshParams& params = {}
    );
};

} // namespace shunya::cad3d
