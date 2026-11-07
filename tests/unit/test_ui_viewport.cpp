#include "tests/test_framework.hpp"

#include "shunya/core/domain/viewport_interaction.hpp"
#include "shunya/core/domain/rendering.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>

using namespace shunya::core;
using namespace shunya::core::domain;

namespace {
bool is_approx(double a, double b, double eps = 1e-3) {
    return std::abs(a - b) <= eps;
}
} // namespace

// =============================================================================
// Task UI-03-001-01: 3D Transform Gizmos & Coordinate Snapping
// =============================================================================

TEST_CASE("UI Viewport: Coordinate Snapping Rounding & Invariance") {
    SnappingConfig config;
    config.enabled = true;
    config.grid_step = 0.1;
    config.angle_step_deg = 15.0;
    config.scale_step = 0.25;

    // Translation snapping
    CHECK(is_approx(config.snap_value(0.24, config.grid_step), 0.20));
    CHECK(is_approx(config.snap_value(0.26, config.grid_step), 0.30));
    CHECK(is_approx(config.snap_value(-0.14, config.grid_step), -0.10));

    auto pos = config.snap_position({0.12, 0.49, -0.98});
    CHECK(is_approx(pos.x, 0.10));
    CHECK(is_approx(pos.y, 0.50));
    CHECK(is_approx(pos.z, -1.00));

    // Rotation snapping (15 degrees = ~0.2618 rad)
    double fifteen_deg_rad = 15.0 * (base::math::PI / 180.0);
    double seventeen_deg_rad = 17.0 * (base::math::PI / 180.0);
    CHECK(is_approx(config.snap_angle_rad(seventeen_deg_rad), fifteen_deg_rad));

    // Disabled snapping returns exact values
    config.enabled = false;
    CHECK(is_approx(config.snap_value(0.24, config.grid_step), 0.24));
    CHECK(is_approx(config.snap_angle_rad(seventeen_deg_rad), seventeen_deg_rad));
}

TEST_CASE("UI Viewport: Gizmo Screen Projections and 2D Axis Picking") {
    ViewportCamera camera;
    camera.position = {0.0, 0.0, 5.0};
    camera.target = {0.0, 0.0, 0.0};
    camera.up = {0.0, 1.0, 0.0};
    camera.fov_degrees = 60.0;

    double vp_w = 800.0;
    double vp_h = 600.0;
    base::math::Vector3<double> origin{0.0, 0.0, 0.0};

    auto proj = GizmoInteractionEngine::project_gizmo(camera, origin, 1.0, vp_w, vp_h);
    REQUIRE(!proj.is_behind_camera);

    // Screen origin should be at the center (400, 300)
    CHECK(is_approx(proj.origin_2d.x, 400.0, 1.0));
    CHECK(is_approx(proj.origin_2d.y, 300.0, 1.0));

    // X axis tip should project to the right of origin
    CHECK(proj.x_tip_2d.x > proj.origin_2d.x);
    // Y axis tip should project above origin (smaller screen y)
    CHECK(proj.y_tip_2d.y < proj.origin_2d.y);

    // Hit Testing
    // Center handle click
    auto axis_center = GizmoInteractionEngine::pick_axis_2d(proj, proj.origin_2d.x, proj.origin_2d.y, 14.0);
    CHECK(axis_center == GizmoAxis::All);

    // Click on X axis midpoint
    double x_mid_x = (proj.origin_2d.x + proj.x_tip_2d.x) * 0.5;
    double x_mid_y = (proj.origin_2d.y + proj.x_tip_2d.y) * 0.5;
    auto axis_x = GizmoInteractionEngine::pick_axis_2d(proj, x_mid_x, x_mid_y, 14.0);
    CHECK(axis_x == GizmoAxis::X);

    // Click on Y axis midpoint
    double y_mid_x = (proj.origin_2d.x + proj.y_tip_2d.x) * 0.5;
    double y_mid_y = (proj.origin_2d.y + proj.y_tip_2d.y) * 0.5;
    auto axis_y = GizmoInteractionEngine::pick_axis_2d(proj, y_mid_x, y_mid_y, 14.0);
    CHECK(axis_y == GizmoAxis::Y);

    // Click far away
    auto axis_none = GizmoInteractionEngine::pick_axis_2d(proj, 50.0, 50.0, 14.0);
    CHECK(axis_none == GizmoAxis::None);
}

