#include "viewport_3d_item.hpp"

#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <vector>

namespace shunya::gui {

namespace {

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

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

struct ProjectedTri {
    float x0, y0, x1, y1, x2, y2;
    double depth;
    QColor fill;
    QColor edge;
};

std::weak_ptr<eng::EngineeringWorkspaceState> s_default_ws_3d;

} // namespace

void ShunyaViewport3DItem::setDefaultWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws) {
    s_default_ws_3d = ws;
}

ShunyaViewport3DItem::ShunyaViewport3DItem(QQuickItem* parent)
    : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setOpaquePainting(false);
    if (auto def = s_default_ws_3d.lock()) {
        setWorkspace(std::move(def));
    }
}

void ShunyaViewport3DItem::setWorkspace(std::shared_ptr<eng::EngineeringWorkspaceState> ws) {
    ws_ = std::move(ws);
    if (ws_) {
        ws_->subscribe([this](const eng::WorkspaceChangeEvent&) {
            refreshScene();
        });
    }
    refreshScene();
}

void ShunyaViewport3DItem::orbitBy(double dyawDeg, double dpitchDeg) {
    yaw_deg_ += dyawDeg;
    pitch_deg_ = std::clamp(pitch_deg_ + dpitchDeg, -85.0, 85.0);
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::panBy(double dxPx, double dyPx) {
    const double scale = dist_ / std::max(100.0, height());
    pan_x_ -= dxPx * scale;
    pan_y_ += dyPx * scale;
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::zoomBy(double factor) {
    dist_ = std::clamp(dist_ * factor, 0.12, 30.0);
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::snapView(const QString& face) {
    if (face == "Front") { yaw_deg_ = 0.0; pitch_deg_ = 0.0; }
    else if (face == "Top") { yaw_deg_ = 0.0; pitch_deg_ = 85.0; }
    else if (face == "Right") { yaw_deg_ = 90.0; pitch_deg_ = 0.0; }
    else if (face == "Left") { yaw_deg_ = -90.0; pitch_deg_ = 0.0; }
    else { yaw_deg_ = 38.0; pitch_deg_ = 26.0; }
    emit viewChanged();
    update();
}

void ShunyaViewport3DItem::resetCamera() {
    yaw_deg_ = 38.0;
    pitch_deg_ = 26.0;
    dist_ = 0.55;
    pan_x_ = 0.0;
    pan_y_ = 0.0;
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
    if (!ws_) return out;
    const auto uid = core::base::Uuid::from_string(selected_id_.toStdString());
    const eng::PhysicalObject* target = uid ? ws_->object_manager().get_object(*uid) : nullptr;
    if (!target && !ws_->object_manager().objects().empty()) {
        target = &ws_->object_manager().objects().front();
    }
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

void ShunyaViewport3DItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) update();
}

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
        if (rz >= -0.05) return false;
        const double inv = focal / (-rz);
        sx = static_cast<float>(W * 0.5 + rx * inv);
        sy_out = static_cast<float>(H * 0.5 - ry * inv);
        return true;
    };

    // Floor grid lines
    const QColor gridC = dark_mode_ ? QColor("#282828") : QColor("#DCDCDC");
    p->setPen(QPen(gridC, 1.0));
    for (int i = -5; i <= 5; ++i) {
        const double v = i * 0.08;
        float x0, y0, x1, y1; double d0, d1;
        if (project(-0.4, 0.0, v, x0, y0, d0) && project(0.4, 0.0, v, x1, y1, d1)) {
            p->drawLine(QPointF(x0, y0), QPointF(x1, y1));
        }
        if (project(v, 0.0, -0.4, x0, y0, d0) && project(v, 0.0, 0.4, x1, y1, d1)) {
            p->drawLine(QPointF(x0, y0), QPointF(x1, y1));
        }
    }

    // Collect projected triangles from all workspace PhysicalObjects
    std::vector<ProjectedTri> tris;
    if (ws_) {
        const auto& objs = ws_->object_manager().objects();
        for (size_t idx = 0; idx < objs.size(); ++idx) {
            const auto& obj = objs[idx];
            cad3d::SolidMesh3D mesh = obj.geometry.generate_solid_mesh();
            auto pos = obj.geometry.position;
            // Spread co-located objects cleanly if all positioned at origin so user sees every body
            if (objs.size() > 1 && std::abs(pos.x) < 1e-6 && std::abs(pos.z) < 1e-6) {
                pos.x = (static_cast<double>(idx) - 0.5 * (objs.size() - 1)) * 0.14;
            }
            double tempK = obj.operating_temp_k;
            double thermScale = 1.0;

            if (physics_active_) {
                if (const auto* body = ws_->physics_runtime().find_body(obj.id)) {
                    pos = body->position;
                    if (objs.size() > 1 && std::abs(pos.x) < 1e-6 && std::abs(pos.z) < 1e-6) {
                        pos.x = (static_cast<double>(idx) - 0.5 * (objs.size() - 1)) * 0.14;
                    }
                    tempK = body->temperature_k;
                    thermScale = 1.0 + body->thermal_strain;
                }
            }

            QColor baseCol = show_thermal_
                ? thermalColor(tempK)
                : materialBaseColor(obj, ws_->material_registry(), dark_mode_);
            if (!selected_id_.isEmpty() && selected_id_.toStdString() == obj.id.to_string()) {
                baseCol = baseCol.lighter(120);
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

            const QColor edgeCol = wireframe_ ? baseCol : (dark_mode_ ? QColor("#202020") : QColor("#303030"));

            for (const auto& tri : mesh.triangles) {
                const auto p0 = xform(mesh.vertices[tri[0]]);
                const auto p1 = xform(mesh.vertices[tri[1]]);
                const auto p2 = xform(mesh.vertices[tri[2]]);

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

                QColor shaded(
                    static_cast<int>(std::clamp(baseCol.red() * shade, 0.0, 255.0)),
                    static_cast<int>(std::clamp(baseCol.green() * shade, 0.0, 255.0)),
                    static_cast<int>(std::clamp(baseCol.blue() * shade, 0.0, 255.0)));

                tris.push_back({sx0, sy0, sx1, sy1, sx2, sy2, (d0 + d1 + d2) / 3.0, shaded, edgeCol});
            }
        }
    }

    // Painter's algorithm depth sort far-to-near
    std::sort(tris.begin(), tris.end(), [](const ProjectedTri& a, const ProjectedTri& b) {
        return a.depth < b.depth;
    });

    for (const auto& t : tris) {
        QPolygonF poly;
        poly << QPointF(t.x0, t.y0) << QPointF(t.x1, t.y1) << QPointF(t.x2, t.y2);
        if (wireframe_) {
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(t.edge, 1.1));
        } else {
            p->setBrush(t.fill);
            p->setPen(QPen(t.edge, 0.6));
        }
        p->drawPolygon(poly);
    }

    // Coordinate triad at bottom-right
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
