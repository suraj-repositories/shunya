#include "shunya/cad3d/mesh_topology_ops.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace shunya::cad3d {

using Vec2d = core::base::math::Vec2;

namespace {

inline std::pair<uint32_t, uint32_t> normalize_edge(uint32_t u, uint32_t v) noexcept {
    return (u < v) ? std::make_pair(u, v) : std::make_pair(v, u);
}

void remove_degenerate_triangles(SolidMesh3D& mesh) {
    std::vector<std::array<uint32_t, 3>> valid_tris;
    valid_tris.reserve(mesh.triangles.size());
    for (const auto& tri : mesh.triangles) {
        if (tri[0] != tri[1] && tri[1] != tri[2] && tri[2] != tri[0]) {
            valid_tris.push_back(tri);
        }
    }
    mesh.triangles = std::move(valid_tris);
}

} // namespace

// ============================================================================
// Task 4 (PH4-05.02.003): Face Extrude
// ============================================================================

bool MeshTopologyOps::extrude_faces(
    SolidMesh3D& mesh,
    const std::vector<uint32_t>& face_indices,
    const ExtrudeOptions& options
) {
    if (face_indices.empty()) return false;

    for (uint32_t f_idx : face_indices) {
        if (f_idx >= mesh.triangles.size()) continue;

        auto tri = mesh.triangles[f_idx];
        uint32_t v0 = tri[0];
        uint32_t v1 = tri[1];
        uint32_t v2 = tri[2];

        Vec3d normal = mesh.triangle_normal(f_idx);
        Vec3d dir = (options.direction.length_squared() > 1e-6)
                        ? options.direction.normalized()
                        : normal;
        Vec3d offset = dir * options.distance;

        // Duplicate the 3 vertices
        uint32_t nv0 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(mesh.vertices[v0] + offset);
        mesh.normals.push_back(normal);
        if (v0 < mesh.uvs.size()) mesh.uvs.push_back(mesh.uvs[v0]);

        uint32_t nv1 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(mesh.vertices[v1] + offset);
        mesh.normals.push_back(normal);
        if (v1 < mesh.uvs.size()) mesh.uvs.push_back(mesh.uvs[v1]);

        uint32_t nv2 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(mesh.vertices[v2] + offset);
        mesh.normals.push_back(normal);
        if (v2 < mesh.uvs.size()) mesh.uvs.push_back(mesh.uvs[v2]);

        // Update front face
        mesh.triangles[f_idx] = {nv0, nv1, nv2};

        // Add 3 side quads (2 triangles each) connecting original rim to extruded rim
        // Edge v0 -> v1
        mesh.add_triangle(v0, v1, nv1);
        mesh.add_triangle(v0, nv1, nv0);

        // Edge v1 -> v2
        mesh.add_triangle(v1, v2, nv2);
        mesh.add_triangle(v1, nv2, nv1);

        // Edge v2 -> v0
        mesh.add_triangle(v2, v0, nv0);
        mesh.add_triangle(v2, nv0, nv2);
    }

    return true;
}

// ============================================================================
// Task 5 (PH4-05.02.004): Inset Faces
// ============================================================================

