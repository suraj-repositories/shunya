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

    /**
     * @brief Guides a 2D profile along an arbitrary 3D cubic Bezier spline curve.
     */
    [[nodiscard]] static SolidMesh3D sweep_spline(
        const cad2d::Path2D& profile_outer,
        const Point3D& p0,
        const Point3D& p1,
        const Point3D& p2,
        const Point3D& p3,
        const cad2d::Paths2D& profile_holes = {},
        size_t curve_samples = 48,
        const SweepParams& params = {}
    );
};

} // namespace shunya::cad3d
