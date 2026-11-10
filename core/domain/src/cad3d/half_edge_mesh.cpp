#include "shunya/cad3d/half_edge_mesh.hpp"
#include "shunya/cad3d/mesh_circulator.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace shunya::cad3d {

void HalfEdgeMesh::clear() noexcept {
    vertices.clear();
    half_edges.clear();
    faces.clear();
    non_manifold_detected_ = false;
}

bool HalfEdgeMesh::build_from_polygons(
    const std::vector<Point3D>& verts,
    const std::vector<std::vector<uint32_t>>& poly_faces
) {
    clear();
    vertices.resize(verts.size());
    for (size_t i = 0; i < verts.size(); ++i) {
        vertices[i].position = verts[i];
    }

    using DirEdge = std::pair<uint32_t, uint32_t>;
    using UndirEdge = std::pair<uint32_t, uint32_t>;
    std::map<DirEdge, uint32_t> dir_map;
    std::map<UndirEdge, int> undir_count;

    for (const auto& poly : poly_faces) {
        if (poly.size() < 3) continue;
        const uint32_t fid = static_cast<uint32_t>(faces.size());
        HEFace face;
        face.degree = static_cast<uint32_t>(poly.size());

        const uint32_t base_he = static_cast<uint32_t>(half_edges.size());
        face.half_edge = base_he;
        half_edges.resize(half_edges.size() + poly.size());

        for (size_t i = 0; i < poly.size(); ++i) {
            const uint32_t u = poly[i];
            const uint32_t v = poly[(i + 1) % poly.size()];
            if (u >= vertices.size() || v >= vertices.size() || u == v) {
                non_manifold_detected_ = true;
                continue;
            }
            const uint32_t he_idx = base_he + static_cast<uint32_t>(i);
            auto& he = half_edges[he_idx];
            he.vertex = u;
            he.face = fid;
            he.next = base_he + static_cast<uint32_t>((i + 1) % poly.size());
            he.prev = base_he + static_cast<uint32_t>((i + poly.size() - 1) % poly.size());

            if (vertices[u].half_edge == HE_INVALID) {
                vertices[u].half_edge = he_idx;
            }

            if (dir_map.contains({u, v})) {
                non_manifold_detected_ = true;
            } else {
                dir_map[{u, v}] = he_idx;
            }

            const UndirEdge uk = (u < v) ? UndirEdge{u, v} : UndirEdge{v, u};
            if (++undir_count[uk] > 2) {
                non_manifold_detected_ = true;
            }
        }
        faces.push_back(face);
    }

    // Link twin half-edges & point boundary vertices to their boundary outgoing half-edge
    // so 1-ring circulators can sweep the entire boundary fan seamlessly in O(deg(v)).
    for (uint32_t he_idx = 0; he_idx < half_edges.size(); ++he_idx) {
        const uint32_t u = half_edges[he_idx].vertex;
        const uint32_t v = target_vertex(he_idx);
        if (v == HE_INVALID) continue;
        auto it = dir_map.find({v, u});
        if (it != dir_map.end()) {
            half_edges[he_idx].twin = it->second;
        }
    }

    for (uint32_t v = 0; v < vertices.size(); ++v) {
        const uint32_t start = vertices[v].half_edge;
        if (start == HE_INVALID) continue;
        // Walk clockwise around v until hitting boundary (prev->twin == HE_INVALID) or returning to start
        uint32_t cur = start;
        size_t guard = 64;
        while (guard-- > 0) {
            const uint32_t p = half_edges[cur].prev;
            if (p >= half_edges.size() || half_edges[p].twin == HE_INVALID) {
                vertices[v].half_edge = cur; // boundary-rightmost outgoing half-edge
                break;
            }
            cur = half_edges[p].twin;
            if (cur == start) break;
        }
    }

    compute_normals();
    return !non_manifold_detected_;
}

bool HalfEdgeMesh::build_from_triangles(
    const std::vector<Point3D>& verts,
    const std::vector<std::array<uint32_t, 3>>& tris
) {
    std::vector<std::vector<uint32_t>> polys;
    polys.reserve(tris.size());
    for (const auto& t : tris) {
        polys.push_back({t[0], t[1], t[2]});
    }
    return build_from_polygons(verts, polys);
}

bool HalfEdgeMesh::build_from_solid(const SolidMesh3D& solid) {
    return build_from_triangles(solid.vertices, solid.triangles);
}

SolidMesh3D HalfEdgeMesh::to_solid_mesh() const {
    SolidMesh3D out;
    out.vertices.reserve(vertices.size());
    for (const auto& v : vertices) {
        out.add_vertex(v.position);
    }
    for (uint32_t fid = 0; fid < faces.size(); ++fid) {
        const auto fv = MeshCirculator::face_vertices(*this, fid);
        if (fv.size() < 3) continue;
        for (size_t i = 2; i < fv.size(); ++i) {
            out.add_triangle(fv[0], fv[i - 1], fv[i]);
        }
    }
    return out;
}

void HalfEdgeMesh::compute_normals() {
    for (auto& v : vertices) v.normal = {0.0, 0.0, 0.0};

    for (uint32_t fid = 0; fid < faces.size(); ++fid) {
        const auto fv = MeshCirculator::face_vertices(*this, fid);
        if (fv.size() < 3) continue;
        Vec3d accum{0.0, 0.0, 0.0};
        const Point3D& p0 = vertices[fv[0]].position;
        for (size_t i = 2; i < fv.size(); ++i) {
            const Vec3d ab = vertices[fv[i - 1]].position - p0;
            const Vec3d ac = vertices[fv[i]].position - p0;
            accum += ab.cross(ac);
        }
        faces[fid].normal = accum.normalized();
        for (const uint32_t vi : fv) {
            vertices[vi].normal += accum;
        }
    }

    for (auto& v : vertices) {
        if (v.normal.length_squared() > 1e-24) {
            v.normal = v.normal.normalized();
        }
    }
}

