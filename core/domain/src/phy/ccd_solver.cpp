#include "shunya/phy/ccd_solver.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

using namespace core::domain::mechanics;

CcdResult CcdSolver::compute_toi(
    const ConvexCollider& shape_a,
    const MotionTransformSweep& sweep_a,
    const ConvexCollider& shape_b,
    const MotionTransformSweep& sweep_b,
    double tolerance) noexcept {

    const AABB swept_a = sweep_a.swept_aabb(shape_a, tolerance);
    const AABB swept_b = sweep_b.swept_aabb(shape_b, tolerance);
    if (!swept_a.intersects(swept_b)) {
        return {false, 1.0, {0, 1, 0}, {}};
    }

    const Vector3d ds_a = sweep_a.p1 - sweep_a.p0;
    const Vector3d ds_b = sweep_b.p1 - sweep_b.p0;
    const Vector3d ds_rel = ds_a - ds_b;

    auto rot_angle = [](const Quaterniond& q0, const Quaterniond& q1) -> double {
        const double d = std::clamp(std::abs(q0.w * q1.w + q0.x * q1.x + q0.y * q1.y + q0.z * q1.z), 0.0, 1.0);
        return 2.0 * std::acos(d);
    };
    const double ang_bound = rot_angle(sweep_a.q0, sweep_a.q1) * shape_a.max_bounding_radius() +
                             rot_angle(sweep_b.q0, sweep_b.q1) * shape_b.max_bounding_radius();

    double t = 0.0;
    for (int iter = 0; iter < kMaxCcdIterations; ++iter) {
        const ConvexCollider ca = sweep_a.interpolate(shape_a, t);
        const ConvexCollider cb = sweep_b.interpolate(shape_b, t);
        const GjkEpaResult res = GjkEpaSolver::detect(ca, cb);

        if (res.intersecting || res.distance <= tolerance) {
            return {true, t, res.normal, (res.point_a + res.point_b) * 0.5};
        }

        // res.normal points from B -> A, so closing speed of A relative to B is -dot(ds_rel, n)
        const double v_closing = -ds_rel.dot(res.normal) + ang_bound;
        if (v_closing <= 1e-12) {
            return {false, 1.0, res.normal, {}};
        }

        const double dt = std::max(1e-7, (res.distance - 0.5 * tolerance) / v_closing);
        t += dt;
        if (t > 1.0) {
            return {false, 1.0, res.normal, {}};
        }
    }

    const ConvexCollider ca = sweep_a.interpolate(shape_a, t);
    const ConvexCollider cb = sweep_b.interpolate(shape_b, t);
    const GjkEpaResult res = GjkEpaSolver::detect(ca, cb);
    if (res.intersecting || res.distance <= tolerance * 2.0) {
        return {true, t, res.normal, (res.point_a + res.point_b) * 0.5};
    }
    return {false, 1.0, res.normal, {}};
}

} // namespace shunya::phy
