#pragma once

#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/cad3d/trajectory_frame.hpp"

#include <vector>

namespace shunya::cad3d {

struct SweepParams {
    double scale_start{1.0};
    double scale_end{1.0};
    double twist_total_rad{0.0};
    bool closed_trajectory{false};
    Vec3d initial_up_hint{0.0, 0.0, 1.0};
};

/**
 * @brief Sweeps closed 2D profiles along 3D trajectories using Rotation-Minimizing Frames (RMF).
 */
class SweepEngine {
public:
    [[nodiscard]] static SolidMesh3D sweep(
        const cad2d::Path2D& profile_outer,
        const cad2d::Paths2D& profile_holes,
        const std::vector<Point3D>& trajectory_points,
        const SweepParams& params = {}
    );
};

} // namespace shunya::cad3d
