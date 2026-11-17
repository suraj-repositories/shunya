#include "shunya/cad2d/polar_guide.hpp"
#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::cad2d {

namespace {

using shunya::core::base::math::PI;

[[nodiscard]] std::string format_hud(double angle_deg, double dist_val) {
    std::ostringstream ss;
    ss << "< " << std::fixed << std::setprecision(1) << angle_deg << "°, L = "
       << std::setprecision(3) << dist_val;
    return ss.str();
}

} // namespace

PolarTrackingEngine::PolarTrackingEngine() {
    set_preset(PolarIncrementPreset::Deg30);
}

void PolarTrackingEngine::set_preset(PolarIncrementPreset preset) {
    current_preset = preset;
    switch (preset) {
        case PolarIncrementPreset::Deg15:
            set_uniform_step_deg(15.0);
            break;
        case PolarIncrementPreset::Deg30:
            set_uniform_step_deg(30.0);
            break;
        case PolarIncrementPreset::Deg45:
            set_uniform_step_deg(45.0);
            break;
        case PolarIncrementPreset::Deg90:
            set_uniform_step_deg(90.0);
            break;
        case PolarIncrementPreset::Custom:
            rebuild_guide_angles();
            break;
    }
}

void PolarTrackingEngine::set_uniform_step_deg(double step_deg) {
    guide_angles_deg.clear();
    double step = std::clamp(step_deg, 1.0, 90.0);
    for (double a = 0.0; a < 360.0 - 1e-6; a += step) {
        guide_angles_deg.push_back(a);
    }
    rebuild_guide_angles();
}

void PolarTrackingEngine::add_custom_angle(double angle_deg) {
    double norm = std::fmod(angle_deg, 360.0);
    if (norm < 0.0) norm += 360.0;
    additional_custom_angles_deg.push_back(norm);
    rebuild_guide_angles();
}

void PolarTrackingEngine::clear_custom_angles() noexcept {
    additional_custom_angles_deg.clear();
    rebuild_guide_angles();
}

void PolarTrackingEngine::rebuild_guide_angles() {
    // Sort and remove duplicates
    std::vector<double> combined = guide_angles_deg;
    combined.insert(combined.end(), additional_custom_angles_deg.begin(), additional_custom_angles_deg.end());

    std::sort(combined.begin(), combined.end());
    auto it = std::unique(combined.begin(), combined.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-4;
    });
    combined.erase(it, combined.end());
    guide_angles_deg = std::move(combined);
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
    out.distance = dist;
    if (dist < 1e-12) return out;

    double base_angle_deg = track_relative_to_segment ? reference_orientation_deg : 0.0;

    double cursor_angle_deg = std::atan2(d.y, d.x) * (180.0 / PI);
    while (cursor_angle_deg < 0.0) cursor_angle_deg += 360.0;

    double rel_cursor_angle = cursor_angle_deg - base_angle_deg;
    while (rel_cursor_angle < 0.0) rel_cursor_angle += 360.0;
    while (rel_cursor_angle >= 360.0) rel_cursor_angle -= 360.0;

    double best_err = snap_threshold_deg + 1.0;
    double best_ang_deg = 0.0;
    bool found = false;

    for (double ga : guide_angles_deg) {
        double norm_ga = std::fmod(ga, 360.0);
        if (norm_ga < 0.0) norm_ga += 360.0;

        double diff = std::abs(rel_cursor_angle - norm_ga);
        if (diff > 180.0) diff = 360.0 - diff;

        if (diff <= snap_threshold_deg && diff < best_err) {
            best_err = diff;
            best_ang_deg = norm_ga;
            found = true;
        }
    }

    if (!found) return out;

    double final_angle_deg = best_ang_deg + base_angle_deg;
    double rad = final_angle_deg * (PI / 180.0);
    Vec2d dir{std::cos(rad), std::sin(rad)};
    double proj_len = std::max(0.0, d.dot(dir));

    out.active = true;
    out.direction = dir;
    out.distance = proj_len;
    out.snapped_angle_deg = final_angle_deg;
    out.angular_error_deg = best_err;
    out.snapped_point = anchor + dir * proj_len;
    out.hud_tooltip = format_hud(final_angle_deg, proj_len);
    return out;
}

LineSegment2D PolarTrackingEngine::generate_visual_guide_ray(
    const PolarGuideResult& result,
    double ray_length
) {
    if (!result.active) return LineSegment2D(result.origin, result.origin);
    return LineSegment2D(result.origin, result.origin + result.direction * ray_length);
}

} // namespace shunya::cad2d
