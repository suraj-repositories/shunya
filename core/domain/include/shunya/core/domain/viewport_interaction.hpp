#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/rendering.hpp"

#include <cmath>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <deque>

namespace shunya::core::domain {

// =============================================================================
// Snapping Configuration & Helpers
// =============================================================================

struct SnappingConfig {
    bool enabled = false;
    double grid_step = 0.1;           // In world length units (e.g. 0.1m)
    double angle_step_deg = 15.0;     // In degrees (e.g. 15 degrees)
    double scale_step = 0.1;          // In fractional scale increments

    [[nodiscard]] double snap_value(double val, double step) const noexcept {
        if (!enabled || step <= 1e-9) return val;
        return std::round(val / step) * step;
    }

    [[nodiscard]] base::math::Vector3<double> snap_position(const base::math::Vector3<double>& pos) const noexcept {
        if (!enabled || grid_step <= 1e-9) return pos;
        return {
            snap_value(pos.x, grid_step),
            snap_value(pos.y, grid_step),
            snap_value(pos.z, grid_step)
        };
    }

    [[nodiscard]] double snap_angle_rad(double angle_rad) const noexcept {
        if (!enabled || angle_step_deg <= 1e-9) return angle_rad;
        double step_rad = angle_step_deg * (base::math::PI / 180.0);
        return std::round(angle_rad / step_rad) * step_rad;
    }
};

// =============================================================================
// Transform Gizmo Screen Projections & Picking
// =============================================================================

struct ScreenProjectedGizmo {
    base::math::Vector2<double> origin_2d{0.0, 0.0};
    base::math::Vector2<double> x_tip_2d{0.0, 0.0};
    base::math::Vector2<double> y_tip_2d{0.0, 0.0};
    base::math::Vector2<double> z_tip_2d{0.0, 0.0};
    bool is_behind_camera = false;
};

class GizmoInteractionEngine {
public:
    [[nodiscard]] static ScreenProjectedGizmo project_gizmo(
        const ViewportCamera& camera,
        const base::math::Vector3<double>& world_pos,
        double axis_length,
        double viewport_w,
        double viewport_h
    );

    [[nodiscard]] static GizmoAxis pick_axis_2d(
        const ScreenProjectedGizmo& proj,
        double mouse_x,
        double mouse_y,
        double tolerance_px = 14.0
    );

    [[nodiscard]] static base::math::Vector3<double> calculate_translation_delta_2d(
        GizmoAxis axis,
        const ViewportCamera& camera,
        const ScreenProjectedGizmo& proj,
        double screen_dx,
        double screen_dy,
        double viewport_h,
        const SnappingConfig& snapping
    );

    [[nodiscard]] static double calculate_rotation_delta_2d(
        GizmoAxis axis,
        const ScreenProjectedGizmo& proj,
        double mouse_x,
        double mouse_y,
        double prev_mouse_x,
        double prev_mouse_y,
        const SnappingConfig& snapping
    );
};

// =============================================================================
// ViewCube Camera Orientation & Orbit Interpolation
// =============================================================================

enum class CubeFace {
    Top,
    Bottom,
    Front,
    Back,
    Left,
    Right,
    IsoTopFrontRight,
    IsoTopFrontLeft,
    IsoTopBackRight,
    IsoTopBackLeft
};

class ViewCubeManager {
public:
    [[nodiscard]] static std::string_view face_to_string(CubeFace face) noexcept;
    [[nodiscard]] static std::optional<CubeFace> face_from_string(std::string_view str) noexcept;

    [[nodiscard]] static base::math::Vector3<double> get_camera_direction_for_face(CubeFace face) noexcept;
    [[nodiscard]] static base::math::Vector3<double> get_camera_up_for_face(CubeFace face) noexcept;

    [[nodiscard]] static ViewportCamera create_camera_for_face(
        CubeFace face,
        double distance = 5.0,
        const base::math::Vector3<double>& target = {0.0, 0.0, 0.0},
        bool orthographic = false
    );

    [[nodiscard]] static ViewportCamera interpolate_camera(
        const ViewportCamera& start,
        const ViewportCamera& end,
        double t
    );
};

// =============================================================================
// Viewport Diagnostics & Telemetry Tracker
// =============================================================================

class DiagnosticsTelemetryTracker {
public:
    explicit DiagnosticsTelemetryTracker(size_t history_capacity = 60);

    void record_frame(double delta_time_s);
    void record_solver_step(double step_time_ms, double residual, uint32_t iterations);

    [[nodiscard]] double current_fps() const noexcept;
    [[nodiscard]] double average_frame_time_ms() const noexcept;
    [[nodiscard]] double last_solver_step_time_ms() const noexcept { return last_solver_step_ms_; }
    [[nodiscard]] double last_solver_residual() const noexcept { return last_residual_; }
    [[nodiscard]] uint32_t last_solver_iterations() const noexcept { return last_iterations_; }

    void reset() noexcept;

private:
    size_t capacity_ = 60;
    std::deque<double> frame_times_s_;
    double frame_time_sum_s_ = 0.0;
    double last_solver_step_ms_ = 0.0;
    double last_residual_ = 0.0;
    uint32_t last_iterations_ = 0;
};

} // namespace shunya::core::domain
