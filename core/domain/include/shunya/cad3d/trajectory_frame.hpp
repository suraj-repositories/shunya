#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <vector>

namespace shunya::cad3d {

/**
 * @brief Orthonormal frame (U=normal, V=binormal, T=tangent) along a 3D trajectory with U x V = T.
 */
struct TrajectoryFrame {
    Point3D position{0.0, 0.0, 0.0};
    Vec3d tangent{0.0, 0.0, 1.0};
    Vec3d normal{1.0, 0.0, 0.0};
    Vec3d binormal{0.0, 1.0, 0.0};
    double arc_fraction{0.0};
};

class TrajectoryCurve3D {
public:
    [[nodiscard]] static std::vector<Point3D> sample_cubic_bezier(
        const Point3D& p0,
        const Point3D& p1,
        const Point3D& p2,
        const Point3D& p3,
        size_t samples = 48
    );

    [[nodiscard]] static std::vector<Point3D> sample_helix(
        double radius,
        double pitch,
        double turns,
        size_t samples = 64
    );

    [[nodiscard]] static std::vector<Point3D> resample_polyline(
        const std::vector<Point3D>& points,
        size_t target_count
    );
};

/**
 * @brief Computes twist-free Rotation-Minimizing Frames (Wang et al. 2008 Double Reflection).
 */
class RotationMinimizingFrame {
public:
    [[nodiscard]] static std::vector<TrajectoryFrame> compute_double_reflection(
        const std::vector<Point3D>& path_points,
        bool closed_loop = false,
        const Vec3d& initial_up_hint = {0.0, 0.0, 1.0}
    );
};

} // namespace shunya::cad3d
