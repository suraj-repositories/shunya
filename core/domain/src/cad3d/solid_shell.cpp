#include "shunya/cad3d/solid_shell.hpp"

#include "shunya/cad3d/polygon_tessellator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace shunya::cad3d {

namespace {

bool solve_3x3(const std::array<std::array<double, 3>, 3>& a, const Vec3d& b, Vec3d& x) noexcept {
    // Gaussian elimination with partial pivoting
    double m[3][4] = {
        {a[0][0], a[0][1], a[0][2], b.x},
        {a[1][0], a[1][1], a[1][2], b.y},
        {a[2][0], a[2][1], a[2][2], b.z}
    };
    for (int col = 0; col < 3; ++col) {
        int piv = col;
        for (int row = col + 1; row < 3; ++row) {
            if (std::abs(m[row][col]) > std::abs(m[piv][col])) piv = row;
        }
        if (std::abs(m[piv][col]) <= 1e-14) return false;
        if (piv != col) {
            for (int c = 0; c < 4; ++c) std::swap(m[col][c], m[piv][c]);
        }
        for (int row = col + 1; row < 3; ++row) {
            const double factor = m[row][col] / m[col][col];
            for (int c = col; c < 4; ++c) m[row][c] -= factor * m[col][c];
        }
    }
    x.z = m[2][3] / m[2][2];
    x.y = (m[1][3] - m[1][2] * x.z) / m[1][1];
    x.x = (m[0][3] - m[0][1] * x.y - m[0][2] * x.z) / m[0][0];
    return true;
}

void add_face_triangles(SolidMesh3D& out, const PolyhedralBRep& brep, const BrepFace& face, const std::vector<uint32_t>& vert_ids, bool invert) {
    const size_t ln = face.outer_loop.size();
    if (ln < 3) return;
    cad2d::Path2D local_2d;
    local_2d.reserve(ln);
    for (const uint32_t vi : face.outer_loop) {
        const Vec3d rel = brep.vertices[vi] - face.origin;
        local_2d.push_back({rel.dot(face.axis_u), rel.dot(face.axis_v)});
    }
    const TessellatedPolygon2D tess = PolygonTessellator::triangulate(local_2d);
    std::vector<uint32_t> tess_to_vid(tess.vertices.size(), 0);
    for (size_t ti = 0; ti < tess.vertices.size(); ++ti) {
        size_t best_k = 0;
        double best_d2 = 1e300;
        for (size_t k = 0; k < local_2d.size(); ++k) {
            const double d2 = (tess.vertices[ti] - local_2d[k]).length_squared();
            if (d2 < best_d2) {
                best_d2 = d2;
                best_k = k;
            }
        }
        tess_to_vid[ti] = vert_ids[face.outer_loop[best_k]];
    }
    for (const auto& t : tess.triangles) {
        if (!invert) {
            out.add_triangle(tess_to_vid[t[0]], tess_to_vid[t[1]], tess_to_vid[t[2]]);
        } else {
            out.add_triangle(tess_to_vid[t[0]], tess_to_vid[t[2]], tess_to_vid[t[1]]);
        }
    }
}

} // namespace

std::vector<Point3D> ShellEngine::compute_offset_vertices(
    const PolyhedralBRep& brep,
    double outward_offset,
    const std::vector<uint32_t>& zero_offset_faces
) {
    const size_t n_verts = brep.vertices.size();
    const std::set<uint32_t> zero_set(zero_offset_faces.begin(), zero_offset_faces.end());
    struct PlaneReq {
        Vec3d normal;
        double rhs;
    };
    std::vector<std::vector<PlaneReq>> vert_reqs(n_verts);
    for (const auto& face : brep.faces) {
        const double d = zero_set.contains(face.id) ? 0.0 : outward_offset;
        for (const uint32_t vi : face.outer_loop) {
            vert_reqs[vi].push_back({face.normal, d});
        }
    }

    std::vector<Point3D> shifted(n_verts);
    for (size_t vi = 0; vi < n_verts; ++vi) {
        const auto& reqs = vert_reqs[vi];
        if (reqs.empty()) {
            shifted[vi] = brep.vertices[vi];
            continue;
        }
        // Normal equations (N^T N + reg*I) * dx = sum(d_i * n_i)
        std::array<std::array<double, 3>, 3> ata{{{1e-9, 0, 0}, {0, 1e-9, 0}, {0, 0, 1e-9}}};
        Vec3d rhs{0.0, 0.0, 0.0};
        for (const auto& r : reqs) {
            ata[0][0] += r.normal.x * r.normal.x; ata[0][1] += r.normal.x * r.normal.y; ata[0][2] += r.normal.x * r.normal.z;
            ata[1][0] += r.normal.y * r.normal.x; ata[1][1] += r.normal.y * r.normal.y; ata[1][2] += r.normal.y * r.normal.z;
            ata[2][0] += r.normal.z * r.normal.x; ata[2][1] += r.normal.z * r.normal.y; ata[2][2] += r.normal.z * r.normal.z;
            rhs += r.normal * r.rhs;
        }
        Vec3d dx{0.0, 0.0, 0.0};
        if (solve_3x3(ata, rhs, dx)) {
            shifted[vi] = brep.vertices[vi] + dx;
        } else {
            Vec3d avg{0.0, 0.0, 0.0};
            for (const auto& r : reqs) avg += r.normal * r.rhs;
            shifted[vi] = brep.vertices[vi] + avg;
        }
    }
    return shifted;
}

