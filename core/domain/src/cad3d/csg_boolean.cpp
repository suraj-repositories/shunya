#include "shunya/cad3d/csg_boolean.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace shunya::cad3d {

SolidMesh3D CsgBooleanEngine::execute(
    CsgOp op,
    const SolidMesh3D& solid_a,
    const SolidMesh3D& solid_b
) {
    const auto polys_a = BspNode::mesh_to_polygons(solid_a);
    const auto polys_b = BspNode::mesh_to_polygons(solid_b);
    if (polys_a.empty()) {
        return (op == CsgOp::Union) ? solid_b : SolidMesh3D{};
    }
    if (polys_b.empty()) {
        return (op == CsgOp::Intersect) ? SolidMesh3D{} : solid_a;
    }

    BspNode a(polys_a);
    BspNode b(polys_b);

    switch (op) {
        case CsgOp::Union: {
            a.clip_to(b);
            b.clip_to(a);
            b.invert();
            b.clip_to(a);
            b.invert();
            a.build(b.all_polygons());
            break;
        }
        case CsgOp::Subtract: {
            a.invert();
            a.clip_to(b);
            b.clip_to(a);
            b.invert();
            b.clip_to(a);
            b.invert();
            a.build(b.all_polygons());
            a.invert();
            break;
        }
        case CsgOp::Intersect: {
            a.invert();
            b.clip_to(a);
            b.invert();
            a.clip_to(b);
            b.clip_to(a);
            a.build(b.all_polygons());
            a.invert();
            break;
        }
    }

    SolidMesh3D result = BspNode::polygons_to_mesh(a.all_polygons());
    repair_tjunctions(result, 1e-6);
    result.ensure_outward_winding();
    return result;
}

void CsgBooleanEngine::repair_tjunctions(SolidMesh3D& mesh, double tol) {
    mesh.weld_vertices(tol * 0.5);
    const double tol_sq = tol * tol;

    for (int pass = 0; pass < 16; ++pass) {
        using UndirEdge = std::pair<uint32_t, uint32_t>;
        std::map<UndirEdge, int> edge_counts;
        for (const auto& t : mesh.triangles) {
            for (int e = 0; e < 3; ++e) {
                const uint32_t u = t[static_cast<size_t>(e)];
                const uint32_t v = t[static_cast<size_t>((e + 1) % 3)];
                ++edge_counts[(u < v) ? UndirEdge{u, v} : UndirEdge{v, u}];
            }
        }

        // Collect all unique vertices participating in open boundary edges (possible T-junctions)
        std::vector<uint32_t> boundary_verts;
        for (const auto& [key, cnt] : edge_counts) {
            if (cnt == 1) {
                boundary_verts.push_back(key.first);
                boundary_verts.push_back(key.second);
            }
        }
        if (boundary_verts.empty()) break;
        std::sort(boundary_verts.begin(), boundary_verts.end());
        boundary_verts.erase(std::unique(boundary_verts.begin(), boundary_verts.end()), boundary_verts.end());

        bool split_any = false;
        std::vector<std::array<uint32_t, 3>> next_tris;
        next_tris.reserve(mesh.triangles.size() + 32);

        for (const auto& t : mesh.triangles) {
            bool split_this = false;
            for (int e = 0; e < 3 && !split_this; ++e) {
                const uint32_t i0 = t[static_cast<size_t>(e)];
                const uint32_t i1 = t[static_cast<size_t>((e + 1) % 3)];
                const uint32_t i_opp = t[static_cast<size_t>((e + 2) % 3)];
                const UndirEdge ek = (i0 < i1) ? UndirEdge{i0, i1} : UndirEdge{i1, i0};
                if (edge_counts[ek] != 1) continue;

                const Point3D& a = mesh.vertices[i0];
                const Point3D& b = mesh.vertices[i1];
                const Vec3d ab = b - a;
                const double ab_sq = ab.length_squared();
                if (ab_sq <= tol_sq) continue;

                double best_t = 0.0;
                uint32_t best_v = 0;
                double best_d2 = tol_sq;
                for (const uint32_t v : boundary_verts) {
                    if (v == i0 || v == i1 || v == i_opp) continue;
                    const Vec3d av = mesh.vertices[v] - a;
                    const double s = av.dot(ab) / ab_sq;
                    if (s <= 1e-6 || s >= 1.0 - 1e-6) continue;
                    const double d2 = (mesh.vertices[v] - (a + ab * s)).length_squared();
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        best_v = v;
                        best_t = s;
                    }
                }
                (void)best_t;
                if (best_d2 < tol_sq) {
                    next_tris.push_back({i0, best_v, i_opp});
                    next_tris.push_back({best_v, i1, i_opp});
                    split_this = true;
                    split_any = true;
                }
            }
            if (!split_this) {
                next_tris.push_back(t);
            }
        }

        if (!split_any) break;
        mesh.triangles = std::move(next_tris);
    }
}

} // namespace shunya::cad3d
