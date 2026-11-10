#include "shunya/cad3d/subdivision.hpp"

#include "shunya/cad3d/mesh_circulator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace shunya::cad3d {

namespace {

using UndirEdge = std::pair<uint32_t, uint32_t>;

bool is_edge_crease(
    const HalfEdgeMesh& mesh,
    uint32_t he_idx,
    double cos_crease_thresh
) noexcept {
    if (he_idx >= mesh.half_edges.size()) return false;
    const uint32_t tw = mesh.half_edges[he_idx].twin;
    if (tw == HE_INVALID) return true; // boundary acts as sharp edge
    const uint32_t f0 = mesh.half_edges[he_idx].face;
    const uint32_t f1 = mesh.half_edges[tw].face;
    if (f0 >= mesh.faces.size() || f1 >= mesh.faces.size()) return false;
    return mesh.faces[f0].normal.dot(mesh.faces[f1].normal) <= cos_crease_thresh;
}

HalfEdgeMesh loop_step(const HalfEdgeMesh& input, double crease_angle_rad) {
    // Ensure input faces are triangles
    const SolidMesh3D tri_solid = input.to_solid_mesh();
    HalfEdgeMesh cur;
    cur.build_from_solid(tri_solid);
    if (cur.faces.empty()) return cur;

    const double cos_thresh = (crease_angle_rad >= core::base::math::PI)
        ? -2.0
        : std::cos(crease_angle_rad);

    const size_t n_verts = cur.vertices.size();
    std::vector<Point3D> new_verts(n_verts);

    // 1. Update original control vertices (interior vs boundary/crease)
    for (uint32_t v = 0; v < n_verts; ++v) {
        const auto out_hes = MeshCirculator::outgoing_half_edges(cur, v);
        std::vector<uint32_t> crease_nbrs;
        std::vector<uint32_t> all_nbrs = MeshCirculator::vertex_neighbors(cur, v);

        for (const uint32_t he : out_hes) {
            if (is_edge_crease(cur, he, cos_thresh)) {
                crease_nbrs.push_back(cur.target_vertex(he));
            }
            const uint32_t pr = cur.half_edges[he].prev;
            if (pr < cur.half_edges.size() && cur.half_edges[pr].twin == HE_INVALID) {
                crease_nbrs.push_back(cur.half_edges[pr].vertex);
            }
        }
        std::sort(crease_nbrs.begin(), crease_nbrs.end());
        crease_nbrs.erase(std::unique(crease_nbrs.begin(), crease_nbrs.end()), crease_nbrs.end());

        const Point3D& pv = cur.vertices[v].position;
        if (crease_nbrs.size() == 2) {
            new_verts[v] = pv * 0.75 +
                           (cur.vertices[crease_nbrs[0]].position + cur.vertices[crease_nbrs[1]].position) * 0.125;
        } else if (crease_nbrs.size() >= 3) {
            new_verts[v] = pv; // Corner pinned along >=3 intersecting creases
        } else if (all_nbrs.size() >= 3) {
            const double n = static_cast<double>(all_nbrs.size());
            const double beta = (all_nbrs.size() == 3) ? (3.0 / 16.0) : (3.0 / (8.0 * n));
            Point3D sum_nbr{0.0, 0.0, 0.0};
            for (const uint32_t nb : all_nbrs) {
                sum_nbr += cur.vertices[nb].position;
            }
            new_verts[v] = pv * (1.0 - n * beta) + sum_nbr * beta;
        } else {
            new_verts[v] = pv;
        }
    }

    // 2. Create odd edge midpoint vertices per unique undirected edge
    std::map<UndirEdge, uint32_t> edge_mid;
    for (uint32_t he_idx = 0; he_idx < cur.half_edges.size(); ++he_idx) {
        const uint32_t a = cur.half_edges[he_idx].vertex;
        const uint32_t b = cur.target_vertex(he_idx);
        const UndirEdge ek = (a < b) ? UndirEdge{a, b} : UndirEdge{b, a};
        if (edge_mid.contains(ek)) continue;

        const uint32_t tw = cur.half_edges[he_idx].twin;
        Point3D pos;
        if (tw == HE_INVALID || is_edge_crease(cur, he_idx, cos_thresh)) {
            pos = (cur.vertices[a].position + cur.vertices[b].position) * 0.5;
        } else {
            const uint32_t opp0 = cur.target_vertex(cur.half_edges[he_idx].next);
            const uint32_t opp1 = cur.target_vertex(cur.half_edges[tw].next);
            pos = (cur.vertices[a].position + cur.vertices[b].position) * 0.375 +
                  (cur.vertices[opp0].position + cur.vertices[opp1].position) * 0.125;
        }
        const uint32_t mid_id = static_cast<uint32_t>(new_verts.size());
        new_verts.push_back(pos);
        edge_mid[ek] = mid_id;
    }

    // 3. Emit 4 sub-triangles per input triangle
    std::vector<std::vector<uint32_t>> new_faces;
    new_faces.reserve(cur.faces.size() * 4);
    auto get_mid = [&](uint32_t u, uint32_t v) {
        return edge_mid[(u < v) ? UndirEdge{u, v} : UndirEdge{v, u}];
    };

    for (uint32_t fid = 0; fid < cur.faces.size(); ++fid) {
        const auto fv = MeshCirculator::face_vertices(cur, fid);
        if (fv.size() != 3) continue;
        const uint32_t v0 = fv[0], v1 = fv[1], v2 = fv[2];
        const uint32_t m01 = get_mid(v0, v1);
        const uint32_t m12 = get_mid(v1, v2);
        const uint32_t m20 = get_mid(v2, v0);
        new_faces.push_back({v0, m01, m20});
        new_faces.push_back({v1, m12, m01});
        new_faces.push_back({v2, m20, m12});
        new_faces.push_back({m01, m12, m20});
    }

    HalfEdgeMesh out;
    out.build_from_polygons(new_verts, new_faces);
    return out;
}

HalfEdgeMesh catmull_clark_step(const HalfEdgeMesh& input, double crease_angle_rad) {
    if (input.faces.empty() || input.vertices.empty()) return input;
    const double cos_thresh = (crease_angle_rad >= core::base::math::PI)
        ? -2.0
        : std::cos(crease_angle_rad);

    // 1. Face points = average of each face's vertices
    std::vector<Point3D> new_verts;
    const size_t n_faces = input.faces.size();
    std::vector<uint32_t> face_pt_idx(n_faces);
    std::vector<std::vector<uint32_t>> cached_fv(n_faces);

    for (uint32_t fid = 0; fid < n_faces; ++fid) {
        cached_fv[fid] = MeshCirculator::face_vertices(input, fid);
        Point3D avg{0.0, 0.0, 0.0};
        for (const uint32_t vi : cached_fv[fid]) avg += input.vertices[vi].position;
        if (!cached_fv[fid].empty()) {
            avg = avg * (1.0 / static_cast<double>(cached_fv[fid].size()));
        }
        face_pt_idx[fid] = static_cast<uint32_t>(new_verts.size());
        new_verts.push_back(avg);
    }

    // 2. Edge points = (v0 + v1 + F0 + F1) / 4 (or midpoint on boundary/crease)
    std::map<UndirEdge, uint32_t> edge_pt_idx;
    for (uint32_t he_idx = 0; he_idx < input.half_edges.size(); ++he_idx) {
        const uint32_t a = input.half_edges[he_idx].vertex;
        const uint32_t b = input.target_vertex(he_idx);
        const UndirEdge ek = (a < b) ? UndirEdge{a, b} : UndirEdge{b, a};
        if (edge_pt_idx.contains(ek)) continue;

        const uint32_t tw = input.half_edges[he_idx].twin;
        Point3D ep;
        if (tw == HE_INVALID || is_edge_crease(input, he_idx, cos_thresh)) {
            ep = (input.vertices[a].position + input.vertices[b].position) * 0.5;
        } else {
            const uint32_t f0 = input.half_edges[he_idx].face;
            const uint32_t f1 = input.half_edges[tw].face;
            ep = (input.vertices[a].position + input.vertices[b].position +
                  new_verts[face_pt_idx[f0]] + new_verts[face_pt_idx[f1]]) * 0.25;
        }
        edge_pt_idx[ek] = static_cast<uint32_t>(new_verts.size());
        new_verts.push_back(ep);
    }

    // 3. Vertex points: (F_avg + 2*R_avg + (n - 3)*V) / n (with boundary/crease rule)
    std::vector<uint32_t> vert_pt_idx(input.vertices.size());
    for (uint32_t v = 0; v < input.vertices.size(); ++v) {
        const auto out_hes = MeshCirculator::outgoing_half_edges(input, v);
        const auto inc_faces = MeshCirculator::incident_faces(input, v);
        const auto nbrs = MeshCirculator::vertex_neighbors(input, v);

        std::vector<uint32_t> crease_nbrs;
        for (const uint32_t he : out_hes) {
            if (is_edge_crease(input, he, cos_thresh)) {
                crease_nbrs.push_back(input.target_vertex(he));
            }
            const uint32_t pr = input.half_edges[he].prev;
            if (pr < input.half_edges.size() && input.half_edges[pr].twin == HE_INVALID) {
                crease_nbrs.push_back(input.half_edges[pr].vertex);
            }
        }
        std::sort(crease_nbrs.begin(), crease_nbrs.end());
        crease_nbrs.erase(std::unique(crease_nbrs.begin(), crease_nbrs.end()), crease_nbrs.end());

        const Point3D& pv = input.vertices[v].position;
        Point3D vp;
        if (crease_nbrs.size() == 2) {
            vp = pv * 0.75 +
                 (input.vertices[crease_nbrs[0]].position + input.vertices[crease_nbrs[1]].position) * 0.125;
        } else if (crease_nbrs.size() >= 3) {
            vp = pv;
        } else if (!inc_faces.empty() && nbrs.size() == inc_faces.size() && nbrs.size() >= 3) {
            const double n = static_cast<double>(nbrs.size());
            Point3D f_avg{0.0, 0.0, 0.0};
            for (const uint32_t fid : inc_faces) f_avg += new_verts[face_pt_idx[fid]];
            f_avg = f_avg * (1.0 / n);

            Point3D r_avg{0.0, 0.0, 0.0};
            for (const uint32_t nb : nbrs) {
                r_avg += (pv + input.vertices[nb].position) * 0.5;
            }
            r_avg = r_avg * (1.0 / n);

            vp = (f_avg + r_avg * 2.0 + pv * (n - 3.0)) * (1.0 / n);
        } else {
            vp = pv;
        }
        vert_pt_idx[v] = static_cast<uint32_t>(new_verts.size());
        new_verts.push_back(vp);
    }

    // 4. Emit one quad per corner of every input polygon face
    std::vector<std::vector<uint32_t>> new_faces;
    for (uint32_t fid = 0; fid < n_faces; ++fid) {
        const auto& fv = cached_fv[fid];
        const size_t deg = fv.size();
        const uint32_t f_pt = face_pt_idx[fid];
        for (size_t i = 0; i < deg; ++i) {
            const uint32_t v_prev = fv[(i + deg - 1) % deg];
            const uint32_t v_curr = fv[i];
            const uint32_t v_next = fv[(i + 1) % deg];
            const uint32_t e_in = edge_pt_idx[(v_prev < v_curr) ? UndirEdge{v_prev, v_curr} : UndirEdge{v_curr, v_prev}];
            const uint32_t e_out = edge_pt_idx[(v_curr < v_next) ? UndirEdge{v_curr, v_next} : UndirEdge{v_next, v_curr}];
            new_faces.push_back({vert_pt_idx[v_curr], e_out, f_pt, e_in});
        }
    }

    HalfEdgeMesh out;
    out.build_from_polygons(new_verts, new_faces);
    return out;
}

} // namespace

HalfEdgeMesh SubdivisionEngine::subdivide_loop(
    const HalfEdgeMesh& mesh,
    size_t levels,
    double crease_angle_rad
) {
    HalfEdgeMesh cur = mesh;
    for (size_t l = 0; l < levels; ++l) {
        cur = loop_step(cur, crease_angle_rad);
    }
    return cur;
}

SolidMesh3D SubdivisionEngine::subdivide_loop(
    const SolidMesh3D& solid,
    size_t levels,
    double crease_angle_rad
) {
    HalfEdgeMesh he;
    he.build_from_solid(solid);
    he = subdivide_loop(he, levels, crease_angle_rad);
    return he.to_solid_mesh();
}

HalfEdgeMesh SubdivisionEngine::subdivide_catmull_clark(
    const HalfEdgeMesh& mesh,
    size_t levels,
    double crease_angle_rad
) {
    HalfEdgeMesh cur = mesh;
    for (size_t l = 0; l < levels; ++l) {
        cur = catmull_clark_step(cur, crease_angle_rad);
    }
    return cur;
}

} // namespace shunya::cad3d