bool HalfEdgeMesh::is_boundary_vertex(uint32_t v_idx) const noexcept {
    if (v_idx >= vertices.size() || vertices[v_idx].half_edge == HE_INVALID) return false;
    const auto out = MeshCirculator::outgoing_half_edges(*this, v_idx);
    for (const uint32_t he : out) {
        if (is_boundary_half_edge(he)) return true;
        const uint32_t prev = half_edges[he].prev;
        if (prev < half_edges.size() && is_boundary_half_edge(prev)) return true;
    }
    return false;
}

uint32_t HalfEdgeMesh::vertex_valency(uint32_t v_idx) const noexcept {
    return static_cast<uint32_t>(MeshCirculator::vertex_neighbors(*this, v_idx).size());
}

bool HalfEdgeMesh::verify_twin_consistency() const noexcept {
    for (uint32_t i = 0; i < half_edges.size(); ++i) {
        const uint32_t tw = half_edges[i].twin;
        if (tw == HE_INVALID) continue;
        if (tw >= half_edges.size() || half_edges[tw].twin != i) return false;
        if (half_edges[tw].vertex != target_vertex(i)) return false;
        if (target_vertex(tw) != half_edges[i].vertex) return false;
    }
    return true;
}

ManifoldReport HalfEdgeMesh::verify_topology() const {
    ManifoldReport rep;
    rep.vertex_count = vertices.size();
    rep.face_count = faces.size();
    size_t interior_pairs = 0;
    size_t boundary_hes = 0;
    for (uint32_t i = 0; i < half_edges.size(); ++i) {
        if (half_edges[i].twin == HE_INVALID) {
            ++boundary_hes;
        } else {
            ++interior_pairs;
        }
    }
    rep.edge_count = (interior_pairs / 2) + boundary_hes;
    rep.boundary_edge_count = static_cast<int>(boundary_hes);
    rep.non_manifold_edge_count = non_manifold_detected_ ? 1 : 0;
    rep.euler_characteristic =
        static_cast<int64_t>(rep.vertex_count) -
        static_cast<int64_t>(rep.edge_count) +
        static_cast<int64_t>(rep.face_count);
    rep.is_watertight = !faces.empty() && rep.boundary_edge_count == 0;
    rep.is_2_manifold = !faces.empty() && !non_manifold_detected_ && rep.boundary_edge_count == 0 && verify_twin_consistency();
    return rep;
}

std::vector<uint32_t> MeshCirculator::outgoing_half_edges(
    const HalfEdgeMesh& mesh,
    uint32_t v_idx
) {
    std::vector<uint32_t> out;
    if (v_idx >= mesh.vertices.size()) return out;
    const uint32_t start = mesh.vertices[v_idx].half_edge;
    if (start == HE_INVALID || start >= mesh.half_edges.size()) return out;

    // Advance CCW: next outgoing around v is half_edges[he].twin -> next
    uint32_t cur = start;
    size_t guard = 256;
    do {
        out.push_back(cur);
        const uint32_t tw = mesh.half_edges[cur].twin;
        if (tw == HE_INVALID || tw >= mesh.half_edges.size()) break;
        cur = mesh.half_edges[tw].next;
    } while (cur != start && cur != HE_INVALID && --guard > 0);
    return out;
}

std::vector<uint32_t> MeshCirculator::vertex_neighbors(
    const HalfEdgeMesh& mesh,
    uint32_t v_idx
) {
    std::vector<uint32_t> nbrs;
    const auto outs = outgoing_half_edges(mesh, v_idx);
    nbrs.reserve(outs.size() + 1);
    uint32_t last_he = HE_INVALID;
    for (const uint32_t he : outs) {
        const uint32_t tgt = mesh.target_vertex(he);
        if (tgt != HE_INVALID) nbrs.push_back(tgt);
        last_he = he;
    }
    // If open boundary fan, include the leftmost incoming boundary neighbor vertex (prev->vertex)
    if (last_he != HE_INVALID) {
        const uint32_t prev = mesh.half_edges[last_he].prev;
        if (prev < mesh.half_edges.size() && mesh.half_edges[prev].twin == HE_INVALID) {
            const uint32_t left_v = mesh.half_edges[prev].vertex;
            if (std::find(nbrs.begin(), nbrs.end(), left_v) == nbrs.end()) {
                nbrs.push_back(left_v);
            }
        }
    }
    return nbrs;
}

std::vector<uint32_t> MeshCirculator::incident_faces(
    const HalfEdgeMesh& mesh,
    uint32_t v_idx
) {
    std::vector<uint32_t> fids;
    const auto outs = outgoing_half_edges(mesh, v_idx);
    fids.reserve(outs.size());
    for (const uint32_t he : outs) {
        const uint32_t f = mesh.half_edges[he].face;
        if (f != HE_INVALID) fids.push_back(f);
    }
    return fids;
}

std::vector<uint32_t> MeshCirculator::face_vertices(
    const HalfEdgeMesh& mesh,
    uint32_t f_idx
) {
    std::vector<uint32_t> fverts;
    if (f_idx >= mesh.faces.size()) return fverts;
    const uint32_t start = mesh.faces[f_idx].half_edge;
    if (start == HE_INVALID || start >= mesh.half_edges.size()) return fverts;
    uint32_t cur = start;
    size_t guard = 256;
    do {
        fverts.push_back(mesh.half_edges[cur].vertex);
        cur = mesh.half_edges[cur].next;
    } while (cur != start && cur != HE_INVALID && --guard > 0);
    return fverts;
}

} // namespace shunya::cad3d
