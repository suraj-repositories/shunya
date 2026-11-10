#pragma once

#include "shunya/cad3d/qem_quadric.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <cstdint>

namespace shunya::cad3d {

struct DecimationParams {
    double target_ratio{0.5};           // used if target_triangle_count == 0
    size_t target_triangle_count{0};    // > 0 overrides target_ratio
    double boundary_weight{100.0};      // penalty plane multiplier along open boundary edges
    double min_normal_dot{0.2};         // guard rejecting edge collapses that flip/fold triangle normals
};

struct DecimationStats {
    size_t input_triangles{0};
    size_t output_triangles{0};
    size_t collapses_performed{0};
    double max_quadric_error{0.0};
};

/**
 * @brief Garland-Heckbert Quadric Error Metric (QEM) progressive mesh simplifier.
 */
class MeshDecimator {
public:
    [[nodiscard]] static SolidMesh3D simplify(
        const SolidMesh3D& mesh,
        const DecimationParams& params = {},
        DecimationStats* out_stats = nullptr
    );
};

} // namespace shunya::cad3d
