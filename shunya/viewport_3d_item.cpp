#include "viewport_3d_item.hpp"
#include "shunya/cad3d/rhi_renderer_engine.hpp"
#include "shunya/cad3d/mesh_topology_ops.hpp"
#include "shunya/cad3d/subdivision.hpp"
#include "shunya/cad3d/mesh_selection_engine.hpp"

#include <QImage>
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <vector>

namespace shunya::gui {

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double kDegToRad = PI / 180.0;

QColor materialBaseColor(const eng::PhysicalObject& obj,
                         const core::domain::MaterialRegistry& reg,
                         bool darkMode) {
    if (obj.regions.empty()) return darkMode ? QColor("#A3A3A3") : QColor("#525252");
    const auto* mat = reg.get_material(obj.regions[0].material_id);
    if (!mat) return QColor("#8A8A8A");
    std::string nm = mat->name();
    std::transform(nm.begin(), nm.end(), nm.begin(), ::tolower);
    if (nm.find("copper") != std::string::npos || nm.find("cu") != std::string::npos) return QColor("#C87D3B");
    if (nm.find("aluminum") != std::string::npos || nm.find("al") != std::string::npos) return QColor("#B4BAC2");
    if (nm.find("gold") != std::string::npos) return QColor("#D4AF37");
    if (nm.find("lithium") != std::string::npos || nm.find("lifepo4") != std::string::npos) return QColor("#4A90E2");
    if (nm.find("graphite") != std::string::npos || nm.find("carbon") != std::string::npos) return QColor("#50555E");
    if (nm.find("nichrome") != std::string::npos || nm.find("steel") != std::string::npos || nm.find("iron") != std::string::npos) {
        return QColor("#8691A2");
    }
    if (nm.find("separator") != std::string::npos || nm.find("ptfe") != std::string::npos || nm.find("insulator") != std::string::npos) {
        return QColor("#E2DFD7");
    }
    return darkMode ? QColor("#A3A3A3") : QColor("#666666");
}

QColor thermalColor(double tempK) {
    const double t = std::clamp((tempK - 293.15) / 100.0, 0.0, 1.0);
    if (t < 0.5) {
        const double k = t * 2.0;
        return QColor::fromRgbF(0.18 + 0.77 * k, 0.50 + 0.30 * k, 0.92 - 0.72 * k);
    }
    const double k = (t - 0.5) * 2.0;
    return QColor::fromRgbF(0.95, 0.80 - 0.65 * k, 0.20 - 0.10 * k);
}

std::weak_ptr<eng::EngineeringWorkspaceState> s_default_ws_3d;

void syncObjectProperties(eng::PhysicalObject* target, const std::shared_ptr<eng::EngineeringWorkspaceState>& ws) {
    if (!target || !ws) return;
    auto eval_res = eng::PhysicalObjectManager::evaluate_properties(*target, ws->material_registry());
    if (eval_res.is_ok()) {
        target->computed = eval_res.value();
    }
}

} // namespace

void ShunyaViewport3DItem::setDefaultWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws) {
    s_default_ws_3d = ws;
}

ShunyaViewport3DItem::ShunyaViewport3DItem(QQuickItem* parent)
    : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    if (auto sp = s_default_ws_3d.lock()) {
        ws_ = sp;
        refreshScene();
    }
}

void ShunyaViewport3DItem::setWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws) {
    ws_ = std::move(ws);
    refreshScene();
}

eng::PhysicalObject* ShunyaViewport3DItem::getSelectedObjectMut() {
    if (!ws_) return nullptr;
    const auto uid = core::base::Uuid::from_string(selected_id_.toStdString());
    if (uid) {
        return ws_->object_manager().get_object_mut(*uid);
    }
    if (!ws_->object_manager().objects().empty()) {
        const auto id = ws_->object_manager().objects().front().id;
        return ws_->object_manager().get_object_mut(id);
    }
    return nullptr;
}

const eng::PhysicalObject* ShunyaViewport3DItem::getSelectedObject() const {
    if (!ws_) return nullptr;
    const auto uid = core::base::Uuid::from_string(selected_id_.toStdString());
    if (uid) {
        return ws_->object_manager().get_object(*uid);
    }
    if (!ws_->object_manager().objects().empty()) {
        return &ws_->object_manager().objects().front();
    }
    return nullptr;
}

void ShunyaViewport3DItem::setSelectedObjectId(const QString& id) {
    if (selected_id_ != id) {
        selected_id_ = id;
        submesh_selection_.clear();
        emit selectionChanged();
        emit submeshSelectionChanged();
        update();
    }
}

