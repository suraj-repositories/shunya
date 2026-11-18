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

// =============================================================================
// VertexMask Implementation (PH4-11.02.004)
// =============================================================================
void VertexMask::paint(
    const SolidMesh3D& mesh,
    const StrokePoint& stroke,
    double radius,
    double strength,
    bool invert_stroke
) noexcept {
    if (mesh.vertices.empty() || radius <= 0.0 || strength <= 0.0) return;
    if (values_.size() != mesh.vertices.size()) {
        values_.resize(mesh.vertices.size(), 0.0);
    }

    const double delta_sign = invert_stroke ? -1.0 : 1.0;

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const double dist = (mesh.vertices[i] - stroke.position).length();
        if (dist > radius) continue;

        const double falloff = SculptEngine::compute_falloff(dist, radius);
        const double delta = falloff * strength * delta_sign;
        values_[i] = std::clamp(values_[i] + delta, 0.0, 1.0);
    }
}

// =============================================================================
// SculptEngine Implementation
// =============================================================================
SculptResult SculptEngine::apply_stroke(SolidMesh3D& mesh, const StrokePoint& stroke, VertexMask* mask) const {
    SculptResult result;
    if (mesh.vertices.empty() || config_.radius <= 0.0 || config_.strength <= 0.0) {
        return result;
    }

    if (mask && mask->size() != mesh.vertices.size()) {
        mask->resize(mesh.vertices.size(), 0.0);
    }

    std::vector<bool> modified(mesh.vertices.size(), false);
    double max_disp = 0.0;

    // 1. Primary stroke
    apply_single_pass(mesh, stroke, modified, max_disp, mask);

    // 2. Bilateral X-axis mirror symmetry (PH4-11.02.003)
    if (config_.symmetry_x) {
        StrokePoint sym_stroke;
        sym_stroke.position = Point3D(-stroke.position.x, stroke.position.y, stroke.position.z);
        sym_stroke.normal = Vec3d(-stroke.normal.x, stroke.normal.y, stroke.normal.z);
        sym_stroke.motion_vector = Vec3d(-stroke.motion_vector.x, stroke.motion_vector.y, stroke.motion_vector.z);

        apply_single_pass(mesh, sym_stroke, modified, max_disp, mask);
    }

    uint32_t count = 0;
    for (size_t i = 0; i < modified.size(); ++i) {
        if (modified[i]) {
            ++count;
            if (config_.symmetry_x && std::abs(mesh.vertices[i].x) < config_.symmetry_tolerance) {
                mesh.vertices[i].x = 0.0;
            }
        }
    }

    if (count > 0 && config_.type != BrushType::Mask) {
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
    double& out_max_disp,
    VertexMask* mask
) const {
    const double r = config_.radius;
    const double sign = config_.invert ? -1.0 : 1.0;
    const Vec3d stroke_norm = (stroke.normal.length_squared() > 1e-12) ? stroke.normal.normalized() : Vec3d(0, 0, 1);

    if (config_.type == BrushType::Mask) {
        if (mask) {
            mask->paint(mesh, stroke, config_.radius, config_.strength, config_.invert);
        }
        return;
    }

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

        // Mask attenuation (PH4-11.02.004): 1.0 = fully protected
        const double mask_factor = mask ? (1.0 - mask->get(i)) : 1.0;
        if (mask_factor <= 1e-6) continue;

        const double falloff = compute_falloff(dist, r);
        const double w = falloff * config_.strength * mask_factor;
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
            default:
                break;
        }

        const double disp = (new_pos - orig_pos).length();
        if (disp > 1e-9) {
            v = new_pos;
            modified_mask[i] = true;
            out_max_disp = std::max(out_max_disp, disp);
        }
    }
}

// =============================================================================
// SurfaceSmoother Implementation (PH4-11.02.005)
// =============================================================================
void SurfaceSmoother::laplacian_smooth(SolidMesh3D& mesh, uint32_t iterations, double lambda) {
    if (mesh.vertices.empty() || mesh.triangles.empty() || iterations == 0) return;
    const double factor = std::clamp(lambda, 0.0, 1.0);

    // Build vertex-to-neighbor adjacency
    std::vector<std::set<uint32_t>> adj(mesh.vertices.size());
    for (const auto& tri : mesh.triangles) {
        for (int i = 0; i < 3; ++i) {
            uint32_t u = tri[i];
            uint32_t v = tri[(i + 1) % 3];
            adj[u].insert(v);
            adj[v].insert(u);
        }
    }

    std::vector<Point3D> current = mesh.vertices;
    std::vector<Point3D> next = current;

    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (size_t i = 0; i < current.size(); ++i) {
            if (adj[i].empty()) continue;

            Point3D avg(0, 0, 0);
            for (uint32_t neighbor : adj[i]) {
                avg = avg + current[neighbor];
            }
            avg = avg * (1.0 / static_cast<double>(adj[i].size()));
            next[i] = current[i] + (avg - current[i]) * factor;
        }
        current = next;
    }

    mesh.vertices = std::move(current);
    recompute_solid_normals(mesh);
}