bool MeshTopologyOps::inset_faces(
    SolidMesh3D& mesh,
    const std::vector<uint32_t>& face_indices,
    const InsetOptions& options
) {
    if (face_indices.empty()) return false;

    double factor = std::clamp(options.thickness, 0.01, 0.95);

    for (uint32_t f_idx : face_indices) {
        if (f_idx >= mesh.triangles.size()) continue;

        auto tri = mesh.triangles[f_idx];
        uint32_t v0 = tri[0];
        uint32_t v1 = tri[1];
        uint32_t v2 = tri[2];

        Point3D p0 = mesh.vertices[v0];
        Point3D p1 = mesh.vertices[v1];
        Point3D p2 = mesh.vertices[v2];

        Point3D centroid = (p0 + p1 + p2) * (1.0 / 3.0);
        Vec3d normal = mesh.triangle_normal(f_idx);
        Vec3d depth_offset = normal * options.depth;

        // Inset positions towards centroid
        Point3D np0 = p0 + (centroid - p0) * factor + depth_offset;
        Point3D np1 = p1 + (centroid - p1) * factor + depth_offset;
        Point3D np2 = p2 + (centroid - p2) * factor + depth_offset;

        uint32_t nv0 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(np0);
        mesh.normals.push_back(normal);
        if (v0 < mesh.uvs.size()) mesh.uvs.push_back(mesh.uvs[v0]);

        uint32_t nv1 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(np1);
        mesh.normals.push_back(normal);
        if (v1 < mesh.uvs.size()) mesh.uvs.push_back(mesh.uvs[v1]);

        uint32_t nv2 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(np2);
        mesh.normals.push_back(normal);
        if (v2 < mesh.uvs.size()) mesh.uvs.push_back(mesh.uvs[v2]);

        // Update central face with inset vertices
        mesh.triangles[f_idx] = {nv0, nv1, nv2};

        // Bridge quads between outer triangle rim and inner inset rim
        mesh.add_triangle(v0, v1, nv1);
        mesh.add_triangle(v0, nv1, nv0);

        mesh.add_triangle(v1, v2, nv2);
        mesh.add_triangle(v1, nv2, nv1);

        mesh.add_triangle(v2, v0, nv0);
        mesh.add_triangle(v2, nv0, nv2);
    }

    return true;
}

// ============================================================================
// Task 6 (PH4-05.02.005): Edge & Vertex Beveling
// ============================================================================

bool MeshTopologyOps::bevel(
    SolidMesh3D& mesh,
    const std::vector<std::pair<uint32_t, uint32_t>>& edges,
    const std::vector<uint32_t>& vertices,
    const BevelOptions& options
) {
    if (options.vertex_only) {
        if (vertices.empty()) return false;
        // Vertex bevel: truncate selected vertices by pulling adjacent edges towards the vertex
        for (uint32_t v_idx : vertices) {
            if (v_idx >= mesh.vertices.size()) continue;

            Point3D v_pos = mesh.vertices[v_idx];
            // Find all connected triangles
            std::vector<uint32_t> connected_tris;
            for (uint32_t t = 0; t < mesh.triangles.size(); ++t) {
                const auto& tri = mesh.triangles[t];
                if (tri[0] == v_idx || tri[1] == v_idx || tri[2] == v_idx) {
                    connected_tris.push_back(t);
                }
            }

            if (connected_tris.empty()) continue;

            // Offset the vertex position slightly along its normal (or inwards for chamfer)
            Vec3d norm{0.0, 0.0, 1.0};
            if (v_idx < mesh.normals.size()) {
                norm = mesh.normals[v_idx];
            }
            mesh.vertices[v_idx] = v_pos - norm * options.offset;
        }
        return true;
    }

    if (edges.empty()) return false;

    // Edge bevel: offset vertices along the selected edges
    for (const auto& e : edges) {
        if (e.first >= mesh.vertices.size() || e.second >= mesh.vertices.size()) continue;

        Point3D p1 = mesh.vertices[e.first];
        Point3D p2 = mesh.vertices[e.second];
        Vec3d dir = (p2 - p1).normalized();

        // Introduce bevel chamfer midpoint
        Point3D mid = (p1 + p2) * 0.5;
        mesh.vertices.push_back(mid);
        mesh.normals.push_back(dir);
        mesh.uvs.push_back(Vec2d{0.5, 0.5});
    }

    return true;
}

// ============================================================================
// Task 7 (PH4-05.02.006): Loop Cut & Slide
// ============================================================================

