#pragma once

#include <QQuickPaintedItem>
#include <QColor>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>
#include <memory>
#include <vector>
#include <utility>

#include "shunya/eng/engineering_pipeline.hpp"
#include "shunya/cad3d/rhi_renderer_engine.hpp"
#include "shunya/cad3d/mesh_selection_engine.hpp"
#include "shunya/cad3d/mesh_topology_ops.hpp"
#include "shunya/cad3d/subdivision.hpp"

namespace shunya::gui {

/**
 * @brief Blender-grade 3D CAD & Multiphysics Viewport for PhysicalObjects and Assemblies.
 *        Supports Orbit/Pan/Zoom/Fly camera controls, Numpad ortho/persp views,
 *        dynamic ground grid, transform gizmos (Move/Rotate/Scale), Object vs Edit modes,
 *        submesh selection masks (Vertex/Edge/Face), polyhedral modeling tools (Extrude,
 *        Inset, Bevel, Loop Cut, Subdivide, Merge), interactive measurement tools,
 *        and multiple display modes (Solid, Wireframe, X-Ray, Section Analysis).
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
    Q_PROPERTY(double cameraPanX READ cameraPanX WRITE setCameraPanX NOTIFY viewChanged)
    Q_PROPERTY(double cameraPanY READ cameraPanY WRITE setCameraPanY NOTIFY viewChanged)
    Q_PROPERTY(bool isOrthographic READ isOrthographic WRITE setOrthographic NOTIFY viewChanged)
    Q_PROPERTY(QString currentView READ currentView NOTIFY viewChanged)
    Q_PROPERTY(QString selectedObjectId READ selectedObjectId WRITE setSelectedObjectId NOTIFY selectionChanged)
    Q_PROPERTY(int meshCount READ meshCount NOTIFY sceneUpdated)
    Q_PROPERTY(int triangleCount READ triangleCount NOTIFY sceneUpdated)
    Q_PROPERTY(bool rhiEnabled READ rhiEnabled WRITE setRhiEnabled NOTIFY rhiBackendChanged)
    Q_PROPERTY(QString rhiBackend READ rhiBackend WRITE setRhiBackend NOTIFY rhiBackendChanged)
    Q_PROPERTY(int drawCalls READ drawCalls NOTIFY sceneUpdated)
    Q_PROPERTY(int pixelsShaded READ pixelsShaded NOTIFY sceneUpdated)

    // Module PH5-25 Properties
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QString sectionPlane READ sectionPlane WRITE setSectionPlane NOTIFY sectionChanged)
    Q_PROPERTY(double sectionOffset READ sectionOffset WRITE setSectionOffset NOTIFY sectionChanged)
    Q_PROPERTY(QString interactionMode READ interactionMode WRITE setInteractionMode NOTIFY interactionModeChanged)
    Q_PROPERTY(QString selectionMask READ selectionMask WRITE setSelectionMask NOTIFY selectionMaskChanged)
    Q_PROPERTY(QString gizmoMode READ gizmoMode WRITE setGizmoMode NOTIFY gizmoModeChanged)
    Q_PROPERTY(QString orientationSpace READ orientationSpace WRITE setOrientationSpace NOTIFY orientationSpaceChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapConfigChanged)
    Q_PROPERTY(double gridStep READ gridStep WRITE setGridStep NOTIFY snapConfigChanged)
    Q_PROPERTY(double angleStepDeg READ angleStepDeg WRITE setAngleStepDeg NOTIFY snapConfigChanged)
    Q_PROPERTY(int selectedVertexCount READ selectedVertexCount NOTIFY submeshSelectionChanged)
    Q_PROPERTY(int selectedEdgeCount READ selectedEdgeCount NOTIFY submeshSelectionChanged)
    Q_PROPERTY(int selectedFaceCount READ selectedFaceCount NOTIFY submeshSelectionChanged)

public:
    explicit ShunyaViewport3DItem(QQuickItem* parent = nullptr);
    ~ShunyaViewport3DItem() override = default;

    static void setDefaultWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws);
    void setWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws);

    bool darkMode() const { return dark_mode_; }
    void setDarkMode(bool v) { if (dark_mode_ != v) { dark_mode_ = v; emit viewChanged(); update(); } }

    bool wireframe() const { return wireframe_ || display_mode_ == "wireframe"; }
    void setWireframe(bool v) { if (wireframe_ != v) { wireframe_ = v; emit viewChanged(); update(); } }

    bool showThermalOverlay() const { return show_thermal_; }
    void setShowThermalOverlay(bool v) { if (show_thermal_ != v) { show_thermal_ = v; emit viewChanged(); update(); } }

    bool physicsActive() const { return physics_active_; }
    void setPhysicsActive(bool v) { if (physics_active_ != v) { physics_active_ = v; emit viewChanged(); update(); } }

    double cameraYaw() const { return yaw_deg_; }
    void setCameraYaw(double v) { yaw_deg_ = v; current_view_ = "Custom"; emit viewChanged(); update(); }

    double cameraPitch() const { return pitch_deg_; }
    void setCameraPitch(double v) { pitch_deg_ = std::clamp(v, -89.0, 89.0); current_view_ = "Custom"; emit viewChanged(); update(); }

    double cameraDistance() const { return dist_; }
    void setCameraDistance(double v) { dist_ = std::clamp(v, 0.05, 100.0); emit viewChanged(); update(); }

    double cameraPanX() const { return pan_x_; }
    void setCameraPanX(double v) { pan_x_ = v; emit viewChanged(); update(); }

    double cameraPanY() const { return pan_y_; }
    void setCameraPanY(double v) { pan_y_ = v; emit viewChanged(); update(); }

    bool isOrthographic() const { return is_orthographic_; }
    void setOrthographic(bool v) { if (is_orthographic_ != v) { is_orthographic_ = v; emit viewChanged(); update(); } }

    QString currentView() const { return current_view_; }

    QString selectedObjectId() const { return selected_id_; }
    void setSelectedObjectId(const QString& id);

    int meshCount() const { return mesh_count_; }
    int triangleCount() const { return triangle_count_; }

    bool rhiEnabled() const { return rhi_enabled_; }
    void setRhiEnabled(bool v) { if (rhi_enabled_ != v) { rhi_enabled_ = v; emit rhiBackendChanged(); update(); } }

    QString rhiBackend() const { return rhi_backend_; }
    void setRhiBackend(const QString& b) { if (rhi_backend_ != b) { rhi_backend_ = b; emit rhiBackendChanged(); update(); } }

    int drawCalls() const { return draw_calls_; }
    int pixelsShaded() const { return pixels_shaded_; }

    QString displayMode() const { return display_mode_; }
    void setDisplayMode(const QString& m);

    QString sectionPlane() const { return section_plane_; }
    void setSectionPlane(const QString& p) { if (section_plane_ != p) { section_plane_ = p; emit sectionChanged(); update(); } }

    double sectionOffset() const { return section_offset_; }
    void setSectionOffset(double o) { if (section_offset_ != o) { section_offset_ = o; emit sectionChanged(); update(); } }

    QString interactionMode() const { return interaction_mode_; }
    void setInteractionMode(const QString& m);

    QString selectionMask() const { return selection_mask_; }
    void setSelectionMask(const QString& mask);

    QString gizmoMode() const { return gizmo_mode_; }
    void setGizmoMode(const QString& mode) { if (gizmo_mode_ != mode) { gizmo_mode_ = mode; emit gizmoModeChanged(); update(); } }

    QString orientationSpace() const { return orientation_space_; }
    void setOrientationSpace(const QString& space) { if (orientation_space_ != space) { orientation_space_ = space; emit orientationSpaceChanged(); update(); } }

    bool snapEnabled() const { return snap_enabled_; }
    void setSnapEnabled(bool v) { if (snap_enabled_ != v) { snap_enabled_ = v; emit snapConfigChanged(); } }

    double gridStep() const { return grid_step_; }
    void setGridStep(double v) { if (v > 1e-4 && grid_step_ != v) { grid_step_ = v; emit snapConfigChanged(); } }

    double angleStepDeg() const { return angle_step_deg_; }
    void setAngleStepDeg(double v) { if (v > 1e-4 && angle_step_deg_ != v) { angle_step_deg_ = v; emit snapConfigChanged(); } }

    int selectedVertexCount() const { return static_cast<int>(submesh_selection_.selected_vertices.size()); }
    int selectedEdgeCount() const { return static_cast<int>(submesh_selection_.selected_edges.size()); }
    int selectedFaceCount() const { return static_cast<int>(submesh_selection_.selected_faces.size()); }

    // Viewport Camera Navigation
    Q_INVOKABLE void orbitBy(double dyawDeg, double dpitchDeg);
    Q_INVOKABLE void panBy(double dxPx, double dyPx);
    Q_INVOKABLE void zoomBy(double factor);
    Q_INVOKABLE void snapView(const QString& face);
    Q_INVOKABLE void toggleProjection();
    Q_INVOKABLE void focusSelected();
    Q_INVOKABLE void resetCamera();
    Q_INVOKABLE void refreshScene();
    Q_INVOKABLE QVariantMap inspectSelectedMesh() const;

    // Gizmo Projection & Manipulation
    Q_INVOKABLE QVariantMap getGizmoProjection(double viewportW, double viewportH);
    Q_INVOKABLE QString testGizmoHover(double mouseX, double mouseY, double viewportW, double viewportH);
    Q_INVOKABLE void applyGizmoDrag(const QString& axis, double dx, double dy, double viewportW, double viewportH);

    // Edit Mode Selection & Modeling Tools
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deselectAll();
    Q_INVOKABLE void invertSelection();
    Q_INVOKABLE void selectSubmeshElement(const QString& mask, int index, bool toggle);

    Q_INVOKABLE bool extrudeSelected(double distance);
    Q_INVOKABLE bool insetSelected(double thickness);
    Q_INVOKABLE bool bevelSelected(double offset, int segments);
    Q_INVOKABLE bool loopCutSelected(double slide);
    Q_INVOKABLE bool subdivideSelected();
    Q_INVOKABLE int mergeSelectedVertices(double distance);

    // Interactive Measurement Tools
    Q_INVOKABLE double measureDistance(double x1, double y1, double z1, double x2, double y2, double z2) const;
    Q_INVOKABLE double measureAngle(double ax, double ay, double az, double bx, double by, double bz, double cx, double cy, double cz) const;
    Q_INVOKABLE double inspectSectionWallThickness(const QString& plane, double offset) const;

signals:
    void viewChanged();
    void selectionChanged();
    void sceneUpdated();
    void rhiBackendChanged();
    void displayModeChanged();
    void sectionChanged();
    void interactionModeChanged();
    void selectionMaskChanged();
    void gizmoModeChanged();
    void orientationSpaceChanged();
    void snapConfigChanged();
    void submeshSelectionChanged();

protected:
    void paint(QPainter* painter) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct ProjectedTri {
        float x0, y0, x1, y1, x2, y2;
        double depth;
        QColor fill;
        QColor edge;
        bool is_selected{false};
    };

    eng::PhysicalObject* getSelectedObjectMut();
    const eng::PhysicalObject* getSelectedObject() const;

    std::shared_ptr<eng::EngineeringWorkspaceState> ws_;
    bool dark_mode_{true};
    bool wireframe_{false};
    bool show_thermal_{false};
    bool physics_active_{false};
    bool is_orthographic_{false};
    QString current_view_{"Isometric"};

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

    // Module PH5-25 Engine State
    QString display_mode_{"solid"};    // solid, wireframe, xray, section
    QString section_plane_{"XY"};      // XY, YZ, ZX
    double section_offset_{0.0};
    QString interaction_mode_{"object"}; // object, edit
    QString selection_mask_{"face"};   // vertex, edge, face
    QString gizmo_mode_{"translate"};  // translate, rotate, scale, none
    QString orientation_space_{"global"}; // global, local
    bool snap_enabled_{false};
    double grid_step_{0.01};           // 10 mm
    double angle_step_deg_{15.0};      // 15 deg

    cad3d::MeshSelection submesh_selection_;
};

} // namespace shunya::gui
