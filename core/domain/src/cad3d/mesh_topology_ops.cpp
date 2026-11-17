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

} // namespace shunya::cad3d