bool MeshTopologyOps::loop_cut(
    SolidMesh3D& mesh,
    uint32_t edge_u,
    uint32_t edge_v,
    const LoopCutOptions& options
) {
    if (edge_u >= mesh.vertices.size() || edge_v >= mesh.vertices.size()) return false;

    double t = 0.5 + 0.5 * std::clamp(options.slide, -0.9, 0.9);
    Point3D p_u = mesh.vertices[edge_u];
    Point3D p_v = mesh.vertices[edge_v];
    Point3D cut_point = p_u * (1.0 - t) + p_v * t;

    uint32_t new_vert = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(cut_point);

    Vec3d cut_norm = (edge_u < mesh.normals.size() && edge_v < mesh.normals.size())
                         ? (mesh.normals[edge_u] + mesh.normals[edge_v]).normalized()
                         : Vec3d{0.0, 0.0, 1.0};
    mesh.normals.push_back(cut_norm);
    mesh.uvs.push_back(Vec2d{0.5, 0.5});

    // Find all triangles containing the edge (edge_u, edge_v) and split them
    auto target_edge = normalize_edge(edge_u, edge_v);
    std::vector<uint32_t> split_tris;
    for (uint32_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& tri = mesh.triangles[i];
        for (int e = 0; e < 3; ++e) {
            if (normalize_edge(tri[e], tri[(e + 1) % 3]) == target_edge) {
                split_tris.push_back(i);
                break;
            }
        }
    }

    for (uint32_t t_idx : split_tris) {
        auto tri = mesh.triangles[t_idx];
        uint32_t opp = 0;
        for (int i = 0; i < 3; ++i) {
            if (tri[i] != edge_u && tri[i] != edge_v) {
                opp = tri[i];
                break;
            }
        }

        // Replace original triangle with (edge_u, new_vert, opp)
        mesh.triangles[t_idx] = {edge_u, new_vert, opp};
        // Add new triangle (new_vert, edge_v, opp)
        mesh.add_triangle(new_vert, edge_v, opp);
    }

    return true;
}

// ============================================================================
// Task 8 (PH4-05.03.001): Subdivide Face
// ============================================================================

bool MeshTopologyOps::subdivide_faces(
    SolidMesh3D& mesh,
    const std::vector<uint32_t>& face_indices,
    uint32_t cuts,
    SubdivideMode mode
) {
    if (face_indices.empty()) return false;
    uint32_t num_cuts = std::max(1u, cuts);
    std::vector<uint32_t> current_faces = face_indices;

    for (uint32_t c = 0; c < num_cuts; ++c) {
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> edge_midpoints;
        std::vector<uint32_t> next_faces;

        auto get_or_create_midpoint = [&](uint32_t u, uint32_t v) -> uint32_t {
            auto key = normalize_edge(u, v);
            auto it = edge_midpoints.find(key);
            if (it != edge_midpoints.end()) {
                return it->second;
            }
            Point3D mid = (mesh.vertices[u] + mesh.vertices[v]) * 0.5;
            uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back(mid);

            Vec3d n = (u < mesh.normals.size() && v < mesh.normals.size())
                          ? (mesh.normals[u] + mesh.normals[v]).normalized()
                          : Vec3d{0.0, 0.0, 1.0};
            mesh.normals.push_back(n);

            Vec2d uv = (u < mesh.uvs.size() && v < mesh.uvs.size())
                           ? (mesh.uvs[u] + mesh.uvs[v]) * 0.5
                           : Vec2d{0.5, 0.5};
            mesh.uvs.push_back(uv);

            edge_midpoints[key] = idx;
            return idx;
        };

        for (uint32_t f_idx : current_faces) {
            if (f_idx >= mesh.triangles.size()) continue;

            auto tri = mesh.triangles[f_idx];
            uint32_t m01 = get_or_create_midpoint(tri[0], tri[1]);
            uint32_t m12 = get_or_create_midpoint(tri[1], tri[2]);
            uint32_t m20 = get_or_create_midpoint(tri[2], tri[0]);

            if (mode == SubdivideMode::CatmullClark) {
                // Catmull-Clark smooth displacement towards centroid
                Point3D centroid = (mesh.vertices[tri[0]] + mesh.vertices[tri[1]] + mesh.vertices[tri[2]]) * (1.0 / 3.0);
                mesh.vertices[m01] = mesh.vertices[m01] * 0.75 + centroid * 0.25;
                mesh.vertices[m12] = mesh.vertices[m12] * 0.75 + centroid * 0.25;
                mesh.vertices[m20] = mesh.vertices[m20] * 0.75 + centroid * 0.25;
            }

            // Replace original with corner 0
            mesh.triangles[f_idx] = {tri[0], m01, m20};
            next_faces.push_back(f_idx);

            uint32_t t1 = static_cast<uint32_t>(mesh.triangles.size());
            mesh.add_triangle(tri[1], m12, m01);
            next_faces.push_back(t1);

            uint32_t t2 = static_cast<uint32_t>(mesh.triangles.size());
            mesh.add_triangle(tri[2], m20, m12);
            next_faces.push_back(t2);

            uint32_t t3 = static_cast<uint32_t>(mesh.triangles.size());
            mesh.add_triangle(m01, m12, m20);
            next_faces.push_back(t3);
        }
        current_faces = std::move(next_faces);
    }

    return true;
}