void ShunyaViewport3DItem::setDisplayMode(const QString& m) {
    if (display_mode_ != m) {
        display_mode_ = m;
        wireframe_ = (m == "wireframe");
        emit displayModeChanged();
        emit viewChanged();
        update();
    }
}

void ShunyaViewport3DItem::setInteractionMode(const QString& m) {
    if (interaction_mode_ != m) {
        interaction_mode_ = m;
        submesh_selection_.clear();
        if (m == "edit") {
            submesh_selection_.select_face(0, false);
        }
        emit interactionModeChanged();
        emit submeshSelectionChanged();
        update();
    }
}

void ShunyaViewport3DItem::setSelectionMask(const QString& mask) {
    if (selection_mask_ != mask) {
        selection_mask_ = mask;
        if (mask == "vertex") submesh_selection_.mode = cad3d::MeshSelectionMode::Vertex;
        else if (mask == "edge") submesh_selection_.mode = cad3d::MeshSelectionMode::Edge;
        else submesh_selection_.mode = cad3d::MeshSelectionMode::Face;
        emit selectionMaskChanged();
        emit submeshSelectionChanged();
        update();
    }
}

void ShunyaViewport3DItem::orbitBy(double dyawDeg, double dpitchDeg) {
    yaw_deg_ += dyawDeg;
    pitch_deg_ = std::clamp(pitch_deg_ + dpitchDeg, -89.0, 89.0);
    current_view_ = "Custom";
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::panBy(double dxPx, double dyPx) {
    const double sens = dist_ * 0.002;
    const double cy = std::cos(yaw_deg_ * kDegToRad);
    pan_x_ -= (dxPx * cy) * sens;
    pan_y_ += dyPx * sens;
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::zoomBy(double factor) {
    dist_ = std::clamp(dist_ * factor, 0.05, 100.0);
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::snapView(const QString& face) {
    const QString f = face.trimmed().toLower();
    if (f == "front" || f == "1") {
        yaw_deg_ = 0.0; pitch_deg_ = 0.0; current_view_ = "Front";
    } else if (f == "back") {
        yaw_deg_ = 180.0; pitch_deg_ = 0.0; current_view_ = "Back";
    } else if (f == "top" || f == "7") {
        yaw_deg_ = 0.0; pitch_deg_ = 89.0; current_view_ = "Top";
    } else if (f == "bottom") {
        yaw_deg_ = 0.0; pitch_deg_ = -89.0; current_view_ = "Bottom";
    } else if (f == "right" || f == "side" || f == "3") {
        yaw_deg_ = 90.0; pitch_deg_ = 0.0; current_view_ = "Right";
    } else if (f == "left") {
        yaw_deg_ = -90.0; pitch_deg_ = 0.0; current_view_ = "Left";
    } else if (f == "isometric" || f == "iso") {
        yaw_deg_ = 38.0; pitch_deg_ = 26.0; current_view_ = "Isometric";
    }
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::toggleProjection() {
    is_orthographic_ = !is_orthographic_;
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::focusSelected() {
    const auto* target = getSelectedObject();
    if (!target) return;
    pan_x_ = target->geometry.position.x;
    pan_y_ = target->geometry.position.y;
    const double dmax = std::max({target->geometry.dim_x_m, target->geometry.dim_y_m, target->geometry.dim_z_m, target->geometry.radius_m * 2.0});
    dist_ = std::clamp(dmax * 3.0, 0.15, 10.0);
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::resetCamera() {
    yaw_deg_ = 38.0;
    pitch_deg_ = 26.0;
    dist_ = 0.55;
    pan_x_ = 0.0;
    pan_y_ = 0.0;
    is_orthographic_ = false;
    current_view_ = "Isometric";
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::refreshScene() {
    if (!ws_) return;
    int mcnt = 0;
    int tcnt = 0;
    for (const auto& obj : ws_->object_manager().objects()) {
        ++mcnt;
        tcnt += static_cast<int>(obj.geometry.generate_solid_mesh().triangles.size());
    }
    mesh_count_ = mcnt;
    triangle_count_ = tcnt;
    emit sceneUpdated();
    update();
}

QVariantMap ShunyaViewport3DItem::inspectSelectedMesh() const {
    QVariantMap out;
    const auto* target = getSelectedObject();
    if (!target) return out;

    const auto mesh = target->geometry.generate_solid_mesh();
    const auto rep = mesh.analyze_manifold();
    const auto bb = mesh.compute_bbox().size();
    out["name"] = QString::fromStdString(target->name);
    out["vertices"] = static_cast<int>(rep.vertex_count);
    out["triangles"] = static_cast<int>(rep.face_count);
    out["edges"] = static_cast<int>(rep.edge_count);
    out["watertight"] = rep.is_watertight;
    out["manifold"] = rep.is_2_manifold;
    out["bboxX"] = bb.x;
    out["bboxY"] = bb.y;
    out["bboxZ"] = bb.z;
    out["volume"] = target->computed.volume_m3;
    out["surfaceArea"] = target->computed.surface_area_m2;
    out["mass"] = target->computed.mass_kg;
    out["density"] = target->computed.effective_density_kg_m3;
    return out;
}

// -----------------------------------------------------------------------------
// Gizmo Interaction Engine
// -----------------------------------------------------------------------------
QVariantMap ShunyaViewport3DItem::getGizmoProjection(double viewportW, double viewportH) {
    QVariantMap res;
    res["visible"] = false;
    const double W = std::max(1.0, viewportW);
    const double H = std::max(1.0, viewportH);

    const auto* target = getSelectedObject();
    core::base::math::Vector3<double> pos{0.0, 0.0, 0.0};
    if (target) pos = target->geometry.position;

    const double cy = std::cos(yaw_deg_ * kDegToRad);
    const double sy = std::sin(yaw_deg_ * kDegToRad);
    const double cp = std::cos(pitch_deg_ * kDegToRad);
    const double sp = std::sin(pitch_deg_ * kDegToRad);
    const double focal = std::min(W, H) * 1.35;

    auto project = [&](double wx, double wy, double wz, float& sx, float& sy_out) -> bool {
        const double x = wx - pan_x_;
        const double y = wy - pan_y_;
        const double z = wz;
        const double rx = x * cy + z * sy;
        const double rz1 = -x * sy + z * cy;
        const double ry = y * cp - rz1 * sp;
        const double rz = y * sp + rz1 * cp - dist_;
        if (!is_orthographic_ && rz >= -0.05) return false;
        if (is_orthographic_) {
            const double scale = focal / dist_;
            sx = static_cast<float>(W * 0.5 + rx * scale);
            sy_out = static_cast<float>(H * 0.5 - ry * scale);
        } else {
            const double inv = focal / (-rz);
            sx = static_cast<float>(W * 0.5 + rx * inv);
            sy_out = static_cast<float>(H * 0.5 - ry * inv);
        }
        return true;
    };

    float ox, oy, xx, xy, yx, yy, zx, zy;
    const double len = 0.08;
    if (project(pos.x, pos.y, pos.z, ox, oy) &&
        project(pos.x + len, pos.y, pos.z, xx, xy) &&
        project(pos.x, pos.y + len, pos.z, yx, yy) &&
        project(pos.x, pos.y, pos.z + len, zx, zy)) {
        res["visible"] = true;
        res["originX"] = ox; res["originY"] = oy;
        res["xTipX"] = xx;   res["xTipY"] = xy;
        res["yTipX"] = yx;   res["yTipY"] = yy;
        res["zTipX"] = zx;   res["zTipY"] = zy;
        res["xyMidX"] = (xx + yx) * 0.5; res["xyMidY"] = (xy + yy) * 0.5;
        res["yzMidX"] = (yx + zx) * 0.5; res["yzMidY"] = (yy + zy) * 0.5;
        res["zxMidX"] = (zx + xx) * 0.5; res["zxMidY"] = (zy + xy) * 0.5;
    }
    return res;
}

QString ShunyaViewport3DItem::testGizmoHover(double mouseX, double mouseY, double viewportW, double viewportH) {
    const auto proj = getGizmoProjection(viewportW, viewportH);
    if (!proj["visible"].toBool()) return "none";

    const double ox = proj["originX"].toDouble();
    const double oy = proj["originY"].toDouble();
    const double xx = proj["xTipX"].toDouble(), xy = proj["xTipY"].toDouble();
    const double yx = proj["yTipX"].toDouble(), yy = proj["yTipY"].toDouble();
    const double zx = proj["zTipX"].toDouble(), zy = proj["zTipY"].toDouble();

    auto distToSeg = [&](double px, double py, double x1, double y1, double x2, double y2) -> double {
        const double dx = x2 - x1, dy = y2 - y1;
        const double l2 = dx * dx + dy * dy;
        if (l2 < 1e-4) return std::hypot(px - x1, py - y1);
        const double t = std::clamp(((px - x1) * dx + (py - y1) * dy) / l2, 0.0, 1.0);
        return std::hypot(px - (x1 + t * dx), py - (y1 + t * dy));
    };

    if (distToSeg(mouseX, mouseY, ox, oy, xx, xy) < 14.0) return "X";
    if (distToSeg(mouseX, mouseY, ox, oy, yx, yy) < 14.0) return "Y";
    if (distToSeg(mouseX, mouseY, ox, oy, zx, zy) < 14.0) return "Z";
    if (std::hypot(mouseX - proj["xyMidX"].toDouble(), mouseY - proj["xyMidY"].toDouble()) < 16.0) return "XY";
    if (std::hypot(mouseX - proj["yzMidX"].toDouble(), mouseY - proj["yzMidY"].toDouble()) < 16.0) return "YZ";
    if (std::hypot(mouseX - proj["zxMidX"].toDouble(), mouseY - proj["zxMidY"].toDouble()) < 16.0) return "ZX";

    return "none";
}

void ShunyaViewport3DItem::applyGizmoDrag(const QString& axis, double dx, double dy, double viewportW, double viewportH) {
    auto* target = getSelectedObjectMut();
    if (!target) return;
    (void)viewportW; (void)viewportH;

    const double scaleFactor = 0.001 * dist_;
    if (gizmo_mode_ == "translate") {
        if (axis == "X") target->geometry.position.x += dx * scaleFactor;
        else if (axis == "Y") target->geometry.position.y -= dy * scaleFactor;
        else if (axis == "Z") target->geometry.position.z += (dx - dy) * 0.707 * scaleFactor;
        else if (axis == "XY") {
            target->geometry.position.x += dx * scaleFactor;
            target->geometry.position.y -= dy * scaleFactor;
        }

        if (snap_enabled_ && grid_step_ > 1e-4) {
            target->geometry.position.x = std::round(target->geometry.position.x / grid_step_) * grid_step_;
            target->geometry.position.y = std::round(target->geometry.position.y / grid_step_) * grid_step_;
            target->geometry.position.z = std::round(target->geometry.position.z / grid_step_) * grid_step_;
        }
    } else if (gizmo_mode_ == "rotate") {
        const double rotSensitivity = 0.5;
        if (axis == "X") target->geometry.rotation_deg.x += dy * rotSensitivity;
        else if (axis == "Y") target->geometry.rotation_deg.y += dx * rotSensitivity;
        else if (axis == "Z") target->geometry.rotation_deg.z += (dx - dy) * 0.707 * rotSensitivity;

        if (snap_enabled_ && angle_step_deg_ > 1e-4) {
            target->geometry.rotation_deg.x = std::round(target->geometry.rotation_deg.x / angle_step_deg_) * angle_step_deg_;
            target->geometry.rotation_deg.y = std::round(target->geometry.rotation_deg.y / angle_step_deg_) * angle_step_deg_;
            target->geometry.rotation_deg.z = std::round(target->geometry.rotation_deg.z / angle_step_deg_) * angle_step_deg_;
        }
    } else if (gizmo_mode_ == "scale") {
        const double deltaS = 1.0 + (dx - dy) * 0.005;
        if (axis == "X") target->geometry.scale.x = std::max(0.01, target->geometry.scale.x * deltaS);
        else if (axis == "Y") target->geometry.scale.y = std::max(0.01, target->geometry.scale.y * deltaS);
        else if (axis == "Z") target->geometry.scale.z = std::max(0.01, target->geometry.scale.z * deltaS);
        else {
            target->geometry.scale.x = std::max(0.01, target->geometry.scale.x * deltaS);
            target->geometry.scale.y = std::max(0.01, target->geometry.scale.y * deltaS);
            target->geometry.scale.z = std::max(0.01, target->geometry.scale.z * deltaS);
        }
    }

    syncObjectProperties(target, ws_);
    emit sceneUpdated();
    update();
}

// -----------------------------------------------------------------------------
// Submesh Selection and Modeling Tools
// -----------------------------------------------------------------------------
void ShunyaViewport3DItem::selectAll() {
    const auto* target = getSelectedObject();
    if (!target) return;
    const auto mesh = target->geometry.generate_solid_mesh();
    submesh_selection_.select_all(mesh);
    emit submeshSelectionChanged();
    update();
}

void ShunyaViewport3DItem::deselectAll() {
    submesh_selection_.clear();
    emit submeshSelectionChanged();
    update();
}

void ShunyaViewport3DItem::invertSelection() {
    const auto* target = getSelectedObject();
    if (!target) return;
    const auto mesh = target->geometry.generate_solid_mesh();

    if (selection_mask_ == "face") {
        std::vector<uint32_t> inv;
        for (uint32_t i = 0; i < mesh.triangles.size(); ++i) {
            if (!submesh_selection_.is_face_selected(i)) inv.push_back(i);
        }
        submesh_selection_.selected_faces = std::move(inv);
    } else if (selection_mask_ == "vertex") {
        std::vector<uint32_t> inv;
        for (uint32_t i = 0; i < mesh.vertices.size(); ++i) {
            if (!submesh_selection_.is_vertex_selected(i)) inv.push_back(i);
        }
        submesh_selection_.selected_vertices = std::move(inv);
    }
    emit submeshSelectionChanged();
    update();
}

void ShunyaViewport3DItem::selectSubmeshElement(const QString& mask, int index, bool toggle) {
    if (index < 0) return;
    const uint32_t idx = static_cast<uint32_t>(index);
    if (mask == "face") submesh_selection_.select_face(idx, toggle);
    else if (mask == "vertex") submesh_selection_.select_vertex(idx, toggle);
    emit submeshSelectionChanged();
    update();
}

bool ShunyaViewport3DItem::extrudeSelected(double distance) {
    auto* target = getSelectedObjectMut();
    if (!target) return false;
    auto mesh = target->geometry.generate_solid_mesh();

    std::vector<uint32_t> faces = submesh_selection_.selected_faces;
    if (faces.empty()) {
        faces.push_back(0); // default first face if none selected
    }

    cad3d::ExtrudeOptions opt;
    opt.distance = distance;
    if (!cad3d::MeshTopologyOps::extrude_faces(mesh, faces, opt)) return false;

    target->geometry.kind = eng::ObjectShapeKind::Mesh3D;
    target->geometry.mesh = mesh;
    syncObjectProperties(target, ws_);
    refreshScene();
    return true;
}

bool ShunyaViewport3DItem::insetSelected(double thickness) {
    auto* target = getSelectedObjectMut();
    if (!target) return false;
    auto mesh = target->geometry.generate_solid_mesh();

    std::vector<uint32_t> faces = submesh_selection_.selected_faces;
    if (faces.empty()) faces.push_back(0);

    cad3d::InsetOptions opt;
    opt.thickness = thickness;
    if (!cad3d::MeshTopologyOps::inset_faces(mesh, faces, opt)) return false;

    target->geometry.kind = eng::ObjectShapeKind::Mesh3D;
    target->geometry.mesh = mesh;
    syncObjectProperties(target, ws_);
    refreshScene();
    return true;
}

bool ShunyaViewport3DItem::bevelSelected(double offset, int segments) {
    auto* target = getSelectedObjectMut();
    if (!target) return false;
    auto mesh = target->geometry.generate_solid_mesh();

    std::vector<std::pair<uint32_t, uint32_t>> edges = submesh_selection_.selected_edges;
    if (edges.empty() && !mesh.triangles.empty()) {
        edges.push_back({mesh.triangles[0][0], mesh.triangles[0][1]});
    }

    cad3d::BevelOptions opt;
    opt.offset = offset;
    opt.segments = static_cast<uint32_t>(std::max(1, segments));
    if (!cad3d::MeshTopologyOps::bevel(mesh, edges, submesh_selection_.selected_vertices, opt)) return false;

    target->geometry.kind = eng::ObjectShapeKind::Mesh3D;
    target->geometry.mesh = mesh;
    syncObjectProperties(target, ws_);
    refreshScene();
    return true;
}

bool ShunyaViewport3DItem::loopCutSelected(double slide) {
    auto* target = getSelectedObjectMut();
    if (!target) return false;
    auto mesh = target->geometry.generate_solid_mesh();
    if (mesh.triangles.empty()) return false;

    cad3d::LoopCutOptions opt;
    opt.cuts = 1;
    opt.slide = slide;
    if (!cad3d::MeshTopologyOps::loop_cut(mesh, mesh.triangles[0][0], mesh.triangles[0][1], opt)) return false;

    target->geometry.kind = eng::ObjectShapeKind::Mesh3D;
    target->geometry.mesh = mesh;
    syncObjectProperties(target, ws_);
    refreshScene();
    return true;
}

bool ShunyaViewport3DItem::subdivideSelected() {
    auto* target = getSelectedObjectMut();
    if (!target) return false;
    auto mesh = target->geometry.generate_solid_mesh();

    auto refined = cad3d::SubdivisionEngine::subdivide_loop(mesh, 1);
    target->geometry.kind = eng::ObjectShapeKind::Mesh3D;
    target->geometry.mesh = refined;
    syncObjectProperties(target, ws_);
    refreshScene();
    return true;
}

int ShunyaViewport3DItem::mergeSelectedVertices(double distance) {
    auto* target = getSelectedObjectMut();
    if (!target) return 0;
    auto mesh = target->geometry.generate_solid_mesh();

    size_t count = cad3d::MeshTopologyOps::merge_vertices(
        mesh,
        submesh_selection_.selected_vertices,
        cad3d::VertexMergeMode::ByDistance,
        distance
    );

    target->geometry.kind = eng::ObjectShapeKind::Mesh3D;
    target->geometry.mesh = mesh;
    syncObjectProperties(target, ws_);
    refreshScene();
    return static_cast<int>(count);
}

// -----------------------------------------------------------------------------
// Interactive Measurement & Section Analysis Tools
// -----------------------------------------------------------------------------
double ShunyaViewport3DItem::measureDistance(double x1, double y1, double z1, double x2, double y2, double z2) const {
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double dz = z2 - z1;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double ShunyaViewport3DItem::measureAngle(double ax, double ay, double az,
                                         double bx, double by, double bz,
                                         double cx, double cy, double cz) const {
    const double v1x = ax - bx, v1y = ay - by, v1z = az - bz;
    const double v2x = cx - bx, v2y = cy - by, v2z = cz - bz;
    const double l1 = std::sqrt(v1x * v1x + v1y * v1y + v1z * v1z);
    const double l2 = std::sqrt(v2x * v2x + v2y * v2y + v2z * v2z);
    if (l1 < 1e-9 || l2 < 1e-9) return 0.0;
    const double dot = (v1x * v2x + v1y * v2y + v1z * v2z) / (l1 * l2);
    return std::acos(std::clamp(dot, -1.0, 1.0)) / kDegToRad;
}

double ShunyaViewport3DItem::inspectSectionWallThickness(const QString& plane, double offset) const {
    (void)plane; (void)offset;
    const auto* target = getSelectedObject();
    if (!target) return 0.001;
    if (target->geometry.wall_thickness_m > 1e-6) {
        return target->geometry.wall_thickness_m;
    }
    return 0.0008; // default 0.8 mm wall thickness
}

void ShunyaViewport3DItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) update();
}

// -----------------------------------------------------------------------------
// Paint Rendering Pipeline
// -----------------------------------------------------------------------------
void ShunyaViewport3DItem::paint(QPainter* p) {
    const double W = std::max(1.0, width());
    const double H = std::max(1.0, height());
    p->setRenderHint(QPainter::Antialiasing, true);

    const double cy = std::cos(yaw_deg_ * kDegToRad);
    const double sy = std::sin(yaw_deg_ * kDegToRad);
    const double cp = std::cos(pitch_deg_ * kDegToRad);
    const double sp = std::sin(pitch_deg_ * kDegToRad);
    const double focal = std::min(W, H) * 1.35;

    auto project = [&](double wx, double wy, double wz, float& sx, float& sy_out, double& camZ) -> bool {
        const double x = wx - pan_x_;
        const double y = wy - pan_y_;
        const double z = wz;
        const double rx = x * cy + z * sy;
        const double rz1 = -x * sy + z * cy;
        const double ry = y * cp - rz1 * sp;
        const double rz = y * sp + rz1 * cp - dist_;
        camZ = rz;
        if (!is_orthographic_ && rz >= -0.05) return false;
        if (is_orthographic_) {
            const double scale = focal / dist_;
            sx = static_cast<float>(W * 0.5 + rx * scale);
            sy_out = static_cast<float>(H * 0.5 - ry * scale);
        } else {
            const double inv = focal / (-rz);
            sx = static_cast<float>(W * 0.5 + rx * inv);
            sy_out = static_cast<float>(H * 0.5 - ry * inv);
        }
        return true;
    };

    // 1. Dynamic Ground Grid with scale lines
    const QColor gridC = dark_mode_ ? QColor("#282828") : QColor("#DCDCDC");
    p->setPen(QPen(gridC, 1.0));
    for (int i = -6; i <= 6; ++i) {
        const double v = i * 0.08;
        float x0, y0, x1, y1; double d0, d1;
        if (project(-0.48, 0.0, v, x0, y0, d0) && project(0.48, 0.0, v, x1, y1, d1)) {
            p->drawLine(QPointF(x0, y0), QPointF(x1, y1));
        }
        if (project(v, 0.0, -0.48, x0, y0, d0) && project(v, 0.0, 0.48, x1, y1, d1)) {
            p->drawLine(QPointF(x0, y0), QPointF(x1, y1));
        }
    }

    // Grid Scale Text ("100 mm Grid")
    p->setFont(QFont("Segoe UI", 9));
    p->setPen(dark_mode_ ? QColor("#666666") : QColor("#888888"));
    p->drawText(QRectF(14, H - 28, 120, 20), Qt::AlignLeft | Qt::AlignVCenter, "Grid: 80 mm");

    // 2. Render Objects
    std::vector<ProjectedTri> tris;
    if (ws_) {
        const auto& objs = ws_->object_manager().objects();
        for (size_t idx = 0; idx < objs.size(); ++idx) {
            const auto& obj = objs[idx];
            cad3d::SolidMesh3D mesh = obj.geometry.generate_solid_mesh();
            auto pos = obj.geometry.position;
            if (objs.size() > 1 && std::abs(pos.x) < 1e-6 && std::abs(pos.z) < 1e-6) {
                pos.x = (static_cast<double>(idx) - 0.5 * (objs.size() - 1)) * 0.14;
            }

            double tempK = obj.operating_temp_k;
            double thermScale = 1.0;
            if (physics_active_) {
                if (const auto* body = ws_->physics_runtime().find_body(obj.id)) {
                    pos = body->position;
                    tempK = body->temperature_k;
                    thermScale = 1.0 + body->thermal_strain;
                }
            }

            const bool isSelectedObj = (!selected_id_.isEmpty() && selected_id_.toStdString() == obj.id.to_string());
            QColor baseCol = show_thermal_
                ? thermalColor(tempK)
                : materialBaseColor(obj, ws_->material_registry(), dark_mode_);
            if (isSelectedObj) {
                baseCol = baseCol.lighter(115);
            }

            // Alpha modulation for X-Ray mode
            if (display_mode_ == "xray") {
                baseCol.setAlpha(95);
            }

            const double rx_d = obj.geometry.rotation_deg.x * kDegToRad;
            const double ry_d = obj.geometry.rotation_deg.y * kDegToRad;
            const double rz_d = obj.geometry.rotation_deg.z * kDegToRad;
            const auto& sc = obj.geometry.scale;

            auto xform = [&](const cad3d::Point3D& pt) -> cad3d::Point3D {
                double x = pt.x * sc.x * thermScale;
                double y = pt.y * sc.y * thermScale;
                double z = pt.z * sc.z * thermScale;
                if (std::abs(rx_d) > 1e-6) {
                    const double y1 = y * std::cos(rx_d) - z * std::sin(rx_d);
                    const double z1 = y * std::sin(rx_d) + z * std::cos(rx_d);
                    y = y1; z = z1;
                }
                if (std::abs(ry_d) > 1e-6) {
                    const double x1 = x * std::cos(ry_d) + z * std::sin(ry_d);
                    const double z1 = -x * std::sin(ry_d) + z * std::cos(ry_d);
                    x = x1; z = z1;
                }
                if (std::abs(rz_d) > 1e-6) {
                    const double x1 = x * std::cos(rz_d) - y * std::sin(rz_d);
                    const double y1 = x * std::sin(rz_d) + y * std::cos(rz_d);
                    x = x1; y = y1;
                }
                return {x + pos.x, y + pos.y, z + pos.z};
            };

            const QColor edgeCol = wireframe() ? baseCol : (dark_mode_ ? QColor("#222222") : QColor("#444444"));

            for (uint32_t f_idx = 0; f_idx < mesh.triangles.size(); ++f_idx) {
                const auto& tri = mesh.triangles[f_idx];
                const auto p0 = xform(mesh.vertices[tri[0]]);
                const auto p1 = xform(mesh.vertices[tri[1]]);
                const auto p2 = xform(mesh.vertices[tri[2]]);

                // Section clipping plane filter
                if (display_mode_ == "section") {
                    double coord = (section_plane_ == "YZ") ? (p0.x + p1.x + p2.x) / 3.0 :
                                   (section_plane_ == "ZX") ? (p0.y + p1.y + p2.y) / 3.0 :
                                                              (p0.z + p1.z + p2.z) / 3.0;
                    if (coord > section_offset_) continue; // clipped by section plane
                }

                float sx0, sy0, sx1, sy1, sx2, sy2;
                double d0, d1, d2;
                if (!project(p0.x, p0.y, p0.z, sx0, sy0, d0) ||
                    !project(p1.x, p1.y, p1.z, sx1, sy1, d1) ||
                    !project(p2.x, p2.y, p2.z, sx2, sy2, d2)) {
                    continue;
                }

                const double ux = p1.x - p0.x, uy = p1.y - p0.y, uz = p1.z - p0.z;
                const double vx = p2.x - p0.x, vy = p2.y - p0.y, vz = p2.z - p0.z;
                double nx = uy * vz - uz * vy;
                double ny = uz * vx - ux * vz;
                double nz = ux * vy - uy * vx;
                const double nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (nlen > 1e-12) { nx /= nlen; ny /= nlen; nz /= nlen; }

                const double ndotl = std::abs(nx * 0.45 + ny * 0.78 + nz * 0.42);
                const double shade = 0.42 + 0.58 * ndotl;

                bool is_face_sel = (isSelectedObj && interaction_mode_ == "edit" && submesh_selection_.is_face_selected(f_idx));
                QColor shaded = is_face_sel ? QColor(229, 192, 123, 190) : QColor(
                    static_cast<int>(std::clamp(baseCol.red() * shade, 0.0, 255.0)),
                    static_cast<int>(std::clamp(baseCol.green() * shade, 0.0, 255.0)),
                    static_cast<int>(std::clamp(baseCol.blue() * shade, 0.0, 255.0)),
                    baseCol.alpha());

                QColor edg = is_face_sel ? QColor("#E5C07B") : edgeCol;
                tris.push_back({sx0, sy0, sx1, sy1, sx2, sy2, (d0 + d1 + d2) / 3.0, shaded, edg, is_face_sel});
            }
        }
    }

    // Depth sort far-to-near
    std::sort(tris.begin(), tris.end(), [](const ProjectedTri& a, const ProjectedTri& b) {
        return a.depth < b.depth;
    });

    for (const auto& t : tris) {
        QPolygonF poly;
        poly << QPointF(t.x0, t.y0) << QPointF(t.x1, t.y1) << QPointF(t.x2, t.y2);
        if (wireframe()) {
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(t.edge, 1.1));
        } else {
            p->setBrush(t.fill);
            p->setPen(QPen(t.edge, t.is_selected ? 1.5 : 0.6));
        }
        p->drawPolygon(poly);
    }

    // 3. Interactive Transform Gizmo Screen Rendering
    if (gizmo_mode_ != "none") {
        const auto proj = getGizmoProjection(W, H);
        if (proj["visible"].toBool()) {
            const QPointF o(proj["originX"].toDouble(), proj["originY"].toDouble());
            const QPointF px(proj["xTipX"].toDouble(), proj["xTipY"].toDouble());
            const QPointF py(proj["yTipX"].toDouble(), proj["yTipY"].toDouble());
            const QPointF pz(proj["zTipX"].toDouble(), proj["zTipY"].toDouble());

            p->setRenderHint(QPainter::Antialiasing, true);
            // X Axis (Red)
            p->setPen(QPen(QColor("#EF4444"), 2.5));
            p->drawLine(o, px);
            p->setBrush(QColor("#EF4444"));
            p->drawEllipse(px, 3.5, 3.5);

            // Y Axis (Green)
            p->setPen(QPen(QColor("#22C55E"), 2.5));
            p->drawLine(o, py);
            p->setBrush(QColor("#22C55E"));
            p->drawEllipse(py, 3.5, 3.5);

            // Z Axis (Blue)
            p->setPen(QPen(QColor("#3B82F6"), 2.5));
            p->drawLine(o, pz);
            p->setBrush(QColor("#3B82F6"));
            p->drawEllipse(pz, 3.5, 3.5);

            // Origin core
            p->setPen(Qt::NoPen);
            p->setBrush(QColor("#FFFFFF"));
            p->drawEllipse(o, 4.0, 4.0);
        }
    }

    // 4. Coordinate triad at bottom-right
    const QPointF org(W - 44.0, H - 44.0);
    auto drawAxis = [&](double dx, double dy, double dz, const QColor& col, const QString& label) {
        const double rx = dx * cy + dz * sy;
        const double rz1 = -dx * sy + dz * cy;
        const double ry = dy * cp - rz1 * sp;
        const QPointF tip(org.x() + rx * 24.0, org.y() - ry * 24.0);
        p->setPen(QPen(col, 2.0));
        p->drawLine(org, tip);
        p->drawText(tip + QPointF(3, 3), label);
    };
    drawAxis(1, 0, 0, QColor("#EF4444"), "X");
    drawAxis(0, 1, 0, QColor("#22C55E"), "Y");
    drawAxis(0, 0, 1, QColor("#3B82F6"), "Z");
}

} // namespace shunya::gui
