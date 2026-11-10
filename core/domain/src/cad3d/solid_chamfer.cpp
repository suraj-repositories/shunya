#include "shunya/cad3d/solid_chamfer.hpp"

#include "shunya/cad3d/polygon_tessellator.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>

namespace shunya::cad3d {

namespace {

using VFKey = std::pair<uint32_t, uint32_t>; // (vertex_id, face_id)
using UndirEdge = std::pair<uint32_t, uint32_t>;

Point3D compute_face_corner_bevel(
    const PolyhedralBRep& brep,
    const BrepFace& face,
    uint32_t v_prev,
    uint32_t v_curr,
    uint32_t v_next,
    double d_in,
    double d_out
) {
    const Point3D& P = brep.vertices[v_curr];
    if (d_in <= 1e-12 && d_out <= 1e-12) return P;

    const Vec3d dir_in = (brep.vertices[v_curr] - brep.vertices[v_prev]).normalized();
    const Vec3d dir_out = (brep.vertices[v_next] - brep.vertices[v_curr]).normalized();

    // Face-inward unit normals perpendicular to dir_in and dir_out within face plane
    const Vec3d n_in = face.normal.cross(dir_in).normalized();
    const Vec3d n_out = face.normal.cross(dir_out).normalized();

    // Intersect lines inside face plane:
    // n_in . delta = d_in,  n_out . delta = d_out,  face.normal . delta = 0
    const Vec3d row0 = n_in;
    const Vec3d row1 = n_out;
    const Vec3d row2 = face.normal;
    const double det = row0.dot(row1.cross(row2));
    if (std::abs(det) <= 1e-10) {
        return P + n_in * std::max(d_in, d_out);
    }

    const Vec3d col0{row1.cross(row2)};
    const Vec3d col1{row2.cross(row0)};
    const Vec3d delta = (col0 * d_in + col1 * d_out) * (1.0 / det);
    return P + delta;
}

} // namespace

SolidMesh3D SolidChamferEngine::chamfer(
    const SolidMesh3D& mesh,
    const ChamferParams& params
) {
    const PolyhedralBRep brep = PolyhedralBRep::from_mesh(mesh);
    if (brep.faces.empty() || brep.edges.empty()) return mesh;

    // Build edge distance lookup
    std::vector<double> edge_dist(brep.edges.size(), 0.0);
    if (params.edge_indices.empty()) {
        for (size_t i = 0; i < brep.edges.size(); ++i) {
            if (brep.edges[i].dihedral_angle_rad >= params.min_angle_rad) {
                edge_dist[i] = params.distance;
            }
        }
    } else {
        for (const uint32_t eid : params.edge_indices) {
            if (eid < edge_dist.size()) {
                edge_dist[eid] = params.distance;
            }
        }
    }

    std::map<UndirEdge, uint32_t> edge_lookup;
    for (const auto& e : brep.edges) {
        edge_lookup[(e.v0 < e.v1) ? UndirEdge{e.v0, e.v1} : UndirEdge{e.v1, e.v0}] = e.id;
    }

    auto get_edge_dist = [&](uint32_t a, uint32_t b) -> double {
        const UndirEdge ek = (a < b) ? UndirEdge{a, b} : UndirEdge{b, a};
        auto it = edge_lookup.find(ek);
        return (it != edge_lookup.end()) ? edge_dist[it->second] : 0.0;
    };

    SolidMesh3D out;
    std::map<VFKey, uint32_t> corner_vid;

    // 1. Generate trimmed inset vertices and triangulated face interiors
    for (const auto& face : brep.faces) {
        const size_t ln = face.outer_loop.size();
        if (ln < 3) continue;

        cad2d::Path2D local_2d;
        local_2d.reserve(ln);
        std::vector<uint32_t> face_corner_ids;
        face_corner_ids.reserve(ln);

        for (size_t i = 0; i < ln; ++i) {
            const uint32_t vp = face.outer_loop[(i + ln - 1) % ln];
            const uint32_t vc = face.outer_loop[i];
            const uint32_t vn = face.outer_loop[(i + 1) % ln];
            const double d_in = get_edge_dist(vp, vc);
            const double d_out = get_edge_dist(vc, vn);
            const Point3D bevel_pt = compute_face_corner_bevel(brep, face, vp, vc, vn, d_in, d_out);

            const uint32_t vid = out.add_vertex(bevel_pt);
            corner_vid[{vc, face.id}] = vid;
            face_corner_ids.push_back(vid);

            const Vec3d rel = bevel_pt - face.origin;
            local_2d.push_back({rel.dot(face.axis_u), rel.dot(face.axis_v)});
        }

        const TessellatedPolygon2D tess = PolygonTessellator::triangulate(local_2d);
        // Map tess.vertices (normalized to CCW) back to face_corner_ids
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
            tess_to_vid[ti] = face_corner_ids[best_k];
        }
        for (const auto& t : tess.triangles) {
            out.add_triangle(tess_to_vid[t[0]], tess_to_vid[t[1]], tess_to_vid[t[2]]);
        }
    }