SolidMesh3D ShellEngine::offset_solid(
    const SolidMesh3D& mesh,
    double offset_distance
) {
    const PolyhedralBRep brep = PolyhedralBRep::from_mesh(mesh);
    if (brep.faces.empty()) return mesh;
    const auto shifted = compute_offset_vertices(brep, offset_distance);

    SolidMesh3D out;
    std::vector<uint32_t> vids(shifted.size());
    for (size_t i = 0; i < shifted.size(); ++i) {
        vids[i] = out.add_vertex(shifted[i]);
    }
    for (const auto& face : brep.faces) {
        add_face_triangles(out, brep, face, vids, false);
    }
    out.ensure_outward_winding();
    return out;
}

SolidMesh3D ShellEngine::shell(
    const SolidMesh3D& mesh,
    double wall_thickness,
    const std::vector<uint32_t>& open_face_indices
) {
    if (wall_thickness <= 0.0) return mesh;
    const PolyhedralBRep brep = PolyhedralBRep::from_mesh(mesh);
    if (brep.faces.empty()) return mesh;

    std::set<uint32_t> open_set(open_face_indices.begin(), open_face_indices.end());
    const auto inner_pts = compute_offset_vertices(brep, -wall_thickness, open_face_indices);

    SolidMesh3D out;
    std::vector<uint32_t> outer_vids(brep.vertices.size());
    std::vector<uint32_t> inner_vids(inner_pts.size());
    for (size_t i = 0; i < brep.vertices.size(); ++i) {
        outer_vids[i] = out.add_vertex(brep.vertices[i]);
    }
    for (size_t i = 0; i < inner_pts.size(); ++i) {
        inner_vids[i] = out.add_vertex(inner_pts[i]);
    }

    // Emit outer & reversed inner faces ONLY for non-open faces
    for (const auto& face : brep.faces) {
        if (open_set.contains(face.id)) continue;
        add_face_triangles(out, brep, face, outer_vids, false);
        add_face_triangles(out, brep, face, inner_vids, true);
    }

    // Connect outer boundary edges adjacent to an open face to inner boundary edges with rim walls
    for (const auto& face : brep.faces) {
        if (!open_set.contains(face.id)) continue;
        const size_t ln = face.outer_loop.size();
        for (size_t i = 0; i < ln; ++i) {
            const uint32_t a = face.outer_loop[i];
            const uint32_t b = face.outer_loop[(i + 1) % ln];
            // Open face traversed a -> b CCW, so kept faces along this rim traverse b -> a.
            // Rim cap closing outer->inner along that open border has edges a -> b on outer side:
            const uint32_t oa = outer_vids[a];
            const uint32_t ob = outer_vids[b];
            const uint32_t ia = inner_vids[a];
            const uint32_t ib = inner_vids[b];
            out.add_triangle(oa, ob, ib);
            out.add_triangle(oa, ib, ia);
        }
    }

    out.weld_vertices(1e-8);
    out.ensure_outward_winding();
    return out;
}

SolidMesh3D ShellEngine::shell_open_by_normal(
    const SolidMesh3D& mesh,
    double wall_thickness,
    const Vec3d& target_normal,
    double min_normal_dot
) {
    const PolyhedralBRep brep = PolyhedralBRep::from_mesh(mesh);
    const Vec3d unit_n = target_normal.normalized();
    std::vector<uint32_t> open_faces;
    for (const auto& face : brep.faces) {
        if (face.normal.dot(unit_n) >= min_normal_dot) {
            open_faces.push_back(face.id);
        }
    }
    return shell(mesh, wall_thickness, open_faces);
}

} // namespace shunya::cad3d
