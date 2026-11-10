#include "shunya/cad3d/solid_fillet.hpp"

#include "shunya/cad3d/polygon_tessellator.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>

namespace shunya::cad3d {

namespace {

using VFKey = std::pair<uint32_t, uint32_t>;
using UndirEdge = std::pair<uint32_t, uint32_t>;

double eval_edge_radius(const FilletParams& params, uint32_t eid, double t) {
    if (params.variable_radius_fn) {
        return std::max(0.0, params.variable_radius_fn(eid, t));
    }
    if (params.radius_end >= 0.0) {
        return std::max(0.0, params.radius * (1.0 - t) + params.radius_end * t);
    }
    return std::max(0.0, params.radius);
}

Point3D compute_face_corner_trim(
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

    const Vec3d n_in = face.normal.cross(dir_in).normalized();
    const Vec3d n_out = face.normal.cross(dir_out).normalized();

    const double det = n_in.dot(n_out.cross(face.normal));
    if (std::abs(det) <= 1e-10) {
        return P + n_in * std::max(d_in, d_out);
    }

    const Vec3d col0 = n_out.cross(face.normal);
    const Vec3d col1 = face.normal.cross(n_in);
    return P + (col0 * d_in + col1 * d_out) * (1.0 / det);
}

Vec3d slerp_unit(const Vec3d& a, const Vec3d& b, double u) {
    const Vec3d ua = a.normalized();
    const Vec3d ub = b.normalized();
    const double d = std::clamp(ua.dot(ub), -1.0, 1.0);
    if (d > 0.9999) {
        return (ua * (1.0 - u) + ub * u).normalized();
    }
    const double omega = std::acos(d);
    const double s = std::sin(omega);
    if (std::abs(s) <= 1e-12) return ua;
    const double w0 = std::sin((1.0 - u) * omega) / s;
    const double w1 = std::sin(u * omega) / s;
    return (ua * w0 + ub * w1).normalized();
}

Point3D eval_circular_blend(
    const Point3D& p_left,
    const Point3D& p_right,
    const Vec3d& n_left,
    const Vec3d& n_right,
    double dihedral_rad,
    double u
) {
    if (u <= 0.0) return p_left;
    if (u >= 1.0) return p_right;
    const double tan_half = std::tan(std::clamp(dihedral_rad * 0.5, 0.05, 1.52));
    const Vec3d bisector = (n_left + n_right).normalized();
    const double half_chord = (p_right - p_left).length() * 0.5;
    if (half_chord <= 1e-12 || bisector.length_squared() < 0.5) {
        return p_left + (p_right - p_left) * u;
    }
    const Point3D center = (p_left + p_right) * 0.5 - bisector * (half_chord / tan_half);
    const Vec3d r0 = p_left - center;
    const Vec3d r1 = p_right - center;
    const double len0 = r0.length();
    const double len1 = r1.length();
    const Vec3d dir = slerp_unit(r0, r1, u);
    return center + dir * (len0 * (1.0 - u) + len1 * u);
}

} // namespace

SolidMesh3D SolidFilletEngine::fillet(
    const SolidMesh3D& mesh,
    const FilletParams& params
) {
    const PolyhedralBRep brep = PolyhedralBRep::from_mesh(mesh);
    if (brep.faces.empty() || brep.edges.empty()) return mesh;

    const size_t segs = std::max<size_t>(2, params.arc_segments);
    std::vector<bool> edge_selected(brep.edges.size(), false);
    if (params.edge_indices.empty()) {
        for (size_t i = 0; i < brep.edges.size(); ++i) {
            if (brep.edges[i].dihedral_angle_rad >= params.min_angle_rad) {
                edge_selected[i] = true;
            }
        }
    } else {
        for (const uint32_t eid : params.edge_indices) {
            if (eid < edge_selected.size()) edge_selected[eid] = true;
        }
    }

    std::map<UndirEdge, uint32_t> edge_lookup;
    for (const auto& e : brep.edges) {
        edge_lookup[(e.v0 < e.v1) ? UndirEdge{e.v0, e.v1} : UndirEdge{e.v1, e.v0}] = e.id;
    }

    auto get_setback_at_vertex = [&](uint32_t va, uint32_t vb) -> double {
        const UndirEdge ek = (va < vb) ? UndirEdge{va, vb} : UndirEdge{vb, va};
        auto it = edge_lookup.find(ek);
        if (it == edge_lookup.end()) return 0.0;
        const uint32_t eid = it->second;
        if (!edge_selected[eid]) return 0.0;
        const auto& edge = brep.edges[eid];
        const double t_at_va = (va == edge.v0) ? 0.0 : 1.0;
        const double r = eval_edge_radius(params, eid, t_at_va);
        return r * std::tan(std::clamp(edge.dihedral_angle_rad * 0.5, 0.05, 1.52));
    };

    SolidMesh3D out;
    std::map<VFKey, uint32_t> corner_vid;

    // 1. Create trimmed planar faces
    for (const auto& face : brep.faces) {
        const size_t ln = face.outer_loop.size();
        if (ln < 3) continue;

        cad2d::Path2D local_2d;
        local_2d.reserve(ln);
        std::vector<uint32_t> face_vids;
        face_vids.reserve(ln);

        for (size_t i = 0; i < ln; ++i) {
            const uint32_t vp = face.outer_loop[(i + ln - 1) % ln];
            const uint32_t vc = face.outer_loop[i];
            const uint32_t vn = face.outer_loop[(i + 1) % ln];
            // Tangent setback for incoming/outgoing edge evaluated at vc
            const double d_in = get_setback_at_vertex(vc, vp);
            const double d_out = get_setback_at_vertex(vc, vn);
            const Point3D trimmed = compute_face_corner_trim(brep, face, vp, vc, vn, d_in, d_out);
            const uint32_t vid = out.add_vertex(trimmed);
            corner_vid[{vc, face.id}] = vid;
            face_vids.push_back(vid);

            const Vec3d rel = trimmed - face.origin;
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
            tess_to_vid[ti] = face_vids[best_k];
        }
        for (const auto& t : tess.triangles) {
            out.add_triangle(tess_to_vid[t[0]], tess_to_vid[t[1]], tess_to_vid[t[2]]);
        }
    }

    // Track corner boundary arcs at each vertex V -> list of directed half-edge chains (from -> to)
    std::vector<std::unordered_map<uint32_t, uint32_t>> corner_next(brep.vertices.size());

    // 2. Build rolling-ball cylindrical/tapered circular blend strips for every edge
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

        if (!edge_selected[e.id]) {
            out.add_triangle(bl, al, ar);
            out.add_triangle(bl, ar, br);
            if (al != ar) corner_next[e.v0][ar] = al;
            if (bl != br) corner_next[e.v1][bl] = br;
            continue;
        }

        const Vec3d& nl = brep.faces[e.face_left].normal;
        const Vec3d& nr = brep.faces[e.face_right].normal;
        const Point3D& p_al = out.vertices[al];
        const Point3D& p_ar = out.vertices[ar];
        const Point3D& p_bl = out.vertices[bl];
        const Point3D& p_br = out.vertices[br];

        std::vector<uint32_t> chain_a(segs + 1);
        std::vector<uint32_t> chain_b(segs + 1);
        chain_a[0] = al;
        chain_a[segs] = ar;
        chain_b[0] = bl;
        chain_b[segs] = br;

        for (size_t s = 1; s < segs; ++s) {
            const double u = static_cast<double>(s) / static_cast<double>(segs);
            chain_a[s] = out.add_vertex(eval_circular_blend(p_al, p_ar, nl, nr, e.dihedral_angle_rad, u));
            chain_b[s] = out.add_vertex(eval_circular_blend(p_bl, p_br, nl, nr, e.dihedral_angle_rad, u));
        }

        for (size_t s = 0; s < segs; ++s) {
            out.add_triangle(chain_b[s], chain_a[s], chain_a[s + 1]);
            out.add_triangle(chain_b[s], chain_a[s + 1], chain_b[s + 1]);
            // Corner hole boundary traversal:
            // At v0: strip edge goes chain_a[s] -> chain_a[s+1], so corner hole goes chain_a[s+1] -> chain_a[s]
            corner_next[e.v0][chain_a[s + 1]] = chain_a[s];
            // At v1: strip edge goes chain_b[s+1] -> chain_b[s], so corner hole goes chain_b[s] -> chain_b[s+1]
            corner_next[e.v1][chain_b[s]] = chain_b[s + 1];
        }
    }

