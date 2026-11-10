#include "shunya/cad3d/tetrahedral_mesher.hpp"

#include "shunya/cad3d/bvh_tree.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace shunya::cad3d {

namespace {

struct TetElem {
    uint32_t v[4]{0, 0, 0, 0};
    bool alive{true};
    Point3D circumcenter{};
    double circumradius_sq{0.0};
};

bool compute_circumsphere(
    const Point3D& a,
    const Point3D& b,
    const Point3D& c,
    const Point3D& d,
    Point3D& out_center,
    double& out_r2
) noexcept {
    const Vec3d u = b - a;
    const Vec3d v = c - a;
    const Vec3d w = d - a;
    const Vec3d vw = v.cross(w);
    const Vec3d wu = w.cross(u);
    const Vec3d uv = u.cross(v);
    const double det2 = 2.0 * u.dot(vw);
    if (std::abs(det2) <= 1e-18) return false;

    const Vec3d offset = (vw * u.length_squared() + wu * v.length_squared() + uv * w.length_squared()) * (1.0 / det2);
    out_center = a + offset;
    out_r2 = offset.length_squared();
    return true;
}

using TriFaceKey = std::array<uint32_t, 3>;

TriFaceKey sorted_face_key(uint32_t a, uint32_t b, uint32_t c) noexcept {
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return {a, b, c};
}

} // namespace

