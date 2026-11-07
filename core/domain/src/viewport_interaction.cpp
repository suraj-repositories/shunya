#include "shunya/core/domain/viewport_interaction.hpp"
#include <algorithm>
#include <numeric>

namespace shunya::core::domain {

namespace {

double point_to_segment_distance(
    const base::math::Vector2<double>& p,
    const base::math::Vector2<double>& a,
    const base::math::Vector2<double>& b
) {
    auto ab = b - a;
    double l2 = ab.x * ab.x + ab.y * ab.y;
    if (l2 < 1e-9) {
        auto d = p - a;
        return std::sqrt(d.x * d.x + d.y * d.y);
    }
    auto ap = p - a;
    double t = std::clamp((ap.x * ab.x + ap.y * ab.y) / l2, 0.0, 1.0);
    base::math::Vector2<double> proj{a.x + ab.x * t, a.y + ab.y * t};
    auto diff = p - proj;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
}

} // namespace

ScreenProjectedGizmo GizmoInteractionEngine::project_gizmo(
    const ViewportCamera& camera,
    const base::math::Vector3<double>& world_pos,
    double axis_length,
    double viewport_w,
    double viewport_h
) {
    ScreenProjectedGizmo out;
    if (viewport_w <= 0.0 || viewport_h <= 0.0) return out;

    auto vp_mat = camera.view_projection_matrix();

    auto project_point = [&](const base::math::Vector3<double>& p, base::math::Vector2<double>& out_screen) -> bool {
        double x = vp_mat.m[0] * p.x + vp_mat.m[1] * p.y + vp_mat.m[2] * p.z + vp_mat.m[3];
        double y = vp_mat.m[4] * p.x + vp_mat.m[5] * p.y + vp_mat.m[6] * p.z + vp_mat.m[7];
        double w = vp_mat.m[12] * p.x + vp_mat.m[13] * p.y + vp_mat.m[14] * p.z + vp_mat.m[15];

        if (w <= 1e-4) return false;

        double ndc_x = x / w;
        double ndc_y = y / w;

        out_screen.x = (ndc_x * 0.5 + 0.5) * viewport_w;
        out_screen.y = (1.0 - (ndc_y * 0.5 + 0.5)) * viewport_h;
        return true;
    };

    bool ok_orig = project_point(world_pos, out.origin_2d);
    bool ok_x = project_point(world_pos + base::math::Vector3<double>{axis_length, 0.0, 0.0}, out.x_tip_2d);
    bool ok_y = project_point(world_pos + base::math::Vector3<double>{0.0, axis_length, 0.0}, out.y_tip_2d);
    bool ok_z = project_point(world_pos + base::math::Vector3<double>{0.0, 0.0, axis_length}, out.z_tip_2d);

    out.is_behind_camera = !(ok_orig && ok_x && ok_y && ok_z);
    return out;
}

GizmoAxis GizmoInteractionEngine::pick_axis_2d(
    const ScreenProjectedGizmo& proj,
    double mouse_x,
    double mouse_y,
    double tolerance_px
) {
    if (proj.is_behind_camera) return GizmoAxis::None;

    base::math::Vector2<double> mouse{mouse_x, mouse_y};

    // Center handle check (all axes translation)
    auto d_orig = mouse - proj.origin_2d;
    double dist_center = std::sqrt(d_orig.x * d_orig.x + d_orig.y * d_orig.y);
    if (dist_center <= tolerance_px * 0.8) {
        return GizmoAxis::All;
    }

    double dist_x = point_to_segment_distance(mouse, proj.origin_2d, proj.x_tip_2d);
    double dist_y = point_to_segment_distance(mouse, proj.origin_2d, proj.y_tip_2d);
    double dist_z = point_to_segment_distance(mouse, proj.origin_2d, proj.z_tip_2d);

    double min_dist = tolerance_px;
    GizmoAxis best_axis = GizmoAxis::None;

    if (dist_x < min_dist) {
        min_dist = dist_x;
        best_axis = GizmoAxis::X;
    }
    if (dist_y < min_dist) {
        min_dist = dist_y;
        best_axis = GizmoAxis::Y;
    }
    if (dist_z < min_dist) {
        min_dist = dist_z;
        best_axis = GizmoAxis::Z;
    }

    return best_axis;
}

base::math::Vector3<double> GizmoInteractionEngine::calculate_translation_delta_2d(
    GizmoAxis axis,
    const ViewportCamera& camera,
    const ScreenProjectedGizmo& proj,
    double screen_dx,
    double screen_dy,
    double viewport_h,
    const SnappingConfig& snapping
) {
    if (axis == GizmoAxis::None || proj.is_behind_camera) return {0.0, 0.0, 0.0};

    // World-to-screen scale factor estimate
    double cam_dist = (camera.position - camera.target).length();
    double scale = (cam_dist * 2.0 * std::tan(camera.fov_degrees * 0.5 * (base::math::PI / 180.0))) / std::max(1.0, viewport_h);

    base::math::Vector2<double> mouse_drag{screen_dx, screen_dy};
    base::math::Vector3<double> delta{0.0, 0.0, 0.0};

    auto project_axis_delta = [&](const base::math::Vector2<double>& tip) -> double {
        auto axis_screen = tip - proj.origin_2d;
        double len = std::sqrt(axis_screen.x * axis_screen.x + axis_screen.y * axis_screen.y);
        if (len < 1e-4) return 0.0;
        base::math::Vector2<double> dir{axis_screen.x / len, axis_screen.y / len};
        double proj_len = (mouse_drag.x * dir.x + mouse_drag.y * dir.y);
        return proj_len * scale;
    };

    switch (axis) {
        case GizmoAxis::X: {
            double d = project_axis_delta(proj.x_tip_2d);
            delta.x = d;
            break;
        }
        case GizmoAxis::Y: {
            double d = project_axis_delta(proj.y_tip_2d);
            delta.y = -d; // Invert screen Y
            break;
        }
        case GizmoAxis::Z: {
            double d = project_axis_delta(proj.z_tip_2d);
            delta.z = d;
            break;
        }
        case GizmoAxis::All: {
            auto right = camera.right();
            auto up = camera.up_vector();
            delta = right * (screen_dx * scale) + up * (-screen_dy * scale);
            break;
        }
        default:
            break;
    }

    if (snapping.enabled) {
        delta.x = snapping.snap_value(delta.x, snapping.grid_step);
        delta.y = snapping.snap_value(delta.y, snapping.grid_step);
        delta.z = snapping.snap_value(delta.z, snapping.grid_step);
    }

    return delta;
}

double GizmoInteractionEngine::calculate_rotation_delta_2d(
    GizmoAxis axis,
    const ScreenProjectedGizmo& proj,
    double mouse_x,
    double mouse_y,
    double prev_mouse_x,
    double prev_mouse_y,
    const SnappingConfig& snapping
) {
    if (axis == GizmoAxis::None || proj.is_behind_camera) return 0.0;

    auto v_prev = base::math::Vector2<double>{prev_mouse_x, prev_mouse_y} - proj.origin_2d;
    auto v_curr = base::math::Vector2<double>{mouse_x, mouse_y} - proj.origin_2d;

    double angle_prev = std::atan2(v_prev.y, v_prev.x);
    double angle_curr = std::atan2(v_curr.y, v_curr.x);
    double delta_rad = angle_curr - angle_prev;

    // Normalize wrap-around
    while (delta_rad > base::math::PI) delta_rad -= 2.0 * base::math::PI;
    while (delta_rad < -base::math::PI) delta_rad += 2.0 * base::math::PI;

    if (snapping.enabled) {
        delta_rad = snapping.snap_angle_rad(delta_rad);
    }

    return delta_rad;
}

// =============================================================================
// ViewCube Implementation
// =============================================================================

std::string_view ViewCubeManager::face_to_string(CubeFace face) noexcept {
    switch (face) {
        case CubeFace::Top: return "Top";
        case CubeFace::Bottom: return "Bottom";
        case CubeFace::Front: return "Front";
        case CubeFace::Back: return "Back";
        case CubeFace::Left: return "Left";
        case CubeFace::Right: return "Right";
        case CubeFace::IsoTopFrontRight: return "IsoTopFrontRight";
        case CubeFace::IsoTopFrontLeft: return "IsoTopFrontLeft";
        case CubeFace::IsoTopBackRight: return "IsoTopBackRight";
        case CubeFace::IsoTopBackLeft: return "IsoTopBackLeft";
    }
    return "Front";
}

std::optional<CubeFace> ViewCubeManager::face_from_string(std::string_view str) noexcept {
    if (str == "Top" || str == "top") return CubeFace::Top;
    if (str == "Bottom" || str == "bottom") return CubeFace::Bottom;
    if (str == "Front" || str == "front") return CubeFace::Front;
    if (str == "Back" || str == "back") return CubeFace::Back;
    if (str == "Left" || str == "left") return CubeFace::Left;
    if (str == "Right" || str == "right") return CubeFace::Right;
    if (str == "IsoTopFrontRight" || str == "Isometric" || str == "iso") return CubeFace::IsoTopFrontRight;
    if (str == "IsoTopFrontLeft") return CubeFace::IsoTopFrontLeft;
    if (str == "IsoTopBackRight") return CubeFace::IsoTopBackRight;
    if (str == "IsoTopBackLeft") return CubeFace::IsoTopBackLeft;
    return std::nullopt;
}

base::math::Vector3<double> ViewCubeManager::get_camera_direction_for_face(CubeFace face) noexcept {
    switch (face) {
        case CubeFace::Top: return {0.0, 1.0, 0.0};
        case CubeFace::Bottom: return {0.0, -1.0, 0.0};
        case CubeFace::Front: return {0.0, 0.0, 1.0};
        case CubeFace::Back: return {0.0, 0.0, -1.0};
        case CubeFace::Left: return {-1.0, 0.0, 0.0};
        case CubeFace::Right: return {1.0, 0.0, 0.0};
        case CubeFace::IsoTopFrontRight: {
            base::math::Vector3<double> v{1.0, 1.0, 1.0};
            return v.normalized();
        }
        case CubeFace::IsoTopFrontLeft: {
            base::math::Vector3<double> v{-1.0, 1.0, 1.0};
            return v.normalized();
        }
        case CubeFace::IsoTopBackRight: {
            base::math::Vector3<double> v{1.0, 1.0, -1.0};
            return v.normalized();
        }
        case CubeFace::IsoTopBackLeft: {
            base::math::Vector3<double> v{-1.0, 1.0, -1.0};
            return v.normalized();
        }
    }
    return {0.0, 0.0, 1.0};
}

base::math::Vector3<double> ViewCubeManager::get_camera_up_for_face(CubeFace face) noexcept {
    switch (face) {
        case CubeFace::Top: return {0.0, 0.0, -1.0};
        case CubeFace::Bottom: return {0.0, 0.0, 1.0};
        default: return {0.0, 1.0, 0.0};
    }
}

ViewportCamera ViewCubeManager::create_camera_for_face(
    CubeFace face,
    double distance,
    const base::math::Vector3<double>& target,
    bool orthographic
) {
    ViewportCamera cam;
    cam.target = target;
    auto dir = get_camera_direction_for_face(face);
    cam.position = target + dir * distance;
    cam.up = get_camera_up_for_face(face);
    cam.projection_type = orthographic ? CameraProjectionType::Orthographic : CameraProjectionType::Perspective;
    cam.ortho_size = distance;
    return cam;
}

ViewportCamera ViewCubeManager::interpolate_camera(
    const ViewportCamera& start,
    const ViewportCamera& end,
    double t
) {
    t = std::clamp(t, 0.0, 1.0);
    // Smoothstep interpolation
    double s = t * t * (3.0 - 2.0 * t);

    ViewportCamera out = end;
    out.position = start.position + (end.position - start.position) * s;
    out.target = start.target + (end.target - start.target) * s;
    out.up = (start.up + (end.up - start.up) * s).normalized();
    out.fov_degrees = start.fov_degrees + (end.fov_degrees - start.fov_degrees) * s;
    out.ortho_size = start.ortho_size + (end.ortho_size - start.ortho_size) * s;
    return out;
}

// =============================================================================
// DiagnosticsTelemetryTracker Implementation
// =============================================================================

DiagnosticsTelemetryTracker::DiagnosticsTelemetryTracker(size_t capacity)
    : capacity_(std::max(size_t{10}, capacity))
{
}

void DiagnosticsTelemetryTracker::record_frame(double delta_time_s) {
    if (delta_time_s <= 0.0) return;
    frame_times_s_.push_back(delta_time_s);
    frame_time_sum_s_ += delta_time_s;

    if (frame_times_s_.size() > capacity_) {
        frame_time_sum_s_ -= frame_times_s_.front();
        frame_times_s_.pop_front();
    }
}

void DiagnosticsTelemetryTracker::record_solver_step(double step_time_ms, double residual, uint32_t iterations) {
    last_solver_step_ms_ = step_time_ms;
    last_residual_ = residual;
    last_iterations_ = iterations;
}

double DiagnosticsTelemetryTracker::current_fps() const noexcept {
    if (frame_times_s_.empty() || frame_time_sum_s_ <= 1e-9) return 60.0;
    double avg_dt = frame_time_sum_s_ / static_cast<double>(frame_times_s_.size());
    return 1.0 / avg_dt;
}

double DiagnosticsTelemetryTracker::average_frame_time_ms() const noexcept {
    if (frame_times_s_.empty()) return 16.6;
    return (frame_time_sum_s_ / static_cast<double>(frame_times_s_.size())) * 1000.0;
}

void DiagnosticsTelemetryTracker::reset() noexcept {
    frame_times_s_.clear();
    frame_time_sum_s_ = 0.0;
    last_solver_step_ms_ = 0.0;
    last_residual_ = 0.0;
    last_iterations_ = 0;
}

} // namespace shunya::core::domain