    // 2. Stitch chamfer edge quads for every B-Rep edge
    for (const auto& e : brep.edges) {
        auto it_al = corner_vid.find({e.v0, e.face_left});
        auto it_bl = corner_vid.find({e.v1, e.face_left});
        auto it_ar = corner_vid.find({e.v0, e.face_right});
        auto it_br = corner_vid.find({e.v1, e.face_right});
        if (it_al == corner_vid.end() || it_bl == corner_vid.end() ||
            it_ar == corner_vid.end() || it_br == corner_vid.end()) {
            continue;
        }
        const uint32_t al = it_al->second;
        const uint32_t bl = it_bl->second;
        const uint32_t ar = it_ar->second;
        const uint32_t br = it_br->second;
        out.add_triangle(bl, al, ar);
        out.add_triangle(bl, ar, br);
    }

    // 3. Fill every corner vertex patch by tracing directed open edges around vertex V
    // Map each vertex V -> list of directed boundary half-edges (corner_r -> corner_l) at V
    for (size_t v_id = 0; v_id < brep.vertices.size(); ++v_id) {
        std::unordered_map<uint32_t, uint32_t> next_corner;
        uint32_t first_corner = 0;
        bool has_loop = false;
        for (const auto& e : brep.edges) {
            if (e.v0 == v_id) {
                // Edge strip had edge al -> ar, so corner hole needs ar -> al
                const uint32_t al = corner_vid[{static_cast<uint32_t>(v_id), e.face_left}];
                const uint32_t ar = corner_vid[{static_cast<uint32_t>(v_id), e.face_right}];
                if (al != ar) {
                    next_corner[ar] = al;
                    if (!has_loop) { first_corner = ar; has_loop = true; }
                }
            } else if (e.v1 == v_id) {
                // Edge strip had edge br -> bl, so corner hole needs bl -> br
                const uint32_t bl = corner_vid[{static_cast<uint32_t>(v_id), e.face_left}];
                const uint32_t br = corner_vid[{static_cast<uint32_t>(v_id), e.face_right}];
                if (bl != br) {
                    next_corner[bl] = br;
                    if (!has_loop) { first_corner = bl; has_loop = true; }
                }
            }
        }
        if (!has_loop || next_corner.size() < 3) continue;

        std::vector<uint32_t> ring;
        uint32_t cur = first_corner;
        size_t guard = next_corner.size() + 2;
        do {
            ring.push_back(cur);
            auto it = next_corner.find(cur);
            if (it == next_corner.end()) break;
            cur = it->second;
        } while (cur != first_corner && --guard > 0);

        if (ring.size() >= 3) {
            for (size_t i = 2; i < ring.size(); ++i) {
                out.add_triangle(ring[0], ring[i - 1], ring[i]);
            }
        }
    }

    out.weld_vertices(1e-8);
    out.ensure_outward_winding();
    return out;
}

} // namespace shunya::cad3d
