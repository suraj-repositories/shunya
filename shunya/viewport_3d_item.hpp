#pragma once

#include <QQuickPaintedItem>
#include <QColor>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "shunya/eng/engineering_pipeline.hpp"

namespace shunya::cad3d {
class RhiRendererEngine;
}

namespace shunya::gui {

/**
 * @brief Interactive 3D CAD & Multiphysics Viewport rendering PhysicalObjects
 *        (Box/Sphere/Cylinder/Cone/Extrude) as shaded SolidMesh3D triangles with
 *        orbit/pan/zoom camera, wireframe toggle, material colors, and live physics sync.
 */
class ShunyaViewport3DItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY viewChanged)
    Q_PROPERTY(bool wireframe READ wireframe WRITE setWireframe NOTIFY viewChanged)
    Q_PROPERTY(bool showThermalOverlay READ showThermalOverlay WRITE setShowThermalOverlay NOTIFY viewChanged)
    Q_PROPERTY(bool physicsActive READ physicsActive WRITE setPhysicsActive NOTIFY viewChanged)
    Q_PROPERTY(double cameraYaw READ cameraYaw WRITE setCameraYaw NOTIFY viewChanged)
    Q_PROPERTY(double cameraPitch READ cameraPitch WRITE setCameraPitch NOTIFY viewChanged)
    Q_PROPERTY(double cameraDistance READ cameraDistance WRITE setCameraDistance NOTIFY viewChanged)
    Q_PROPERTY(QString selectedObjectId READ selectedObjectId WRITE setSelectedObjectId NOTIFY selectionChanged)
    Q_PROPERTY(int meshCount READ meshCount NOTIFY sceneUpdated)
    Q_PROPERTY(int triangleCount READ triangleCount NOTIFY sceneUpdated)
    Q_PROPERTY(bool rhiEnabled READ rhiEnabled WRITE setRhiEnabled NOTIFY rhiBackendChanged)
    Q_PROPERTY(QString rhiBackend READ rhiBackend WRITE setRhiBackend NOTIFY rhiBackendChanged)
    Q_PROPERTY(int drawCalls READ drawCalls NOTIFY sceneUpdated)
    Q_PROPERTY(int pixelsShaded READ pixelsShaded NOTIFY sceneUpdated)

public:
    explicit ShunyaViewport3DItem(QQuickItem* parent = nullptr);
    ~ShunyaViewport3DItem() override = default;

    static void setDefaultWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws);
    void setWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws);

    bool darkMode() const { return dark_mode_; }
    void setDarkMode(bool v) { if (dark_mode_ != v) { dark_mode_ = v; emit viewChanged(); update(); } }

    bool wireframe() const { return wireframe_; }
    void setWireframe(bool v) { if (wireframe_ != v) { wireframe_ = v; emit viewChanged(); update(); } }

    bool showThermalOverlay() const { return show_thermal_; }
    void setShowThermalOverlay(bool v) { if (show_thermal_ != v) { show_thermal_ = v; emit viewChanged(); update(); } }

    bool physicsActive() const { return physics_active_; }
    void setPhysicsActive(bool v) { if (physics_active_ != v) { physics_active_ = v; emit viewChanged(); update(); } }

    double cameraYaw() const { return yaw_deg_; }
    void setCameraYaw(double v) { yaw_deg_ = v; emit viewChanged(); update(); }

    double cameraPitch() const { return pitch_deg_; }
    void setCameraPitch(double v) { pitch_deg_ = std::clamp(v, -85.0, 85.0); emit viewChanged(); update(); }

    double cameraDistance() const { return dist_; }
    void setCameraDistance(double v) { dist_ = std::clamp(v, 0.15, 50.0); emit viewChanged(); update(); }

    QString selectedObjectId() const { return selected_id_; }
    void setSelectedObjectId(const QString& id) {
        if (selected_id_ != id) { selected_id_ = id; emit selectionChanged(); update(); }
    }

    int meshCount() const { return mesh_count_; }
    int triangleCount() const { return triangle_count_; }

    bool rhiEnabled() const { return rhi_enabled_; }
    void setRhiEnabled(bool v) { if (rhi_enabled_ != v) { rhi_enabled_ = v; emit rhiBackendChanged(); update(); } }

    QString rhiBackend() const { return rhi_backend_; }
    void setRhiBackend(const QString& b) { if (rhi_backend_ != b) { rhi_backend_ = b; emit rhiBackendChanged(); update(); } }

    int drawCalls() const { return draw_calls_; }
    int pixelsShaded() const { return pixels_shaded_; }

    Q_INVOKABLE void orbitBy(double dyawDeg, double dpitchDeg);
    Q_INVOKABLE void panBy(double dxPx, double dyPx);
    Q_INVOKABLE void zoomBy(double factor);
    Q_INVOKABLE void snapView(const QString& face);
    Q_INVOKABLE void resetCamera();
    Q_INVOKABLE void refreshScene();
    Q_INVOKABLE QVariantMap inspectSelectedMesh() const;

signals:
    void viewChanged();
    void selectionChanged();
    void sceneUpdated();
    void rhiBackendChanged();

protected:
    void paint(QPainter* painter) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    std::shared_ptr<eng::EngineeringWorkspaceState> ws_;
    bool dark_mode_{true};
    bool wireframe_{false};
    bool show_thermal_{false};
    bool physics_active_{false};
    double yaw_deg_{38.0};
    double pitch_deg_{26.0};
    double dist_{0.55};
    double pan_x_{0.0};
    double pan_y_{0.0};
    QString selected_id_;
    int mesh_count_{0};
    int triangle_count_{0};
    bool rhi_enabled_{true};
    QString rhi_backend_{"Vulkan"};
    int draw_calls_{0};
    int pixels_shaded_{0};
    std::unique_ptr<cad3d::RhiRendererEngine> rhi_engine_;
};

} // namespace shunya::gui