TEST_CASE("UI Viewport: Gizmo Translation & Rotation Delta Calculation") {
    ViewportCamera camera;
    camera.position = {0.0, 0.0, 5.0};
    camera.target = {0.0, 0.0, 0.0};
    camera.up = {0.0, 1.0, 0.0};
    camera.fov_degrees = 60.0;

    double vp_w = 800.0;
    double vp_h = 600.0;
    auto proj = GizmoInteractionEngine::project_gizmo(camera, {0, 0, 0}, 1.0, vp_w, vp_h);

    SnappingConfig snap;
    snap.enabled = true;
    snap.grid_step = 0.5;

    // Translation along X
    auto delta_x = GizmoInteractionEngine::calculate_translation_delta_2d(
        GizmoAxis::X, camera, proj, 50.0, 0.0, vp_h, snap
    );
    CHECK(delta_x.x > 0.0);
    CHECK(delta_x.y == 0.0);
    CHECK(delta_x.z == 0.0);
    // Assert snapped to multiple of 0.5
    CHECK(is_approx(std::fmod(delta_x.x, 0.5), 0.0));

    // Rotation around center
    snap.enabled = false;
    double delta_rot = GizmoInteractionEngine::calculate_rotation_delta_2d(
        GizmoAxis::Z, proj, 450.0, 300.0, 400.0, 250.0, snap
    );
    CHECK(std::abs(delta_rot) > 0.0);
}

// =============================================================================
// Task UI-03-001-02: Viewport Camera Orientation ViewCube & Ortho/Persp
// =============================================================================

TEST_CASE("UI Viewport: ViewCube Manager Face Directions and Camera Presets") {
    // Face String round-trips
    CHECK(ViewCubeManager::face_to_string(CubeFace::Top) == "Top");
    CHECK(ViewCubeManager::face_from_string("Front").value() == CubeFace::Front);
    CHECK(ViewCubeManager::face_from_string("Isometric").value() == CubeFace::IsoTopFrontRight);

    // Top view camera: look from (0, distance, 0) down to (0, 0, 0)
    auto top_cam = ViewCubeManager::create_camera_for_face(CubeFace::Top, 10.0, {0, 0, 0}, true);
    CHECK(top_cam.projection_type == CameraProjectionType::Orthographic);
    CHECK(is_approx(top_cam.position.x, 0.0));
    CHECK(is_approx(top_cam.position.y, 10.0));
    CHECK(is_approx(top_cam.position.z, 0.0));
    CHECK(is_approx(top_cam.up.z, -1.0));

    // Front view camera: look from (0, 0, distance) to (0, 0, 0)
    auto front_cam = ViewCubeManager::create_camera_for_face(CubeFace::Front, 8.0, {0, 0, 0}, false);
    CHECK(front_cam.projection_type == CameraProjectionType::Perspective);
    CHECK(is_approx(front_cam.position.z, 8.0));
    CHECK(is_approx(front_cam.up.y, 1.0));

    // Camera Interpolation at boundary and midpoint
    auto mid_cam = ViewCubeManager::interpolate_camera(top_cam, front_cam, 0.5);
    CHECK(mid_cam.position.y > 0.0);
    CHECK(mid_cam.position.z > 0.0);

    auto start_cam = ViewCubeManager::interpolate_camera(top_cam, front_cam, 0.0);
    CHECK(is_approx(start_cam.position.y, top_cam.position.y));

    auto end_cam = ViewCubeManager::interpolate_camera(top_cam, front_cam, 1.0);
    CHECK(is_approx(end_cam.position.z, front_cam.position.z));
}

// =============================================================================
// Task UI-03-001-03: Viewport HUD Telemetry Tracking
// =============================================================================

TEST_CASE("UI Viewport: Diagnostics Telemetry Rolling Metrics Calculation") {
    DiagnosticsTelemetryTracker tracker(60);

    // Empty tracker returns sensible default
    CHECK(tracker.current_fps() == 60.0);

    // Record 60 frames at 16.666 ms (60 FPS)
    for (int i = 0; i < 60; ++i) {
        tracker.record_frame(0.016666);
    }
    CHECK(is_approx(tracker.current_fps(), 60.0, 0.5));
    CHECK(is_approx(tracker.average_frame_time_ms(), 16.666, 0.1));

    // Record solver diagnostics
    tracker.record_solver_step(3.25, 1.45e-8, 15);
    CHECK(is_approx(tracker.last_solver_step_time_ms(), 3.25));
    CHECK(is_approx(tracker.last_solver_residual(), 1.45e-8));
    CHECK(tracker.last_solver_iterations() == 15);

    // Reset clears state cleanly
    tracker.reset();
    CHECK(tracker.current_fps() == 60.0);
    CHECK(tracker.last_solver_iterations() == 0);
}
