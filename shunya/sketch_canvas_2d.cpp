#include "sketch_canvas_2d.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <algorithm>
#include <cmath>

namespace shunya::gui {

namespace {

QSGGeometryNode* ensure_line_node(QSGNode* root, int index, const QColor& color, float lineWidth) {
    while (root->childCount() <= index) {
        auto* node = new QSGGeometryNode();
        auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geom->setDrawingMode(QSGGeometry::DrawLines);
        geom->setLineWidth(lineWidth);
        node->setGeometry(geom);
        node->setFlag(QSGNode::OwnsGeometry);

        auto* mat = new QSGFlatColorMaterial();
        mat->setColor(color);
        node->setMaterial(mat);
        node->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(node);
    }
    auto* node = static_cast<QSGGeometryNode*>(root->childAtIndex(index));
    static_cast<QSGFlatColorMaterial*>(node->material())->setColor(color);
    node->geometry()->setLineWidth(lineWidth);
    return node;
}

void upload_lines(QSGGeometryNode* node, const std::vector<float>& coords) {
    int vert_count = static_cast<int>(coords.size() / 2);
    auto* geom = node->geometry();
    geom->allocate(vert_count);
    auto* v = geom->vertexDataAsPoint2D();
    for (int i = 0; i < vert_count; ++i) {
        v[i].set(coords[2 * i], coords[2 * i + 1]);
    }
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
}

void push_seg(std::vector<float>& out, float x0, float y0, float x1, float y1) {
    out.push_back(x0);
    out.push_back(y0);
    out.push_back(x1);
    out.push_back(y1);
}

void append_dashed_polyline(
    std::vector<float>& out,
    const cad2d::SketchViewport2D& vp,
    const std::vector<cad2d::Point2D>& pts
) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        auto p0 = vp.world_to_screen(pts[i]);
        auto p1 = vp.world_to_screen(pts[i + 1]);
        double dx = p1.x - p0.x;
        double dy = p1.y - p0.y;
        double len = std::hypot(dx, dy);
        if (len < 1.0) continue;

        double dash = 6.0;
        double gap = 4.0;
        double step = dash + gap;
        for (double s = 0.0; s < len; s += step) {
            double e = std::min(len, s + dash);
            double t0 = s / len;
            double t1 = e / len;
            push_seg(
                out,
                static_cast<float>(p0.x + dx * t0), static_cast<float>(p0.y + dy * t0),
                static_cast<float>(p0.x + dx * t1), static_cast<float>(p0.y + dy * t1)
            );
        }
    }
}

void append_solid_polyline(
    std::vector<float>& out,
    const cad2d::SketchViewport2D& vp,
    const std::vector<cad2d::Point2D>& pts
) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        auto p0 = vp.world_to_screen(pts[i]);
        auto p1 = vp.world_to_screen(pts[i + 1]);
        push_seg(out, static_cast<float>(p0.x), static_cast<float>(p0.y),
                 static_cast<float>(p1.x), static_cast<float>(p1.y));
    }
}

} // namespace

ShunyaSketchCanvas2D::ShunyaSketchCanvas2D(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    viewport_.viewport_width = 800.0;
    viewport_.viewport_height = 500.0;
    viewport_.pixels_per_unit = 40.0; // 40 px = 1m (reasonable view for demo [-8, 8])
    loadDemoSketch();
}

void ShunyaSketchCanvas2D::setActiveTool(const QString& tool) {
    if (active_tool_ != tool) {
        active_tool_ = tool;
        is_drawing_ = false;
        emit toolChanged();
        update();
    }
}

void ShunyaSketchCanvas2D::setSnapEnabled(bool v) {
    if (snap_enabled_ != v) {
        snap_enabled_ = v;
        if (!v) active_snap_.reset();
        emit settingsChanged();
        emit cursorChanged();
        update();
    }
}

void ShunyaSketchCanvas2D::setPolarEnabled(bool v) {
    if (polar_.enabled != v) {
        polar_.enabled = v;
        if (!v) active_polar_ = {};
        emit settingsChanged();
        emit cursorChanged();
        update();
    }
}

void ShunyaSketchCanvas2D::setConstructionMode(bool v) {
    if (construction_mode_ != v) {
        construction_mode_ = v;
        emit settingsChanged();
        update();
    }
}

