#pragma once

#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <vector>

namespace shunya::cad3d {

struct LoftSection {
    cad2d::Path2D profile_2d;
    SketchPlane3D plane{SketchPlane3D::xy()};
};

struct LoftParams {
    size_t samples_per_section{64};
    bool cap_ends{true};
    bool closed_loft{false};
};

/**
 * @brief Lofts across multiple planar 3D cross-sections via arc-length resampling & minimal-twist alignment.
 */
class LoftEngine {
public:
    [[nodiscard]] static SolidMesh3D loft(
        const std::vector<LoftSection>& sections,
        const LoftParams& params = {}
    );
};

} // namespace shunya::cad3d