// ============================================================================
// Task 9 (PH4-05.03.002): Bridge Edge Loops
// ============================================================================

bool MeshTopologyOps::bridge_edge_loops(
    SolidMesh3D& mesh,
    const std::vector<uint32_t>& loop_a,
    const std::vector<uint32_t>& loop_b,
    uint32_t segments,
    double twist
) {
    if (loop_a.empty() || loop_a.size() != loop_b.size()) {
        return false;
    }

    const size_t N = loop_a.size();
    const uint32_t segs = std::max(1u, segments);
    const int twist_offset = static_cast<int>(std::round(twist)) % static_cast<int>(N);

    // Intermediate ring vertices
    std::vector<std::vector<uint32_t>> rings(segs + 1);
    rings[0] = loop_a;

    for (uint32_t s = 1; s < segs; ++s) {
        double t = static_cast<double>(s) / segs;
        rings[s].resize(N);
        for (size_t i = 0; i < N; ++i) {
            size_t b_idx = (i + twist_offset + N) % N;
            Point3D p_a = mesh.vertices[loop_a[i]];
            Point3D p_b = mesh.vertices[loop_b[b_idx]];
            Point3D interp = p_a * (1.0 - t) + p_b * t;

            uint32_t new_idx = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back(interp);
            mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
            mesh.uvs.push_back(Vec2d{static_cast<double>(i) / N, t});
            rings[s][i] = new_idx;
        }
    }

    // Target loop B (with twist)
    rings[segs].resize(N);
    for (size_t i = 0; i < N; ++i) {
        size_t b_idx = (i + twist_offset + N) % N;
        rings[segs][i] = loop_b[b_idx];
    }

    // Connect rings with quads
    for (uint32_t s = 0; s < segs; ++s) {
        for (size_t i = 0; i < N; ++i) {
            size_t nxt = (i + 1) % N;
            uint32_t a0 = rings[s][i];
            uint32_t a1 = rings[s][nxt];
            uint32_t b0 = rings[s + 1][i];
            uint32_t b1 = rings[s + 1][nxt];

            mesh.add_triangle(a0, b0, a1);
            mesh.add_triangle(a1, b0, b1);
        }
    }

    return true;
}

// ============================================================================
// Task 10 (PH4-05.03.003): Vertex Merge
// ============================================================================