void ShunyaSketchCanvas2D::update_cursor_state(double sx, double sy) {
    cad2d::Point2D raw_world = viewport_.screen_to_world({sx, sy});
    effective_cursor_world_ = raw_world;

    active_snap_.reset();
    active_polar_ = {};

    if (snap_enabled_) {
        std::optional<cad2d::Point2D> anchor_opt = is_drawing_ ? std::optional<cad2d::Point2D>(draw_anchor_world_) : std::nullopt;
        active_snap_ = snap_mgr_.query_snap(
            doc_, viewport_, raw_world, 12.0,
            static_cast<uint32_t>(cad2d::SnapType::All),
            anchor_opt
        );
    }

    if (active_snap_) {
        effective_cursor_world_ = active_snap_->world_pos;
        snap_screen_pt_ = viewport_.world_to_screen(active_snap_->world_pos);
    } else if (is_drawing_ && polar_.enabled) {
        active_polar_ = polar_.evaluate(draw_anchor_world_, raw_world);
        if (active_polar_.active) {
            effective_cursor_world_ = active_polar_.snapped_point;
            polar_origin_px_ = viewport_.world_to_screen(active_polar_.origin);
            polar_end_px_ = viewport_.world_to_screen(
                active_polar_.origin + active_polar_.direction * ((effective_cursor_world_ - active_polar_.origin).length() + 4.0)
            );
        }
    }

    QString xs = QString::fromStdString(cad2d::SketchViewport2D::format_world_coordinate(effective_cursor_world_.x));
    QString ys = QString::fromStdString(cad2d::SketchViewport2D::format_world_coordinate(effective_cursor_world_.y));
    cursor_coord_text_ = QString("X: %1   Y: %2").arg(xs, ys);
    emit cursorChanged();
}

void ShunyaSketchCanvas2D::onHoverMove(double sx, double sy) {
    update_cursor_state(sx, sy);
    if (is_drawing_) update();
}

void ShunyaSketchCanvas2D::onClickDown(double sx, double sy) {
    update_cursor_state(sx, sy);
    if (active_tool_ == "select") return;

    if (!is_drawing_) {
        draw_anchor_world_ = effective_cursor_world_;
        is_drawing_ = true;
        update();
    } else {
        double dist = (effective_cursor_world_ - draw_anchor_world_).length();
        if (dist > 1e-6) {
            if (active_tool_ == "line") {
                doc_.add_entity(std::make_shared<cad2d::LineSegment2D>(draw_anchor_world_, effective_cursor_world_), construction_mode_);
            } else if (active_tool_ == "circle") {
                doc_.add_entity(std::make_shared<cad2d::Circle2D>(draw_anchor_world_, dist), construction_mode_);
            } else if (active_tool_ == "rect") {
                double x0 = std::min(draw_anchor_world_.x, effective_cursor_world_.x);
                double x1 = std::max(draw_anchor_world_.x, effective_cursor_world_.x);
                double y0 = std::min(draw_anchor_world_.y, effective_cursor_world_.y);
                double y1 = std::max(draw_anchor_world_.y, effective_cursor_world_.y);
                doc_.add_entity(std::make_shared<cad2d::Polyline2D>(
                    std::vector<cad2d::Point2D>{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, true
                ), construction_mode_);
            }
            snap_mgr_.rebuild_index(doc_);
            refresh_solid_loop_count();
            emit documentChanged();
        }
        is_drawing_ = false;
        active_polar_ = {};
        update();
    }
}

void ShunyaSketchCanvas2D::zoomAt(double sx, double sy, double factor) {
    viewport_.zoom_at_screen({sx, sy}, factor);
    update_cursor_state(sx, sy);
    update();
}

void ShunyaSketchCanvas2D::panBy(double dx, double dy) {
    viewport_.pan_by_screen(dx, dy);
    if (active_snap_) {
        snap_screen_pt_ = viewport_.world_to_screen(active_snap_->world_pos);
    }
    emit cursorChanged();
    update();
}

void ShunyaSketchCanvas2D::cancelDrawing() {
    is_drawing_ = false;
    active_polar_ = {};
    emit cursorChanged();
    update();
}

void ShunyaSketchCanvas2D::clearDocument() {
    doc_.clear();
    snap_mgr_.rebuild_index(doc_);
    is_drawing_ = false;
    refresh_solid_loop_count();
    emit documentChanged();
    update();
}

void ShunyaSketchCanvas2D::loadDemoSketch() {
    doc_.clear();
    // 1. Solid outer rectangle
    doc_.add_entity(std::make_shared<cad2d::Polyline2D>(
        std::vector<cad2d::Point2D>{{-4.0, -2.5}, {4.0, -2.5}, {4.0, 2.5}, {-4.0, 2.5}}, true
    ), false);
    // 2. Solid centered circular hole & mounting holes
    doc_.add_entity(std::make_shared<cad2d::Circle2D>(cad2d::Point2D{0.0, 0.0}, 1.25), false);
    doc_.add_entity(std::make_shared<cad2d::Circle2D>(cad2d::Point2D{-2.8, 1.5}, 0.4), false);
    doc_.add_entity(std::make_shared<cad2d::Circle2D>(cad2d::Point2D{2.8, 1.5}, 0.4), false);
    // 3. Construction centerlines (dashed yellow, excluded from solid loops!)
    doc_.add_entity(std::make_shared<cad2d::LineSegment2D>(cad2d::Point2D{-5.0, 0.0}, cad2d::Point2D{5.0, 0.0}), true);
    doc_.add_entity(std::make_shared<cad2d::LineSegment2D>(cad2d::Point2D{0.0, -3.5}, cad2d::Point2D{0.0, 3.5}), true);

    snap_mgr_.rebuild_index(doc_);
    refresh_solid_loop_count();
    emit documentChanged();
    update();
}

void ShunyaSketchCanvas2D::refresh_solid_loop_count() {
    solid_loop_count_ = static_cast<int>(doc_.extract_solid_profile_loops(32).size());
}

void ShunyaSketchCanvas2D::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    viewport_.viewport_width = std::max(1.0, newGeometry.width());
    viewport_.viewport_height = std::max(1.0, newGeometry.height());
    update();
}

