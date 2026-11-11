#pragma once

#include <QQuickItem>
#include <QSGNode>
#include <QString>
#include <QVariantList>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

#include "shunya/cad2d/sketch_viewport_2d.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/snap_manager.hpp"
#include "shunya/cad2d/polar_guide.hpp"

namespace shunya::gui {

class ShunyaSketchCanvas2D : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString activeTool READ activeTool WRITE setActiveTool NOTIFY toolChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool polarEnabled READ polarEnabled WRITE setPolarEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool constructionMode READ constructionMode WRITE setConstructionMode NOTIFY settingsChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY settingsChanged)
    Q_PROPERTY(int polygonSides READ polygonSides WRITE setPolygonSides NOTIFY settingsChanged)
    Q_PROPERTY(QString selectedEntityId READ selectedEntityId WRITE setSelectedEntityId NOTIFY selectionChanged)
    Q_PROPERTY(QString cursorCoordText READ cursorCoordText NOTIFY cursorChanged)
    Q_PROPERTY(int entityCount READ entityCount NOTIFY documentChanged)
    Q_PROPERTY(int solidLoopCount READ solidLoopCount NOTIFY documentChanged)

    Q_PROPERTY(bool snapActive READ snapActive NOTIFY cursorChanged)
    Q_PROPERTY(double snapScreenX READ snapScreenX NOTIFY cursorChanged)
    Q_PROPERTY(double snapScreenY READ snapScreenY NOTIFY cursorChanged)
    Q_PROPERTY(QString snapTypeName READ snapTypeName NOTIFY cursorChanged)

    Q_PROPERTY(bool polarActive READ polarActive NOTIFY cursorChanged)
    Q_PROPERTY(double polarOriginX READ polarOriginX NOTIFY cursorChanged)
    Q_PROPERTY(double polarOriginY READ polarOriginY NOTIFY cursorChanged)
    Q_PROPERTY(double polarEndX READ polarEndX NOTIFY cursorChanged)
    Q_PROPERTY(double polarEndY READ polarEndY NOTIFY cursorChanged)
    Q_PROPERTY(double polarAngleDeg READ polarAngleDeg NOTIFY cursorChanged)

public:
    explicit ShunyaSketchCanvas2D(QQuickItem* parent = nullptr);
    ~ShunyaSketchCanvas2D() override = default;

    QString activeTool() const { return active_tool_; }
    Q_INVOKABLE void setActiveTool(const QString& tool);

    bool snapEnabled() const { return snap_enabled_; }
    Q_INVOKABLE void setSnapEnabled(bool v);

    bool polarEnabled() const { return polar_.enabled; }
    Q_INVOKABLE void setPolarEnabled(bool v);

    bool constructionMode() const { return construction_mode_; }
    Q_INVOKABLE void setConstructionMode(bool v);

    bool darkMode() const { return dark_mode_; }
    Q_INVOKABLE void setDarkMode(bool v);

    int polygonSides() const { return polygon_sides_; }
    Q_INVOKABLE void setPolygonSides(int n);

    QString selectedEntityId() const { return selected_entity_id_; }
    Q_INVOKABLE void setSelectedEntityId(const QString& id);

    QString cursorCoordText() const { return cursor_coord_text_; }
    int entityCount() const { return static_cast<int>(doc_.entities().size()); }
    int solidLoopCount() const { return solid_loop_count_; }

    bool snapActive() const { return active_snap_.has_value(); }
    double snapScreenX() const { return snap_screen_pt_.x; }
    double snapScreenY() const { return snap_screen_pt_.y; }
    QString snapTypeName() const {
        return active_snap_ ? QString::fromStdString(std::string(cad2d::snap_type_to_string(active_snap_->type))) : "";
    }

    bool polarActive() const { return active_polar_.active; }
    double polarOriginX() const { return polar_origin_px_.x; }
    double polarOriginY() const { return polar_origin_px_.y; }
    double polarEndX() const { return polar_end_px_.x; }
    double polarEndY() const { return polar_end_px_.y; }
    double polarAngleDeg() const { return active_polar_.snapped_angle_deg; }

    Q_INVOKABLE void onHoverMove(double sx, double sy);
    Q_INVOKABLE void onClickDown(double sx, double sy);
    Q_INVOKABLE void zoomAt(double sx, double sy, double factor);
    Q_INVOKABLE void panBy(double dx, double dy);
    Q_INVOKABLE void cancelDrawing();
    Q_INVOKABLE void finishPath();
    Q_INVOKABLE void clearDocument();
    Q_INVOKABLE void loadDemoSketch();

    Q_INVOKABLE QVariantList getEntities() const;
    Q_INVOKABLE void transformEntity(const QString& entityId, double dx, double dy, double rotDeg, double scale);
    Q_INVOKABLE void duplicateEntity(const QString& entityId);
    Q_INVOKABLE void deleteEntity(const QString& entityId);
    Q_INVOKABLE void assignEntityMaterial(const QString& entityId, const QString& matId, double depthM);

    const cad2d::SketchDocument2D& document() const { return doc_; }
    void setDocument(cad2d::SketchDocument2D doc);

signals:
    void toolChanged();
    void settingsChanged();
    void cursorChanged();
    void documentChanged();
    void selectionChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void update_cursor_state(double sx, double sy);
    void refresh_solid_loop_count();

    cad2d::SketchViewport2D viewport_;
    cad2d::SketchDocument2D doc_;
    cad2d::SnapTargetManager snap_mgr_{0.5};
    cad2d::PolarTrackingEngine polar_;

    QString active_tool_{"line"};
    bool snap_enabled_{true};
    bool construction_mode_{false};
    bool dark_mode_{true};
    int polygon_sides_{6};
    bool is_drawing_{false};
    cad2d::Point2D draw_anchor_world_{};
    cad2d::Point2D effective_cursor_world_{};
    std::vector<cad2d::Point2D> path_pts_;
    QString selected_entity_id_;
    std::unordered_map<std::string, std::pair<QString, double>> entity_materials_; // entity_id -> (matId, depthM)

    std::optional<cad2d::SnapCandidate> active_snap_;
    cad2d::Point2D snap_screen_pt_{};
    cad2d::PolarGuideResult active_polar_{};
    cad2d::Point2D polar_origin_px_{};
    cad2d::Point2D polar_end_px_{};

    QString cursor_coord_text_{"X: 0.00 mm  Y: 0.00 mm"};
    int solid_loop_count_{0};
};

} // namespace shunya::gui