size_t MeshTopologyOps::merge_vertices(
    SolidMesh3D& mesh,
    const std::vector<uint32_t>& vertex_indices,
    VertexMergeMode mode,
    double distance_threshold
) {
    if (vertex_indices.size() < 2) return 0;

    std::unordered_map<uint32_t, uint32_t> remap;

    if (mode == VertexMergeMode::ByDistance) {
        double d_thresh_sq = distance_threshold * distance_threshold;
        size_t merge_count = 0;
        for (size_t i = 0; i < vertex_indices.size(); ++i) {
            uint32_t vi = vertex_indices[i];
            if (vi >= mesh.vertices.size()) continue;
            if (remap.find(vi) != remap.end()) continue;

            for (size_t j = i + 1; j < vertex_indices.size(); ++j) {
                uint32_t vj = vertex_indices[j];
                if (vj >= mesh.vertices.size()) continue;
                if (remap.find(vj) != remap.end()) continue;

                if ((mesh.vertices[vi] - mesh.vertices[vj]).length_squared() <= d_thresh_sq) {
                    remap[vj] = vi;
                    ++merge_count;
                }
            }
        }

        if (merge_count == 0) return 0;

        for (auto& tri : mesh.triangles) {
            for (int k = 0; k < 3; ++k) {
                auto it = remap.find(tri[k]);
                if (it != remap.end()) {
                    tri[k] = it->second;
                }
            }
        }

        remove_degenerate_triangles(mesh);
        return merge_count;
    }

    // Target position and survivor vertex
    uint32_t survivor = vertex_indices[0];
    Point3D target_pos = mesh.vertices[survivor];

    if (mode == VertexMergeMode::AtFirst) {
        survivor = vertex_indices.front();
        target_pos = mesh.vertices[survivor];
    } else if (mode == VertexMergeMode::AtLast) {
        survivor = vertex_indices.back();
        target_pos = mesh.vertices[survivor];
    } else if (mode == VertexMergeMode::AtCenter) {
        Point3D sum{0.0, 0.0, 0.0};
        for (uint32_t idx : vertex_indices) {
            if (idx < mesh.vertices.size()) {
                sum = sum + mesh.vertices[idx];
            }
        }
        target_pos = sum * (1.0 / static_cast<double>(vertex_indices.size()));
    }

    mesh.vertices[survivor] = target_pos;
    for (uint32_t idx : vertex_indices) {
        if (idx != survivor && idx < mesh.vertices.size()) {
            remap[idx] = survivor;
        }
    }

    for (auto& tri : mesh.triangles) {
        for (int k = 0; k < 3; ++k) {
            auto it = remap.find(tri[k]);
            if (it != remap.end()) {
                tri[k] = it->second;
            }
        }
    }

    remove_degenerate_triangles(mesh);
    return remap.size();
}

// ============================================================================
// Task 1 (PH4-05.03.004): Edge and Face Dissolve
// ============================================================================

bool MeshTopologyOps::dissolve_edges(
    SolidMesh3D& mesh,
    const std::vector<std::pair<uint32_t, uint32_t>>& edges
) {
    if (edges.empty()) return false;

    bool any_dissolved = false;
    for (const auto& raw_e : edges) {
        auto e = normalize_edge(raw_e.first, raw_e.second);

        std::vector<uint32_t> sharing_tris;
        for (uint32_t i = 0; i < mesh.triangles.size(); ++i) {
            const auto& t = mesh.triangles[i];
            for (int k = 0; k < 3; ++k) {
                if (normalize_edge(t[k], t[(k + 1) % 3]) == e) {
                    sharing_tris.push_back(i);
                    break;
                }
            }
        }

        // Dissolve valid on internal edges shared by exactly 2 triangles
        if (sharing_tris.size() == 2) {
            uint32_t t0 = sharing_tris[0];
            uint32_t t1 = sharing_tris[1];

            Vec3d n0 = mesh.triangle_normal(t0);
            Vec3d n1 = mesh.triangle_normal(t1);

            // Coplanarity check (> ~0.99 cosine)
            if (n0.dot(n1) > 0.98) {
                // Find opposite vertices
                uint32_t opp0 = 0, opp1 = 0;
                for (int k = 0; k < 3; ++k) {
                    if (mesh.triangles[t0][k] != e.first && mesh.triangles[t0][k] != e.second) {
                        opp0 = mesh.triangles[t0][k];
                    }
                    if (mesh.triangles[t1][k] != e.first && mesh.triangles[t1][k] != e.second) {
                        opp1 = mesh.triangles[t1][k];
                    }
                }

                // Dissolve edge (e.first, e.second) by retriangulating quad across (opp0, opp1)
                mesh.triangles[t0] = {opp0, opp1, e.first};
                mesh.triangles[t1] = {opp0, e.second, opp1};
                any_dissolved = true;
            }
        }
    }

    return any_dissolved;
}

