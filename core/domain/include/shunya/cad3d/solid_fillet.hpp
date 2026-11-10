#pragma once

#include "shunya/cad3d/polyhedral_brep.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace shunya::cad3d {

struct FilletParams {
    double radius{0.2};          // default fillet radius
    double radius_end{-1.0};     // if >= 0, varies linearly from radius -> radius_end along each edge
    size_t arc_segments{6};      // circular blend subdivisions per edge
    double min_angle_rad{0.15};  // minimum dihedral angle (~8.6 deg)
    std::vector<uint32_t> edge_indices; // empty = all sharp edges >= min_angle_rad
    std::function<double(uint32_t edge_id, double t)> variable_radius_fn; // optional radius(edge, t in [0,1])
};

/**
 * @brief Rolling-ball 3D edge filleting engine with constant & variable-radius blend patches and corner spheres.
 */
class SolidFilletEngine {
public:
    [[nodiscard]] static SolidMesh3D fillet(
        const SolidMesh3D& mesh,
        const FilletParams& params = {}
    );
};

} // namespace shunya::cad3d