void SurfaceSmoother::taubin_smooth(SolidMesh3D& mesh, uint32_t iterations, double lambda, double mu) {
    if (mesh.vertices.empty() || mesh.triangles.empty() || iterations == 0) return;

    std::vector<std::set<uint32_t>> adj(mesh.vertices.size());
    for (const auto& tri : mesh.triangles) {
        for (int i = 0; i < 3; ++i) {
            uint32_t u = tri[i];
            uint32_t v = tri[(i + 1) % 3];
            adj[u].insert(v);
            adj[v].insert(u);
        }
    }

    std::vector<Point3D> current = mesh.vertices;
    std::vector<Point3D> next = current;

    auto step = [&](double step_val) {
        for (size_t i = 0; i < current.size(); ++i) {
            if (adj[i].empty()) continue;
            Point3D avg(0, 0, 0);
            for (uint32_t nbr : adj[i]) {
                avg = avg + current[nbr];
            }
            avg = avg * (1.0 / static_cast<double>(adj[i].size()));
            next[i] = current[i] + (avg - current[i]) * step_val;
        }
        current = next;
    };

    for (uint32_t iter = 0; iter < iterations; ++iter) {
        step(lambda); // Shrinking step
        step(mu);     // Unshrinking step
    }

    mesh.vertices = std::move(current);
    recompute_solid_normals(mesh);
}

void SurfaceSmoother::normal_smooth_pass(SolidMesh3D& mesh, double crease_angle_rad) {
    if (mesh.vertices.empty() || mesh.triangles.empty()) return;

    const double cos_threshold = std::cos(crease_angle_rad);

    // Compute per-triangle face normals and face areas
    const size_t tcount = mesh.triangles.size();
    std::vector<Vec3d> tri_normals(tcount);
    std::vector<double> tri_areas(tcount);

    for (size_t t = 0; t < tcount; ++t) {
        const auto& p0 = mesh.vertices[mesh.triangles[t][0]];
        const auto& p1 = mesh.vertices[mesh.triangles[t][1]];
        const auto& p2 = mesh.vertices[mesh.triangles[t][2]];
        Vec3d cross_prod = (p1 - p0).cross(p2 - p0);
        const double len = cross_prod.length();
        tri_areas[t] = len * 0.5;
        tri_normals[t] = (len > 1e-12) ? (cross_prod / len) : Vec3d(0, 0, 1);
    }

    // Vertex-to-triangle incidence list
    std::vector<std::vector<uint32_t>> v_triangles(mesh.vertices.size());
    for (size_t t = 0; t < tcount; ++t) {
        v_triangles[mesh.triangles[t][0]].push_back(static_cast<uint32_t>(t));
        v_triangles[mesh.triangles[t][1]].push_back(static_cast<uint32_t>(t));
        v_triangles[mesh.triangles[t][2]].push_back(static_cast<uint32_t>(t));
    }

    mesh.normals.resize(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        if (v_triangles[i].empty()) {
            mesh.normals[i] = Vec3d(0, 0, 1);
            continue;
        }

        // Use first triangle as normal reference
        const Vec3d& ref_norm = tri_normals[v_triangles[i][0]];
        Vec3d accumulated(0, 0, 0);

        for (uint32_t t_idx : v_triangles[i]) {
            const Vec3d& fn = tri_normals[t_idx];
            // If angle with reference face is within crease threshold, blend it
            if (fn.dot(ref_norm) >= cos_threshold) {
                accumulated += fn * tri_areas[t_idx];
            }
        }

        if (accumulated.length_squared() > 1e-12) {
            mesh.normals[i] = accumulated.normalized();
        } else {
            mesh.normals[i] = ref_norm;
        }
    }
}

} // namespace shunya::rig3d