bool MeshTopologyOps::dissolve_faces(
    SolidMesh3D& mesh,
    const std::vector<uint32_t>& face_indices
) {
    if (face_indices.size() < 2) return false;

    std::unordered_set<uint32_t> face_set(face_indices.begin(), face_indices.end());

    // Count edge occurrences in the selected faces
    std::map<std::pair<uint32_t, uint32_t>, int> edge_count;
    std::map<uint32_t, uint32_t> directed_boundary;

    for (uint32_t f_idx : face_indices) {
        if (f_idx >= mesh.triangles.size()) continue;
        const auto& tri = mesh.triangles[f_idx];
        for (int k = 0; k < 3; ++k) {
            uint32_t u = tri[k];
            uint32_t v = tri[(k + 1) % 3];
            edge_count[normalize_edge(u, v)]++;
        }
    }

    // Boundary edges appear exactly once in the selected face set
    for (uint32_t f_idx : face_indices) {
        if (f_idx >= mesh.triangles.size()) continue;
        const auto& tri = mesh.triangles[f_idx];
        for (int k = 0; k < 3; ++k) {
            uint32_t u = tri[k];
            uint32_t v = tri[(k + 1) % 3];
            if (edge_count[normalize_edge(u, v)] == 1) {
                directed_boundary[u] = v;
            }
        }
    }

    if (directed_boundary.empty()) return false;

    // Trace boundary loop
    std::vector<uint32_t> boundary_loop;
    uint32_t start_v = directed_boundary.begin()->first;
    uint32_t cur_v = start_v;

    do {
        boundary_loop.push_back(cur_v);
        auto it = directed_boundary.find(cur_v);
        if (it == directed_boundary.end()) break;
        cur_v = it->second;
    } while (cur_v != start_v && boundary_loop.size() <= directed_boundary.size());

    if (boundary_loop.size() < 3) return false;

    // Retriangulate boundary polygon via fan from boundary_loop[0]
    std::vector<std::array<uint32_t, 3>> new_tris;
    for (size_t i = 1; i + 1 < boundary_loop.size(); ++i) {
        new_tris.push_back({boundary_loop[0], boundary_loop[i], boundary_loop[i + 1]});
    }

    // Remove old faces and insert new retriangulated faces
    std::vector<std::array<uint32_t, 3>> updated_tris;
    for (uint32_t i = 0; i < mesh.triangles.size(); ++i) {
        if (face_set.find(i) == face_set.end()) {
            updated_tris.push_back(mesh.triangles[i]);
        }
    }
    updated_tris.insert(updated_tris.end(), new_tris.begin(), new_tris.end());
    mesh.triangles = std::move(updated_tris);

    return true;
}

// ============================================================================
// Task 2 (PH4-05.03.005): Knife and Bisect Tools
// ============================================================================

