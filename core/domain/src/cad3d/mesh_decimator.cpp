#include "shunya/cad3d/mesh_decimator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <queue>
#include <utility>
#include <vector>

namespace shunya::cad3d {

namespace {

struct CollapseCandidate {
    double cost{0.0};
    uint32_t u{0};
    uint32_t v{0};
    uint32_t ver_u{0};
    uint32_t ver_v{0};
    Point3D target{};

    bool operator>(const CollapseCandidate& o) const noexcept {
        return cost > o.cost;
    }
};

struct DecTri {
    uint32_t v[3]{0, 0, 0};
    bool alive{true};
};

} // namespace

SolidMesh3D MeshDecimator::simplify(
    const SolidMesh3D& mesh,
    const DecimationParams& params,
    DecimationStats* out_stats
) {
    DecimationStats stats;
    stats.input_triangles = mesh.triangles.size();
    if (mesh.triangles.empty() || mesh.vertices.empty()) {
        if (out_stats) *out_stats = stats;
        return mesh;
    }

    const size_t target_tris = (params.target_triangle_count > 0)
        ? params.target_triangle_count
        : std::max<size_t>(4, static_cast<size_t>(std::llround(
              static_cast<double>(mesh.triangles.size()) * std::clamp(params.target_ratio, 0.01, 1.0))));

    if (mesh.triangles.size() <= target_tris) {
        stats.output_triangles = mesh.triangles.size();
        if (out_stats) *out_stats = stats;
        return mesh;
    }

    const size_t n_verts = mesh.vertices.size();
    std::vector<Point3D> verts = mesh.vertices;
    std::vector<bool> v_alive(n_verts, true);
    std::vector<uint32_t> v_version(n_verts, 0);
    std::vector<ErrorQuadric> quadrics(n_verts);
    std::vector<std::vector<uint32_t>> v_tris(n_verts);

    std::vector<DecTri> tris;
    tris.reserve(mesh.triangles.size());

    using UndirEdge = std::pair<uint32_t, uint32_t>;
    std::map<UndirEdge, uint32_t> edge_tri_count;
    std::map<UndirEdge, uint32_t> edge_first_tri;

    for (size_t ti = 0; ti < mesh.triangles.size(); ++ti) {
        const auto& t = mesh.triangles[ti];
        const uint32_t tid = static_cast<uint32_t>(tris.size());
        tris.push_back({{t[0], t[1], t[2]}, true});

        const ErrorQuadric q = ErrorQuadric::from_triangle(verts[t[0]], verts[t[1]], verts[t[2]]);
        for (int k = 0; k < 3; ++k) {
            const uint32_t vi = t[static_cast<size_t>(k)];
            quadrics[vi] += q;
            v_tris[vi].push_back(tid);

            const uint32_t vj = t[static_cast<size_t>((k + 1) % 3)];
            const UndirEdge ek = (vi < vj) ? UndirEdge{vi, vj} : UndirEdge{vj, vi};
            ++edge_tri_count[ek];
            if (edge_first_tri.find(ek) == edge_first_tri.end()) {
                edge_first_tri[ek] = tid;
            }
        }
    }

    // Add high-weight perpendicular boundary planes to preserve open silhouette edges
    if (params.boundary_weight > 0.0) {
        for (const auto& [ek, cnt] : edge_tri_count) {
            if (cnt != 1) continue;
            const auto& tri = tris[edge_first_tri[ek]];
            const Vec3d face_n = (verts[tri.v[1]] - verts[tri.v[0]])
                                     .cross(verts[tri.v[2]] - verts[tri.v[0]])
                                     .normalized();
            const Vec3d e_vec = verts[ek.second] - verts[ek.first];
            const double e_len = e_vec.length();
            if (e_len <= 1e-12 || face_n.length_squared() < 0.5) continue;
            const Vec3d bnd_n = e_vec.cross(face_n).normalized();
            const double d = -bnd_n.dot(verts[ek.first]);
            const ErrorQuadric bq = ErrorQuadric::from_plane(bnd_n, d, params.boundary_weight * e_len * e_len);
            quadrics[ek.first] += bq;
            quadrics[ek.second] += bq;
        }
    }

    std::priority_queue<CollapseCandidate, std::vector<CollapseCandidate>, std::greater<CollapseCandidate>> pq;
    auto push_edge = [&](uint32_t u, uint32_t v) {
        if (u >= v) std::swap(u, v);
        if (!v_alive[u] || !v_alive[v]) return;
        const ErrorQuadric q_sum = quadrics[u] + quadrics[v];
        const Point3D opt = q_sum.optimal_target(verts[u], verts[v]);
        const double err = q_sum.evaluate(opt);
        pq.push({err, u, v, v_version[u], v_version[v], opt});
    };

    for (const auto& [ek, cnt] : edge_tri_count) {
        (void)cnt;
        push_edge(ek.first, ek.second);
    }

    // Scratch buffers reused inside loop (avoid inner-loop heap churn)
    std::vector<uint32_t> nbr_u;
    std::vector<uint32_t> nbr_v;
    std::vector<uint32_t> shared_tris;

    auto gather_neighbors = [&](uint32_t vert, std::vector<uint32_t>& out_nbrs) {
        out_nbrs.clear();
        for (const uint32_t tid : v_tris[vert]) {
            if (!tris[tid].alive) continue;
            for (int k = 0; k < 3; ++k) {
                const uint32_t vk = tris[tid].v[k];
                if (vk != vert) out_nbrs.push_back(vk);
            }
        }
        std::sort(out_nbrs.begin(), out_nbrs.end());
        out_nbrs.erase(std::unique(out_nbrs.begin(), out_nbrs.end()), out_nbrs.end());
    };

    size_t alive_tris = tris.size();
    while (alive_tris > target_tris && !pq.empty()) {
        const CollapseCandidate top = pq.top();
        pq.pop();

        const uint32_t u = top.u;
        const uint32_t v = top.v;
        if (!v_alive[u] || !v_alive[v] ||
            v_version[u] != top.ver_u || v_version[v] != top.ver_v) {
            continue;
        }

        // Verify topological Link Condition so collapses never create non-manifold pinches
        shared_tris.clear();
        for (const uint32_t tid : v_tris[u]) {
            if (!tris[tid].alive) continue;
            const auto& tv = tris[tid].v;
            if (tv[0] == v || tv[1] == v || tv[2] == v) {
                shared_tris.push_back(tid);
            }
        }
        if (shared_tris.empty() || shared_tris.size() > 2) continue;

        gather_neighbors(u, nbr_u);
        gather_neighbors(v, nbr_v);
        size_t common_nbrs = 0;
        for (const uint32_t nu : nbr_u) {
            if (std::binary_search(nbr_v.begin(), nbr_v.end(), nu)) {
                ++common_nbrs;
            }
        }
        if (common_nbrs != shared_tris.size()) continue;

        // Check that no surviving triangle around u or v flips or degenerates
        bool invalid_flip = false;
        auto check_ring = [&](uint32_t vert) {
            for (const uint32_t tid : v_tris[vert]) {
                if (!tris[tid].alive) continue;
                const auto& tv = tris[tid].v;
                if ((tv[0] == u || tv[1] == u || tv[2] == u) &&
                    (tv[0] == v || tv[1] == v || tv[2] == v)) {
                    continue; // Will be removed
                }
                const Point3D& a0 = verts[tv[0]];
                const Point3D& a1 = verts[tv[1]];
                const Point3D& a2 = verts[tv[2]];
                const Vec3d old_n = (a1 - a0).cross(a2 - a0).normalized();

                const Point3D b0 = (tv[0] == vert) ? top.target : a0;
                const Point3D b1 = (tv[1] == vert) ? top.target : a1;
                const Point3D b2 = (tv[2] == vert) ? top.target : a2;
                const Vec3d new_cross = (b1 - b0).cross(b2 - b0);
                if (new_cross.length_squared() <= 1e-24) {
                    invalid_flip = true;
                    return;
                }
                const Vec3d new_n = new_cross.normalized();
                if (old_n.dot(new_n) <= params.min_normal_dot) {
                    invalid_flip = true;
                    return;
                }
            }
        };
        check_ring(u);
        if (!invalid_flip) check_ring(v);
        if (invalid_flip) continue;

        // Commit edge collapse (v -> u)
        verts[u] = top.target;
        quadrics[u] += quadrics[v];
        v_alive[v] = false;
        ++v_version[u];
        ++v_version[v];
        ++stats.collapses_performed;
        stats.max_quadric_error = std::max(stats.max_quadric_error, top.cost);

        for (const uint32_t stid : shared_tris) {
            if (tris[stid].alive) {
                tris[stid].alive = false;
                --alive_tris;
            }
        }

        // Redirect surviving triangles on v -> u
        for (const uint32_t tid : v_tris[v]) {
            if (!tris[tid].alive) continue;
            for (int k = 0; k < 3; ++k) {
                if (tris[tid].v[k] == v) tris[tid].v[k] = u;
            }
            v_tris[u].push_back(tid);
        }
        v_tris[v].clear();

        // Compact v_tris[u]
        std::sort(v_tris[u].begin(), v_tris[u].end());
        v_tris[u].erase(
            std::remove_if(v_tris[u].begin(), v_tris[u].end(), [&](uint32_t tid) { return !tris[tid].alive; }),
            v_tris[u].end()
        );
        v_tris[u].erase(std::unique(v_tris[u].begin(), v_tris[u].end()), v_tris[u].end());

        // Re-evaluate adjacent edges around u
        gather_neighbors(u, nbr_u);
        for (const uint32_t nu : nbr_u) {
            push_edge(u, nu);
        }
    }

    // Re-pack dense vertices & triangles
    SolidMesh3D out;
    std::vector<uint32_t> vert_remap(n_verts, UINT32_MAX);
    out.triangles.reserve(alive_tris);

    for (const auto& t : tris) {
        if (!t.alive) continue;
        std::array<uint32_t, 3> tri_out{};
        for (int k = 0; k < 3; ++k) {
            const uint32_t ov = t.v[static_cast<size_t>(k)];
            if (vert_remap[ov] == UINT32_MAX) {
                vert_remap[ov] = out.add_vertex(verts[ov]);
            }
            tri_out[static_cast<size_t>(k)] = vert_remap[ov];
        }
        out.add_triangle(tri_out[0], tri_out[1], tri_out[2]);
    }

    stats.output_triangles = out.triangles.size();
    if (out_stats) *out_stats = stats;
    return out;
}

} // namespace shunya::cad3d
