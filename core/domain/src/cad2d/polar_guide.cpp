#include "shunya/cad2d/polar_guide.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

void PolarTrackingEngine::set_uniform_step_deg(double step_deg) {
    guide_angles_deg.clear();
    double step = std::clamp(step_deg, 1.0, 90.0);
    for (double a = 0.0; a < 360.0 - 1e-6; a += step) {
        guide_angles_deg.push_back(a);
    }
}

PolarGuideResult PolarTrackingEngine::evaluate(
    const Point2D& anchor,
    const Point2D& cursor_world
) const {
    PolarGuideResult out;
    out.origin = anchor;
    out.snapped_point = cursor_world;

    if (!enabled || guide_angles_deg.empty()) return out;

    Vec2d d = cursor_world - anchor;
    double dist = d.length();
    if (dist < 1e-12) return out;

    double angle_deg = std::atan2(d.y, d.x) * (180.0 / core::base::math::PI);
    while (angle_deg < 0.0) angle_deg += 360.0;

    double best_err = snap_threshold_deg + 1.0;
    double best_ang_deg = 0.0;
    bool found = false;

    for (double ga : guide_angles_deg) {
        double norm_ga = std::fmod(ga, 360.0);
        if (norm_ga < 0.0) norm_ga += 360.0;

        double diff = std::abs(angle_deg - norm_ga);
        if (diff > 180.0) diff = 360.0 - diff;

        if (diff <= snap_threshold_deg && diff < best_err) {
            best_err = diff;
            best_ang_deg = norm_ga;
            found = true;
        }
    }

    if (!found) return out;

    double rad = best_ang_deg * (core::base::math::PI / 180.0);
    Vec2d dir{std::cos(rad), std::sin(rad)};
    double proj_len = std::max(0.0, d.dot(dir));

    out.active = true;
    out.direction = dir;
    out.snapped_angle_deg = best_ang_deg;
    out.angular_error_deg = best_err;
    out.snapped_point = anchor + dir * proj_len;
    return out;
}

} // namespace shunya::cad2d
