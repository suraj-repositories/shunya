#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector3D>

#include "shunya/core/domain/viewport_interaction.hpp"
#include "shunya/core/domain/rendering.hpp"

namespace shunya::gui {

class ShunyaViewportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString gizmoMode READ gizmoMode WRITE setGizmoMode NOTIFY gizmoModeChanged)
    Q_PROPERTY(QString activeAxis READ activeAxis NOTIFY activeAxisChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapConfigChanged)
    Q_PROPERTY(double gridStep READ gridStep WRITE setGridStep NOTIFY snapConfigChanged)
    Q_PROPERTY(double angleStepDeg READ angleStepDeg WRITE setAngleStepDeg NOTIFY snapConfigChanged)
    Q_PROPERTY(QString currentView READ currentView NOTIFY cameraChanged)
    Q_PROPERTY(bool isOrthographic READ isOrthographic NOTIFY cameraChanged)

    Q_PROPERTY(double fps READ fps NOTIFY telemetryChanged)
    Q_PROPERTY(double frameTimeMs READ frameTimeMs NOTIFY telemetryChanged)
    Q_PROPERTY(double solverStepTimeMs READ solverStepTimeMs NOTIFY telemetryChanged)
    Q_PROPERTY(double solverResidual READ solverResidual NOTIFY telemetryChanged)
    Q_PROPERTY(int solverIterations READ solverIterations NOTIFY telemetryChanged)

    Q_PROPERTY(double targetPositionX READ targetPositionX NOTIFY targetTransformChanged)
    Q_PROPERTY(double targetPositionY READ targetPositionY NOTIFY targetTransformChanged)
    Q_PROPERTY(double targetPositionZ READ targetPositionZ NOTIFY targetTransformChanged)

    Q_PROPERTY(double targetRotationX READ targetRotationX NOTIFY targetTransformChanged)
    Q_PROPERTY(double targetRotationY READ targetRotationY NOTIFY targetTransformChanged)
    Q_PROPERTY(double targetRotationZ READ targetRotationZ NOTIFY targetTransformChanged)

    Q_PROPERTY(double targetScaleX READ targetScaleX NOTIFY targetTransformChanged)
    Q_PROPERTY(double targetScaleY READ targetScaleY NOTIFY targetTransformChanged)
    Q_PROPERTY(double targetScaleZ READ targetScaleZ NOTIFY targetTransformChanged)

public:
    explicit ShunyaViewportController(QObject* parent = nullptr);
    ~ShunyaViewportController() override = default;

    QString gizmoMode() const { return gizmo_mode_; }
    void setGizmoMode(const QString& mode);

    QString activeAxis() const { return active_axis_; }

    bool snapEnabled() const { return snapping_.enabled; }
    void setSnapEnabled(bool enabled);

    double gridStep() const { return snapping_.grid_step; }
    void setGridStep(double step);

    double angleStepDeg() const { return snapping_.angle_step_deg; }
    void setAngleStepDeg(double angle);

    QString currentView() const { return current_view_; }
    bool isOrthographic() const { return camera_.projection_type == core::domain::CameraProjectionType::Orthographic; }

    double fps() const { return telemetry_.current_fps(); }
    double frameTimeMs() const { return telemetry_.average_frame_time_ms(); }
    double solverStepTimeMs() const { return telemetry_.last_solver_step_time_ms(); }
    double solverResidual() const { return telemetry_.last_solver_residual(); }
    int solverIterations() const { return static_cast<int>(telemetry_.last_solver_iterations()); }

    double targetPositionX() const { return target_position_.x; }
    double targetPositionY() const { return target_position_.y; }
    double targetPositionZ() const { return target_position_.z; }

    double targetRotationX() const { return target_rotation_.x; }
    double targetRotationY() const { return target_rotation_.y; }
    double targetRotationZ() const { return target_rotation_.z; }

    double targetScaleX() const { return target_scale_.x; }
    double targetScaleY() const { return target_scale_.y; }
    double targetScaleZ() const { return target_scale_.z; }

    Q_INVOKABLE void snapToView(const QString& faceName);
    Q_INVOKABLE void toggleProjection();
    Q_INVOKABLE void orbit(double deltaYawDeg, double deltaPitchDeg);
    Q_INVOKABLE void pan(double deltaX, double deltaY);
    Q_INVOKABLE void zoom(double deltaZoom);

    Q_INVOKABLE QVariantMap getGizmoProjection(double viewportW, double viewportH);
    Q_INVOKABLE QString testGizmoHover(double mouseX, double mouseY, double viewportW, double viewportH);
    Q_INVOKABLE void applyGizmoDrag(const QString& axis, double dx, double dy, double mouseX, double mouseY, double viewportW, double viewportH);
    Q_INVOKABLE void resetTargetTransform();
    Q_INVOKABLE void simulateTelemetryTick();

signals:
    void gizmoModeChanged();
    void activeAxisChanged();
    void snapConfigChanged();
    void cameraChanged();
    void telemetryChanged();
    void targetTransformChanged();
    void cameraAnimated(const QString& targetView);

private:
    QString gizmo_mode_ = "translate"; // translate, rotate, scale
    QString active_axis_ = "none";
    QString current_view_ = "Isometric";

    core::domain::SnappingConfig snapping_;
    core::domain::ViewportCamera camera_;
    core::domain::DiagnosticsTelemetryTracker telemetry_{60};

    core::base::math::Vector3<double> target_position_{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> target_rotation_{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> target_scale_{1.0, 1.0, 1.0};
    double axis_length_ = 1.2;
};

} // namespace shunya::gui
