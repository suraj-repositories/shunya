#include "shunya/cad3d/polyhedral_brep.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <unordered_map>
#include <utility>

namespace shunya::cad3d {

PolyhedralBRep PolyhedralBRep::from_mesh(
    const SolidMesh3D& mesh,
    double coplanar_angle_tol_rad
) {
    PolyhedralBRep brep;
    if (mesh.vertices.empty() || mesh.triangles.empty()) return brep;

    SolidMesh3D clean = mesh;
    clean.weld_vertices(1e-7);
    clean.ensure_outward_winding();
    brep.vertices = clean.vertices;

    const size_t num_tris = clean.triangles.size();
    std::vector<Vec3d> tri_normals(num_tris);
    for (size_t i = 0; i < num_tris; ++i) {
        tri_normals[i] = clean.triangle_normal(i);
    }

    // Build triangle adjacency across shared undirected edges
    using UndirEdge = std::pair<uint32_t, uint32_t>;
    std::map<UndirEdge, std::vector<size_t>> edge_tris;
    for (size_t ti = 0; ti < num_tris; ++ti) {
        const auto& t = clean.triangles[ti];
        for (int e = 0; e < 3; ++e) {
            const uint32_t u = t[static_cast<size_t>(e)];
            const uint32_t v = t[static_cast<size_t>((e + 1) % 3)];
            edge_tris[(u < v) ? UndirEdge{u, v} : UndirEdge{v, u}].push_back(ti);
        }
    }

    const double cos_tol = std::cos(coplanar_angle_tol_rad);
    std::vector<int> tri_face(num_tris, -1);

    // Flood-fill coplanar triangle regions into B-Rep faces
    for (size_t seed = 0; seed < num_tris; ++seed) {
        if (tri_face[seed] >= 0) continue;
        const int fid = static_cast<int>(brep.faces.size());
        BrepFace face;
        face.id = static_cast<uint32_t>(fid);
        Vec3d n_accum{0.0, 0.0, 0.0};

        std::queue<size_t> q;
        q.push(seed);
        tri_face[seed] = fid;

        while (!q.empty()) {
            const size_t cur = q.front();
            q.pop();
            face.source_triangles.push_back(cur);
            const auto& ct = clean.triangles[cur];
            const Vec3d ab = clean.vertices[ct[1]] - clean.vertices[ct[0]];
            const Vec3d ac = clean.vertices[ct[2]] - clean.vertices[ct[0]];
            n_accum += ab.cross(ac);

            for (int e = 0; e < 3; ++e) {
                const uint32_t u = ct[static_cast<size_t>(e)];
                const uint32_t v = ct[static_cast<size_t>((e + 1) % 3)];
                const UndirEdge ek = (u < v) ? UndirEdge{u, v} : UndirEdge{v, u};
                for (const size_t nbr : edge_tris[ek]) {
                    if (tri_face[nbr] >= 0) continue;
                    if (tri_normals[cur].dot(tri_normals[nbr]) >= cos_tol &&
                        tri_normals[seed].dot(tri_normals[nbr]) >= cos_tol) {
                        tri_face[nbr] = fid;
                        q.push(nbr);
                    }
                }
            }
        }

        face.normal = n_accum.normalized();
        if (face.normal.length_squared() < 0.5) {
            face.normal = tri_normals[seed];
        }
        const Vec3d ref = (std::abs(face.normal.x) < 0.8) ? Vec3d{1.0, 0.0, 0.0} : Vec3d{0.0, 1.0, 0.0};
        face.axis_u = (ref - face.normal * face.normal.dot(ref)).normalized();
        face.axis_v = face.normal.cross(face.axis_u).normalized();

        // Extract boundary half-edges of this coplanar region (directed edges without internal twin)
        std::map<std::pair<uint32_t, uint32_t>, int> region_edges;
        for (const size_t ti : face.source_triangles) {
            const auto& t = clean.triangles[ti];
            for (int e = 0; e < 3; ++e) {
                const uint32_t u = t[static_cast<size_t>(e)];
                const uint32_t v = t[static_cast<size_t>((e + 1) % 3)];
                auto twin_it = region_edges.find({v, u});
                if (twin_it != region_edges.end() && twin_it->second > 0) {
                    --twin_it->second;
                    if (twin_it->second == 0) region_edges.erase(twin_it);
                } else {
                    ++region_edges[{u, v}];
                }
            }
        }

        // Chain directed boundary edges into outer vertex loop
        std::unordered_map<uint32_t, uint32_t> next_v;
        for (const auto& [uv, cnt] : region_edges) {
            if (cnt > 0) {
                next_v[uv.first] = uv.second;
            }
        }
        if (!next_v.empty()) {
            const uint32_t start_v = region_edges.begin()->first.first;
            uint32_t cur_v = start_v;
            size_t guard = region_edges.size() + 4;
            do {
                face.outer_loop.push_back(cur_v);
                auto it = next_v.find(cur_v);
                if (it == next_v.end()) break;
                cur_v = it->second;
            } while (cur_v != start_v && --guard > 0);
        }

        if (!face.outer_loop.empty()) {
            // Merge collinear vertices along the outer boundary loop if angle ~ 0
            std::vector<uint32_t> reduced;
            const size_t ln = face.outer_loop.size();
            for (size_t i = 0; i < ln; ++i) {
                const uint32_t vp = face.outer_loop[(i + ln - 1) % ln];
                const uint32_t vc = face.outer_loop[i];
                const uint32_t vn = face.outer_loop[(i + 1) % ln];
                const Vec3d e0 = (clean.vertices[vc] - clean.vertices[vp]).normalized();
                const Vec3d e1 = (clean.vertices[vn] - clean.vertices[vc]).normalized();
                if (e0.cross(e1).length_squared() > 1e-10) {
                    reduced.push_back(vc);
                }
            }
            if (reduced.size() >= 3) {
                face.outer_loop = std::move(reduced);
            }
            Point3D centroid{0.0, 0.0, 0.0};
            for (const uint32_t vi : face.outer_loop) centroid += clean.vertices[vi];
            face.origin = centroid * (1.0 / static_cast<double>(face.outer_loop.size()));
        }

        brep.faces.push_back(std::move(face));
    }

    // Build topological B-Rep edges between distinct faces
    std::map<UndirEdge, std::pair<uint32_t, int>> half_map; // key -> {first_face, dir(+1/-1)}
    for (const auto& f : brep.faces) {
        const size_t ln = f.outer_loop.size();
        for (size_t i = 0; i < ln; ++i) {
            const uint32_t a = f.outer_loop[i];
            const uint32_t b = f.outer_loop[(i + 1) % ln];
            const UndirEdge ek = (a < b) ? UndirEdge{a, b} : UndirEdge{b, a};
            auto it = half_map.find(ek);
            if (it == half_map.end()) {
                half_map[ek] = {f.id, (a < b) ? 1 : -1};
            } else {
                const uint32_t other_f = it->second.first;
                if (other_f == f.id) continue;
                BrepEdge edge;
                edge.id = static_cast<uint32_t>(brep.edges.size());
                // Orient so (v0 -> v1) runs along face_left
                if (it->second.second > 0) {
                    edge.v0 = ek.first;
                    edge.v1 = ek.second;
                    edge.face_left = other_f;
                    edge.face_right = f.id;
                } else {
                    edge.v0 = a;
                    edge.v1 = b;
                    edge.face_left = f.id;
                    edge.face_right = other_f;
                }
                const double d = std::clamp(
                    brep.faces[edge.face_left].normal.dot(brep.faces[edge.face_right].normal),
                    -1.0, 1.0
                );
                edge.dihedral_angle_rad = std::acos(d);
                brep.edges.push_back(edge);
            }
        }
    }

    return brep;
}

} // namespace shunya::cad3d