TetraMesh3D TetrahedralMesher::generate(
    const SolidMesh3D& boundary_mesh,
    const TetMeshParams& params
) {
    TetraMesh3D out;
    if (boundary_mesh.vertices.empty() || boundary_mesh.triangles.empty()) return out;

    SolidMesh3D clean = boundary_mesh;
    clean.weld_vertices(1e-7);
    clean.ensure_outward_winding();

    LinearBvh3D bvh;
    bvh.build(clean, 4);

    const BBox3D bbox = clean.compute_bbox();
    const Vec3d size = bbox.size();
    const double diag = size.length();
    if (diag <= 1e-12) return out;

    // Collect boundary vertices + evenly spaced interior lattice points strictly inside boundary
    std::vector<Point3D> pts = clean.vertices;
    const double spacing = (params.interior_spacing > 1e-9) ? params.interior_spacing : (diag / 5.0);
    if (params.generate_interior_points && spacing > 1e-9) {
        const double margin = spacing * 0.35;
        for (double x = bbox.min.x + margin; x <= bbox.max.x - margin; x += spacing) {
            for (double y = bbox.min.y + margin; y <= bbox.max.y - margin; y += spacing) {
                for (double z = bbox.min.z + margin; z <= bbox.max.z - margin; z += spacing) {
                    const Point3D p{x, y, z};
                    if (bvh.contains_point(p)) {
                        // Ensure minimum separation from existing boundary vertices
                        bool too_close = false;
                        const double min_sep_sq = (spacing * 0.25) * (spacing * 0.25);
                        for (const auto& bv : clean.vertices) {
                            if ((bv - p).length_squared() < min_sep_sq) {
                                too_close = true;
                                break;
                            }
                        }
                        if (!too_close) pts.push_back(p);
                    }
                }
            }
        }
    }

    // 1. Initialize perturbed points for cospherical-free Bowyer-Watson + super-tetrahedron
    const uint32_t n_real = static_cast<uint32_t>(pts.size());
    std::vector<Point3D> del_pts = pts;
    const double jitter = diag * 1e-8;
    for (uint32_t i = 0; i < n_real; ++i) {
        const double h1 = std::sin(static_cast<double>(i + 1) * 12.9898);
        const double h2 = std::cos(static_cast<double>(i + 1) * 78.233);
        const double h3 = std::sin(static_cast<double>(i + 1) * 43.1234);
        del_pts[i] += Vec3d{h1 * jitter, h2 * jitter, h3 * jitter};
    }

    const Point3D c = (bbox.min + bbox.max) * 0.5;
    const double R = diag * 10.0;
    del_pts.push_back(c + Vec3d{ 0.0,      0.0,      3.0 * R});
    del_pts.push_back(c + Vec3d{ 0.0,      2.828*R, -1.0 * R});
    del_pts.push_back(c + Vec3d{-2.449*R, -1.414*R, -1.0 * R});
    del_pts.push_back(c + Vec3d{ 2.449*R, -1.414*R, -1.0 * R});

    std::vector<TetElem> tets;
    {
        TetElem st;
        st.v[0] = n_real;
        st.v[1] = n_real + 1;
        st.v[2] = n_real + 2;
        st.v[3] = n_real + 3;
        if (TetQualityAnalyzer::signed_jacobian(del_pts[st.v[0]], del_pts[st.v[1]], del_pts[st.v[2]], del_pts[st.v[3]]) < 0.0) {
            std::swap(st.v[2], st.v[3]);
        }
        compute_circumsphere(del_pts[st.v[0]], del_pts[st.v[1]], del_pts[st.v[2]], del_pts[st.v[3]], st.circumcenter, st.circumradius_sq);
        tets.push_back(st);
    }

    // 2. Incremental 3D Bowyer-Watson Delaunay insertion
    for (uint32_t pid = 0; pid < n_real; ++pid) {
        const Point3D& P = del_pts[pid];

        struct RawFace {
            uint32_t v[3];
        };
        std::map<TriFaceKey, std::pair<int, RawFace>> cavity_faces;

        for (auto& tet : tets) {
            if (!tet.alive) continue;
            const double d2 = (P - tet.circumcenter).length_squared();
            if (d2 <= tet.circumradius_sq * (1.0 + 1e-11)) {
                tet.alive = false;
                const RawFace f4[4] = {
                    {{tet.v[0], tet.v[2], tet.v[1]}},
                    {{tet.v[0], tet.v[1], tet.v[3]}},
                    {{tet.v[1], tet.v[2], tet.v[3]}},
                    {{tet.v[2], tet.v[0], tet.v[3]}}
                };
                for (const auto& f : f4) {
                    const TriFaceKey key = sorted_face_key(f.v[0], f.v[1], f.v[2]);
                    auto it = cavity_faces.find(key);
                    if (it == cavity_faces.end()) {
                        cavity_faces.emplace(key, std::make_pair(1, f));
                    } else {
                        ++it->second.first;
                    }
                }
            }
        }

        for (const auto& [key, entry] : cavity_faces) {
            (void)key;
            if (entry.first != 1) continue; // Interior cavity face deleted by 2 bad tets
            const auto& f = entry.second.v;
            TetElem nt;
            nt.v[0] = f[0];
            nt.v[1] = f[1];
            nt.v[2] = f[2];
            nt.v[3] = pid;
            const double jac = TetQualityAnalyzer::signed_jacobian(del_pts[nt.v[0]], del_pts[nt.v[1]], del_pts[nt.v[2]], del_pts[nt.v[3]]);
            if (std::abs(jac) <= 1e-16) continue;
            if (jac < 0.0) std::swap(nt.v[1], nt.v[2]);
            if (compute_circumsphere(del_pts[nt.v[0]], del_pts[nt.v[1]], del_pts[nt.v[2]], del_pts[nt.v[3]], nt.circumcenter, nt.circumradius_sq)) {
                tets.push_back(nt);
            }
        }
    }

    // 3. Remove super-tetrahedron elements and keep non-degenerate elements strictly inside boundary via BVH
    out.vertices = std::move(pts);
    for (const auto& tet : tets) {
        if (!tet.alive) continue;
        if (tet.v[0] >= n_real || tet.v[1] >= n_real || tet.v[2] >= n_real || tet.v[3] >= n_real) {
            continue;
        }
        auto elem = std::array<uint32_t, 4>{tet.v[0], tet.v[1], tet.v[2], tet.v[3]};
        double real_jac = TetQualityAnalyzer::signed_jacobian(
            out.vertices[elem[0]], out.vertices[elem[1]],
            out.vertices[elem[2]], out.vertices[elem[3]]
        );
        if (std::abs(real_jac) <= 1e-9) continue; // Drop zero-volume boundary sliver
        const Point3D centroid = (out.vertices[elem[0]] + out.vertices[elem[1]] +
                                  out.vertices[elem[2]] + out.vertices[elem[3]]) * 0.25;
        if (!bvh.contains_point(centroid)) continue;

        if (real_jac < 0.0) std::swap(elem[2], elem[3]);
        out.tetrahedra.push_back(elem);
    }

    return out;
}

} // namespace shunya::cad3d