    // 3. Build spherical corner blend patches over each corner vertex boundary ring
    for (size_t v_id = 0; v_id < brep.vertices.size(); ++v_id) {
        const auto& next_map = corner_next[v_id];
        if (next_map.size() < 3) continue;

        const uint32_t start_v = next_map.begin()->first;
        std::vector<uint32_t> ring;
        uint32_t cur = start_v;
        size_t guard = next_map.size() + 4;
        do {
            ring.push_back(cur);
            auto it = next_map.find(cur);
            if (it == next_map.end()) break;
            cur = it->second;
        } while (cur != start_v && --guard > 0);

        if (ring.size() < 3) continue;

        // Place central corner hub radially slightly inflated along average outward normal for smooth spherical blend
        Point3D avg_pt{0.0, 0.0, 0.0};
        Vec3d avg_dir{0.0, 0.0, 0.0};
        for (const uint32_t rv : ring) {
            avg_pt += out.vertices[rv];
            avg_dir += (out.vertices[rv] - brep.vertices[v_id]);
        }
        avg_pt = avg_pt * (1.0 / static_cast<double>(ring.size()));
        const Vec3d push_dir = (brep.vertices[v_id] - avg_pt);
        const Point3D hub = avg_pt + push_dir * 0.35;
        const uint32_t hub_id = out.add_vertex(hub);

        for (size_t i = 0; i < ring.size(); ++i) {
            out.add_triangle(hub_id, ring[i], ring[(i + 1) % ring.size()]);
        }
    }

    out.weld_vertices(1e-8);
    out.ensure_outward_winding();
    return out;
}

} // namespace shunya::cad3d