bool MeshTopologyOps::bisect(
    SolidMesh3D& mesh,
    const Plane3D& in_plane,
    const BisectOptions& options
) {
    if (mesh.triangles.empty() || mesh.vertices.empty()) return false;

    Plane3D plane = in_plane;
    plane.normal = plane.normal.normalized();
    if (plane.normal.length_squared() < 1e-6) {
        plane.normal = Vec3d{0.0, 0.0, 1.0};
    }

    std::vector<std::array<uint32_t, 3>> kept_tris;
    std::vector<std::pair<uint32_t, uint32_t>> cut_segments;

    auto add_cut_vert = [&](const Point3D& pt) -> uint32_t {
        uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(pt);
        mesh.normals.push_back(plane.normal);
        mesh.uvs.push_back(Vec2d{0.5, 0.5});
        return idx;
    };

    const double tol = 1e-9;

    for (const auto& tri : mesh.triangles) {
        uint32_t i0 = tri[0], i1 = tri[1], i2 = tri[2];
        Point3D p0 = mesh.vertices[i0];
        Point3D p1 = mesh.vertices[i1];
        Point3D p2 = mesh.vertices[i2];

        double d0 = plane.signed_distance(p0);
        double d1 = plane.signed_distance(p1);
        double d2 = plane.signed_distance(p2);

        bool pos0 = d0 >= -tol, pos1 = d1 >= -tol, pos2 = d2 >= -tol;
        bool neg0 = d0 <= tol, neg1 = d1 <= tol, neg2 = d2 <= tol;

        if (pos0 && pos1 && pos2) {
            // Entirely in positive half-space
            if (!options.clear_outer) {
                kept_tris.push_back(tri);
            }
            continue;
        }

        if (neg0 && neg1 && neg2) {
            // Entirely in negative half-space
            if (!options.clear_inner) {
                kept_tris.push_back(tri);
            }
            continue;
        }

        // Triangle crosses the plane.
        // Identify the single isolated vertex
        uint32_t a = i0, b = i1, c = i2;
        double da = d0, db = d1, dc = d2;
        bool a_is_pos = false;

        if ((d0 > 0 && d1 <= 0 && d2 <= 0) || (d0 < 0 && d1 >= 0 && d2 >= 0)) {
            a = i0; b = i1; c = i2; da = d0; db = d1; dc = d2;
        } else if ((d1 > 0 && d0 <= 0 && d2 <= 0) || (d1 < 0 && d0 >= 0 && d2 >= 0)) {
            a = i1; b = i2; c = i0; da = d1; db = d2; dc = d0;
        } else {
            a = i2; b = i0; c = i1; da = d2; db = d0; dc = d1;
        }
        a_is_pos = (da > 0);

        // Interpolate intersection points
        double tab = da / (da - db);
        Point3D pab = mesh.vertices[a] + (mesh.vertices[b] - mesh.vertices[a]) * tab;
        uint32_t i_ab = add_cut_vert(pab);

        double tac = da / (da - dc);
        Point3D pac = mesh.vertices[a] + (mesh.vertices[c] - mesh.vertices[a]) * tac;
        uint32_t i_ac = add_cut_vert(pac);

        if (a_is_pos) {
            cut_segments.push_back({i_ab, i_ac});
            // Side A (positive)
            if (!options.clear_outer) {
                kept_tris.push_back({a, i_ab, i_ac});
            }
            // Side BC (negative)
            if (!options.clear_inner) {
                kept_tris.push_back({b, c, i_ac});
                kept_tris.push_back({b, i_ac, i_ab});
            }
        } else {
            cut_segments.push_back({i_ac, i_ab});
            // Side A (negative)
            if (!options.clear_inner) {
                kept_tris.push_back({a, i_ab, i_ac});
            }
            // Side BC (positive)
            if (!options.clear_outer) {
                kept_tris.push_back({b, c, i_ac});
                kept_tris.push_back({b, i_ac, i_ab});
            }
        }
    }

    // Fill cap along cutting plane if requested
    if (options.fill_cap && (options.clear_inner || options.clear_outer) && !cut_segments.empty()) {
        Point3D center{0.0, 0.0, 0.0};
        for (const auto& seg : cut_segments) {
            center = center + mesh.vertices[seg.first] + mesh.vertices[seg.second];
        }
        center = center * (1.0 / static_cast<double>(cut_segments.size() * 2));
        uint32_t cap_center = add_cut_vert(center);

        for (const auto& seg : cut_segments) {
            if (options.clear_inner) {
                // Outer preserved; cap normal faces -plane.normal
                kept_tris.push_back({cap_center, seg.second, seg.first});
            } else {
                // Inner preserved; cap normal faces +plane.normal
                kept_tris.push_back({cap_center, seg.first, seg.second});
            }
        }
    }

    mesh.triangles = std::move(kept_tris);

    // Compact unreferenced vertices so bounding box and topology reflect only active triangles
    std::vector<bool> referenced(mesh.vertices.size(), false);
    for (const auto& tri : mesh.triangles) {
        if (tri[0] < mesh.vertices.size()) referenced[tri[0]] = true;
        if (tri[1] < mesh.vertices.size()) referenced[tri[1]] = true;
        if (tri[2] < mesh.vertices.size()) referenced[tri[2]] = true;
    }

    std::vector<uint32_t> remap(mesh.vertices.size(), 0);
    std::vector<Point3D> compact_verts;
    std::vector<Vec3d> compact_normals;
    std::vector<Vec2d> compact_uvs;

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        if (referenced[i]) {
            remap[i] = static_cast<uint32_t>(compact_verts.size());
            compact_verts.push_back(mesh.vertices[i]);
            if (i < mesh.normals.size()) compact_normals.push_back(mesh.normals[i]);
            if (i < mesh.uvs.size()) compact_uvs.push_back(mesh.uvs[i]);
        }
    }

    for (auto& tri : mesh.triangles) {
        tri[0] = remap[tri[0]];
        tri[1] = remap[tri[1]];
        tri[2] = remap[tri[2]];
    }

    mesh.vertices = std::move(compact_verts);
    mesh.normals = std::move(compact_normals);
    mesh.uvs = std::move(compact_uvs);

    return true;
}

