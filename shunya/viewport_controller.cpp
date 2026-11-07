#include "viewport_controller.hpp"
#include <cmath>

namespace shunya::gui {

ShunyaViewportController::ShunyaViewportController(QObject* parent)
    : QObject(parent)
{
    // Initialize default camera to isometric perspective view
    camera_ = core::domain::ViewCubeManager::create_camera_for_face(
        core::domain::CubeFace::IsoTopFrontRight,
        8.0,
        {0.0, 0.0, 0.0},
        false
    );
    current_view_ = "Isometric";

    // Setup initial telemetry baseline
    for (int i = 0; i < 60; ++i) {
        telemetry_.record_frame(0.01666);
    }
    telemetry_.record_solver_step(2.4, 1.42e-7, 12);
}

void ShunyaViewportController::setGizmoMode(const QString& mode) {
    if (gizmo_mode_ != mode) {
        gizmo_mode_ = mode;
        emit gizmoModeChanged();
    }
}

void ShunyaViewportController::setSnapEnabled(bool enabled) {
    if (snapping_.enabled != enabled) {
        snapping_.enabled = enabled;
        emit snapConfigChanged();
    }
}

void ShunyaViewportController::setGridStep(double step) {
    if (step > 1e-4 && snapping_.grid_step != step) {
        snapping_.grid_step = step;
        emit snapConfigChanged();
    }
}

void ShunyaViewportController::setAngleStepDeg(double angle) {
    if (angle > 1e-4 && snapping_.angle_step_deg != angle) {
        snapping_.angle_step_deg = angle;
        emit snapConfigChanged();
    }
}

void ShunyaViewportController::snapToView(const QString& faceName) {
    auto face_opt = core::domain::ViewCubeManager::face_from_string(faceName.toStdString());
    if (!face_opt) return;

    bool ortho = isOrthographic();
    camera_ = core::domain::ViewCubeManager::create_camera_for_face(
        *face_opt,
        (camera_.position - camera_.target).length(),
        camera_.target,
        ortho
    );
    current_view_ = faceName;
    emit cameraChanged();
    emit cameraAnimated(faceName);
}

void ShunyaViewportController::toggleProjection() {
    if (camera_.projection_type == core::domain::CameraProjectionType::Perspective) {
        camera_.projection_type = core::domain::CameraProjectionType::Orthographic;
    } else {
        camera_.projection_type = core::domain::CameraProjectionType::Perspective;
    }
    emit cameraChanged();
}

void ShunyaViewportController::orbit(double deltaYawDeg, double deltaPitchDeg) {
    double yaw_rad = deltaYawDeg * (core::base::math::PI / 180.0);
    double pitch_rad = deltaPitchDeg * (core::base::math::PI / 180.0);
    camera_.orbit(yaw_rad, pitch_rad);
    current_view_ = "Custom";
    emit cameraChanged();
}

void ShunyaViewportController::pan(double deltaX, double deltaY) {
    camera_.pan(deltaX, deltaY);
    emit cameraChanged();
}

void ShunyaViewportController::zoom(double deltaZoom) {
    camera_.zoom(deltaZoom);
    emit cameraChanged();
}

QVariantMap ShunyaViewportController::getGizmoProjection(double viewportW, double viewportH) {
    auto proj = core::domain::GizmoInteractionEngine::project_gizmo(
        camera_,
        target_position_,
        axis_length_,
        viewportW,
        viewportH
    );

    QVariantMap res;
    res["visible"] = !proj.is_behind_camera;
    res["originX"] = proj.origin_2d.x;
    res["originY"] = proj.origin_2d.y;
    res["xTipX"] = proj.x_tip_2d.x;
    res["xTipY"] = proj.x_tip_2d.y;
    res["yTipX"] = proj.y_tip_2d.x;
    res["yTipY"] = proj.y_tip_2d.y;
    res["zTipX"] = proj.z_tip_2d.x;
    res["zTipY"] = proj.z_tip_2d.y;
    return res;
}

QString ShunyaViewportController::testGizmoHover(double mouseX, double mouseY, double viewportW, double viewportH) {
    auto proj = core::domain::GizmoInteractionEngine::project_gizmo(
        camera_,
        target_position_,
        axis_length_,
        viewportW,
        viewportH
    );

    auto axis = core::domain::GizmoInteractionEngine::pick_axis_2d(proj, mouseX, mouseY, 14.0);
    QString axis_str;
    switch (axis) {
        case core::domain::GizmoAxis::X: axis_str = "x"; break;
        case core::domain::GizmoAxis::Y: axis_str = "y"; break;
        case core::domain::GizmoAxis::Z: axis_str = "z"; break;
        case core::domain::GizmoAxis::All: axis_str = "all"; break;
        default: axis_str = "none"; break;
    }

    if (active_axis_ != axis_str) {
        active_axis_ = axis_str;
        emit activeAxisChanged();
    }
    return axis_str;
}

void ShunyaViewportController::applyGizmoDrag(
    const QString& axis,
    double dx,
    double dy,
    double mouseX,
    double mouseY,
    double viewportW,
    double viewportH
) {
    core::domain::GizmoAxis g_axis = core::domain::GizmoAxis::None;
    if (axis == "x") g_axis = core::domain::GizmoAxis::X;
    else if (axis == "y") g_axis = core::domain::GizmoAxis::Y;
    else if (axis == "z") g_axis = core::domain::GizmoAxis::Z;
    else if (axis == "all") g_axis = core::domain::GizmoAxis::All;

    if (g_axis == core::domain::GizmoAxis::None) return;

    auto proj = core::domain::GizmoInteractionEngine::project_gizmo(
        camera_,
        target_position_,
        axis_length_,
        viewportW,
        viewportH
    );

    if (gizmo_mode_ == "translate") {
        auto delta = core::domain::GizmoInteractionEngine::calculate_translation_delta_2d(
            g_axis,
            camera_,
            proj,
            dx,
            dy,
            viewportH,
            snapping_
        );
        target_position_ = target_position_ + delta;
        emit targetTransformChanged();
    } else if (gizmo_mode_ == "rotate") {
        double delta_rot = core::domain::GizmoInteractionEngine::calculate_rotation_delta_2d(
            g_axis,
            proj,
            mouseX,
            mouseY,
            mouseX - dx,
            mouseY - dy,
            snapping_
        );
        double delta_deg = delta_rot * (180.0 / core::base::math::PI);
        if (g_axis == core::domain::GizmoAxis::X) target_rotation_.x += delta_deg;
        else if (g_axis == core::domain::GizmoAxis::Y) target_rotation_.y += delta_deg;
        else if (g_axis == core::domain::GizmoAxis::Z || g_axis == core::domain::GizmoAxis::All) target_rotation_.z += delta_deg;
        emit targetTransformChanged();
    } else if (gizmo_mode_ == "scale") {
        double factor = 1.0 + (dx - dy) * 0.01;
        if (snapping_.enabled) {
            factor = snapping_.snap_value(factor, snapping_.scale_step);
        }
        if (factor > 0.05) {
            if (g_axis == core::domain::GizmoAxis::X) target_scale_.x *= factor;
            else if (g_axis == core::domain::GizmoAxis::Y) target_scale_.y *= factor;
            else if (g_axis == core::domain::GizmoAxis::Z) target_scale_.z *= factor;
            else if (g_axis == core::domain::GizmoAxis::All) {
                target_scale_.x *= factor;
                target_scale_.y *= factor;
                target_scale_.z *= factor;
            }
            emit targetTransformChanged();
        }
    }
}

void ShunyaViewportController::resetTargetTransform() {
    target_position_ = {0.0, 0.0, 0.0};
    target_rotation_ = {0.0, 0.0, 0.0};
    target_scale_ = {1.0, 1.0, 1.0};
    emit targetTransformChanged();
}

void ShunyaViewportController::simulateTelemetryTick() {
    telemetry_.record_frame(0.01666 + (rand() % 10 - 5) * 0.0002);
    telemetry_.record_solver_step(2.1 + (rand() % 10) * 0.05, 1.42e-7, 12);
    emit telemetryChanged();
}

} // namespace shunya::gui
