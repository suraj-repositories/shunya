#include "shunya/rig3d/sculpt_brush.hpp"

#include <cmath>
#include <map>
#include <set>

namespace shunya::rig3d {

static void recompute_solid_normals(SolidMesh3D& mesh) {
    mesh.normals.assign(mesh.vertices.size(), Vec3d(0.0, 0.0, 0.0));
    for (const auto& tri : mesh.triangles) {
        if (tri[0] >= mesh.vertices.size() || tri[1] >= mesh.vertices.size() || tri[2] >= mesh.vertices.size()) {
            continue;
        }
        const auto& p0 = mesh.vertices[tri[0]];
        const auto& p1 = mesh.vertices[tri[1]];
        const auto& p2 = mesh.vertices[tri[2]];
        const Vec3d fn = (p1 - p0).cross(p2 - p0);
        mesh.normals[tri[0]] += fn;
        mesh.normals[tri[1]] += fn;
        mesh.normals[tri[2]] += fn;
    }
    for (auto& n : mesh.normals) {
        if (n.length_squared() > 1e-12) {
            n = n.normalized();
        } else {
            n = Vec3d(0.0, 0.0, 1.0);
        }
    }
}

SculptResult SculptEngine::apply_stroke(SolidMesh3D& mesh, const StrokePoint& stroke) const {
    SculptResult result;
    if (mesh.vertices.empty() || config_.radius <= 0.0 || config_.strength <= 0.0) {
        return result;
    }

    std::vector<bool> modified(mesh.vertices.size(), false);
    double max_disp = 0.0;

    // 1. Primary stroke
    apply_single_pass(mesh, stroke, modified, max_disp);

    // 2. Bilateral X-axis mirror symmetry (PH4-11.02.003)
    if (config_.symmetry_x) {
        StrokePoint sym_stroke;
        sym_stroke.position = Point3D(-stroke.position.x, stroke.position.y, stroke.position.z);
        sym_stroke.normal = Vec3d(-stroke.normal.x, stroke.normal.y, stroke.normal.z);
        sym_stroke.motion_vector = Vec3d(-stroke.motion_vector.x, stroke.motion_vector.y, stroke.motion_vector.z);

        apply_single_pass(mesh, sym_stroke, modified, max_disp);
    }

    // Count modified vertices
    uint32_t count = 0;
    for (size_t i = 0; i < modified.size(); ++i) {
        if (modified[i]) {
            ++count;
            if (config_.symmetry_x && std::abs(mesh.vertices[i].x) < config_.symmetry_tolerance) {
                mesh.vertices[i].x = 0.0;
            }
        }
    }

    if (count > 0) {
        recompute_solid_normals(mesh);
    }

    result.vertices_modified = count;
    result.max_displacement = max_disp;
    return result;
}

void SculptEngine::apply_single_pass(
    SolidMesh3D& mesh,
    const StrokePoint& stroke,
    std::vector<bool>& modified_mask,
    double& out_max_disp
) const {
    const double r = config_.radius;
    const double sign = config_.invert ? -1.0 : 1.0;
    const Vec3d stroke_norm = (stroke.normal.length_squared() > 1e-12) ? stroke.normal.normalized() : Vec3d(0, 0, 1);

    // Build adjacency for smooth brush if needed
    std::vector<std::vector<uint32_t>> adj;
    if (config_.type == BrushType::Smooth) {
        adj.resize(mesh.vertices.size());
        for (const auto& tri : mesh.triangles) {
            uint32_t i0 = tri[0];
            uint32_t i1 = tri[1];
            uint32_t i2 = tri[2];
            adj[i0].push_back(i1); adj[i0].push_back(i2);
            adj[i1].push_back(i0); adj[i1].push_back(i2);
            adj[i2].push_back(i0); adj[i2].push_back(i1);
        }
    }

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        auto& v = mesh.vertices[i];
        const double dist = (v - stroke.position).length();
        if (dist > r) continue;

        const double falloff = compute_falloff(dist, r);
        const double w = falloff * config_.strength;
        if (w <= 1e-9) continue;

        Point3D orig_pos = v;
        Point3D new_pos = orig_pos;

        switch (config_.type) {
            case BrushType::Clay: {
                const double target_height = r * 0.15 * sign;
                const Point3D plane_pt = stroke.position + stroke_norm * target_height;
                const double delta = (plane_pt - orig_pos).dot(stroke_norm);
                new_pos = orig_pos + stroke_norm * (delta * w);
                break;
            }
            case BrushType::Smooth: {
                if (!adj[i].empty()) {
                    Point3D avg(0, 0, 0);
                    for (uint32_t neighbor : adj[i]) {
                        avg = avg + mesh.vertices[neighbor];
                    }
                    avg = avg * (1.0 / static_cast<double>(adj[i].size()));
                    new_pos = orig_pos + (avg - orig_pos) * w;
                }
                break;
            }
            case BrushType::Grab: {
                new_pos = orig_pos + stroke.motion_vector * w;
                break;
            }
            case BrushType::Inflate: {
                Vec3d vn = (i < mesh.normals.size()) ? mesh.normals[i] : stroke_norm;
                if (vn.length_squared() < 1e-12) vn = stroke_norm;
                else vn = vn.normalized();
                new_pos = orig_pos + vn * (r * 0.2 * sign * w);
                break;
            }
            case BrushType::Flatten: {
                const double delta = (stroke.position - orig_pos).dot(stroke_norm);
                new_pos = orig_pos + stroke_norm * (delta * w);
                break;
            }
        }

        const double disp = (new_pos - orig_pos).length();
        if (disp > 1e-9) {
            v = new_pos;
            modified_mask[i] = true;
            out_max_disp = std::max(out_max_disp, disp);
        }
    }
}

} // namespace shunya::rig3d