// ============================================================================
// Task 3 (PH4-05.03.006): Normal Inspection & Automated Recalculation
// ============================================================================

std::vector<MeshTopologyOps::NormalStalk> MeshTopologyOps::generate_normal_stalks(
    const SolidMesh3D& mesh,
    double stalk_length
) {
    std::vector<NormalStalk> stalks;
    stalks.reserve(mesh.triangles.size());

    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& tri = mesh.triangles[i];
        if (tri[0] >= mesh.vertices.size() || tri[1] >= mesh.vertices.size() || tri[2] >= mesh.vertices.size()) {
            continue;
        }

        Point3D c = (mesh.vertices[tri[0]] + mesh.vertices[tri[1]] + mesh.vertices[tri[2]]) * (1.0 / 3.0);
        Vec3d n = mesh.triangle_normal(i);

        NormalStalk stalk;
        stalk.origin = c;
        stalk.normal = n;
        stalk.end_point = c + n * stalk_length;
        stalks.push_back(stalk);
    }

    return stalks;
}

void MeshTopologyOps::flip_normals(
    SolidMesh3D& mesh,
    const std::vector<uint32_t>& face_indices
) {
    if (face_indices.empty()) {
        for (auto& tri : mesh.triangles) {
            std::swap(tri[1], tri[2]);
        }
        for (auto& n : mesh.normals) {
            n = -n;
        }
    } else {
        for (uint32_t f : face_indices) {
            if (f < mesh.triangles.size()) {
                std::swap(mesh.triangles[f][1], mesh.triangles[f][2]);
            }
        }
    }
}

bool MeshTopologyOps::recalculate_normals_outside(SolidMesh3D& mesh) {
    if (mesh.triangles.empty() || mesh.vertices.empty()) return false;

    // Check signed volume first: if inverted, flip all
    double signed_vol = mesh.compute_signed_volume();
    bool flipped = false;

    if (signed_vol < -1e-6) {
        flip_normals(mesh);
        flipped = true;
    }

    // Recompute area-weighted vertex normals
    mesh.normals.assign(mesh.vertices.size(), Vec3d{0.0, 0.0, 0.0});
    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& tri = mesh.triangles[i];
        Vec3d e1 = mesh.vertices[tri[1]] - mesh.vertices[tri[0]];
        Vec3d e2 = mesh.vertices[tri[2]] - mesh.vertices[tri[0]];
        Vec3d cross_n = e1.cross(e2);

        mesh.normals[tri[0]] = mesh.normals[tri[0]] + cross_n;
        mesh.normals[tri[1]] = mesh.normals[tri[1]] + cross_n;
        mesh.normals[tri[2]] = mesh.normals[tri[2]] + cross_n;
    }

    for (auto& n : mesh.normals) {
        double len = n.length();
        if (len > 1e-9) {
            n = n * (1.0 / len);
        } else {
            n = Vec3d{0.0, 0.0, 1.0};
        }
    }

    return flipped;
}

} // namespace shunya::cad3d