QSGNode* ShunyaSketchCanvas2D::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    QSGNode* root = oldNode;
    if (!root) {
        root = new QSGNode();
    }

    float w = static_cast<float>(width());
    float h = static_cast<float>(height());

    // 1. Adaptive Grid
    auto grid = viewport_.compute_adaptive_grid(16.0);
    std::vector<float> minor_verts;
    std::vector<float> major_verts;
    for (const auto& vl : grid.vertical_lines) {
        float sx = static_cast<float>(vl.screen_coord);
        push_seg(vl.is_major ? major_verts : minor_verts, sx, 0.0f, sx, h);
    }
    for (const auto& hl : grid.horizontal_lines) {
        float sy = static_cast<float>(hl.screen_coord);
        push_seg(hl.is_major ? major_verts : minor_verts, 0.0f, sy, w, sy);
    }

    upload_lines(ensure_line_node(root, 0, QColor("#1c2128"), 1.0f), minor_verts);
    upload_lines(ensure_line_node(root, 1, QColor("#2a313c"), 1.0f), major_verts);

    // 2. Origin Crosshair Axes
    std::vector<float> origin_verts;
    auto orig_px = viewport_.world_to_screen({0.0, 0.0});
    push_seg(origin_verts, 0.0f, static_cast<float>(orig_px.y), w, static_cast<float>(orig_px.y));
    push_seg(origin_verts, static_cast<float>(orig_px.x), 0.0f, static_cast<float>(orig_px.x), h);
    upload_lines(ensure_line_node(root, 2, QColor("#3e4451"), 1.2f), origin_verts);

    // 3. Document Entities (Solid vs Construction)
    std::vector<float> solid_verts;
    std::vector<float> constr_verts;
    for (const auto& ent : doc_.entities()) {
        if (!ent.visible || !ent.curve) continue;
        size_t segs = (ent.curve->type() == cad2d::CurveType2D::LineSegment || ent.curve->type() == cad2d::CurveType2D::Polyline) ? 1 : 64;
        auto pts = ent.curve->sample_polyline(segs);
        if (ent.is_construction) {
            append_dashed_polyline(constr_verts, viewport_, pts);
        } else {
            append_solid_polyline(solid_verts, viewport_, pts);
        }
    }

    // Add interactive drawing preview
    if (is_drawing_) {
        std::vector<cad2d::Point2D> preview_pts;
        if (active_tool_ == "line") {
            preview_pts = {draw_anchor_world_, effective_cursor_world_};
        } else if (active_tool_ == "circle") {
            double r = (effective_cursor_world_ - draw_anchor_world_).length();
            if (r > 1e-6) preview_pts = cad2d::Circle2D(draw_anchor_world_, r).sample_polyline(64);
        } else if (active_tool_ == "rect") {
            double x0 = draw_anchor_world_.x, y0 = draw_anchor_world_.y;
            double x1 = effective_cursor_world_.x, y1 = effective_cursor_world_.y;
            preview_pts = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0}};
        }
        if (construction_mode_) append_dashed_polyline(constr_verts, viewport_, preview_pts);
        else append_solid_polyline(solid_verts, viewport_, preview_pts);
    }

    upload_lines(ensure_line_node(root, 3, QColor("#61afef"), 1.8f), solid_verts);
    upload_lines(ensure_line_node(root, 4, QColor("#e5c07b"), 1.2f), constr_verts);

    return root;
}

} // namespace shunya::gui
