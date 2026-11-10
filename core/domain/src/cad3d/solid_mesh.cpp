#include "shunya/cad3d/solid_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>

namespace shunya::cad3d {

bool BBox3D::is_valid() const noexcept {
    return std::isfinite(min.x) && std::isfinite(min.y) && std::isfinite(min.z) &&
           std::isfinite(max.x) && std::isfinite(max.y) && std::isfinite(max.z) &&
           min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

void BBox3D::expand(const Point3D& pt) noexcept {
    min.x = std::min(min.x, pt.x);
    min.y = std::min(min.y, pt.y);
    min.z = std::min(min.z, pt.z);
    max.x = std::max(max.x, pt.x);
    max.y = std::max(max.y, pt.y);
    max.z = std::max(max.z, pt.z);
}

void BBox3D::expand(const BBox3D& other) noexcept {
    if (!other.is_valid()) return;
    expand(other.min);
    expand(other.max);
}

Vec3d BBox3D::size() const noexcept {
    if (!is_valid()) return {0.0, 0.0, 0.0};
    return max - min;
}

uint32_t SolidMesh3D::add_vertex(const Point3D& pt) {
    vertices.push_back(pt);
    return static_cast<uint32_t>(vertices.size() - 1);
}

void SolidMesh3D::add_triangle(uint32_t i0, uint32_t i1, uint32_t i2) {
    if (i0 == i1 || i1 == i2 || i2 == i0) return;
    triangles.push_back({i0, i1, i2});
}

void SolidMesh3D::clear() noexcept {
    vertices.clear();
    triangles.clear();
}

void SolidMesh3D::weld_vertices(double tol) {
    if (vertices.empty()) return;
    const double inv_tol = (tol > 0.0) ? (1.0 / tol) : 1e9;
    struct GridKey {
        int64_t x, y, z;
        bool operator==(const GridKey& o) const noexcept {
            return x == o.x && y == o.y && z == o.z;
        }
    };
    struct GridHash {
        size_t operator()(const GridKey& k) const noexcept {
            size_t h = static_cast<size_t>(k.x) * 73856093ULL;
            h ^= static_cast<size_t>(k.y) * 19349663ULL;
            h ^= static_cast<size_t>(k.z) * 83492791ULL;
            return h;
        }
    };

    std::unordered_map<GridKey, uint32_t, GridHash> lookup;
    std::vector<Point3D> new_verts;
    std::vector<uint32_t> remap(vertices.size(), 0);
    new_verts.reserve(vertices.size());

    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& p = vertices[i];
        const GridKey key{
            static_cast<int64_t>(std::llround(p.x * inv_tol)),
            static_cast<int64_t>(std::llround(p.y * inv_tol)),
            static_cast<int64_t>(std::llround(p.z * inv_tol))
        };
        auto it = lookup.find(key);
        if (it != lookup.end()) {
            remap[i] = it->second;
        } else {
            const uint32_t nid = static_cast<uint32_t>(new_verts.size());
            new_verts.push_back(p);
            lookup.emplace(key, nid);
            remap[i] = nid;
        }
    }

    std::vector<std::array<uint32_t, 3>> clean_tris;
    clean_tris.reserve(triangles.size());
    for (const auto& t : triangles) {
        const uint32_t a = remap[t[0]];
        const uint32_t b = remap[t[1]];
        const uint32_t c = remap[t[2]];
        if (a != b && b != c && c != a) {
            const Vec3d ab = new_verts[b] - new_verts[a];
            const Vec3d ac = new_verts[c] - new_verts[a];
            if (ab.cross(ac).length_squared() > 1e-24) {
                clean_tris.push_back({a, b, c});
            }
        }
    }

    vertices = std::move(new_verts);
    triangles = std::move(clean_tris);
}

void SolidMesh3D::ensure_outward_winding() {
    if (compute_signed_volume() < 0.0) {
        for (auto& t : triangles) {
            std::swap(t[1], t[2]);
        }
    }
}

double SolidMesh3D::compute_signed_volume() const noexcept {
    double vol6 = 0.0;
    for (const auto& t : triangles) {
        const Point3D& a = vertices[t[0]];
        const Point3D& b = vertices[t[1]];
        const Point3D& c = vertices[t[2]];
        vol6 += a.dot(b.cross(c));
    }
    return vol6 / 6.0;
}

double SolidMesh3D::compute_volume() const noexcept {
    return std::abs(compute_signed_volume());
}

double SolidMesh3D::compute_surface_area() const noexcept {
    double area2 = 0.0;
    for (const auto& t : triangles) {
        const Vec3d ab = vertices[t[1]] - vertices[t[0]];
        const Vec3d ac = vertices[t[2]] - vertices[t[0]];
        area2 += ab.cross(ac).length();
    }
    return area2 * 0.5;
}

BBox3D SolidMesh3D::compute_bbox() const noexcept {
    BBox3D box;
    for (const auto& v : vertices) {
        box.expand(v);
    }
    return box;
}

Vec3d SolidMesh3D::triangle_normal(size_t tri_idx) const noexcept {
    const auto& t = triangles[tri_idx];
    const Vec3d ab = vertices[t[1]] - vertices[t[0]];
    const Vec3d ac = vertices[t[2]] - vertices[t[0]];
    return ab.cross(ac).normalized();
}

ManifoldReport SolidMesh3D::analyze_manifold() const {
    ManifoldReport rep;
    rep.vertex_count = vertices.size();
    rep.face_count = triangles.size();

    using UndirEdge = std::pair<uint32_t, uint32_t>;
    struct EdgeStats {
        int forward{0};
        int reverse{0};
        int total{0};
    };

    std::map<UndirEdge, EdgeStats> edges;
    for (const auto& t : triangles) {
        for (int e = 0; e < 3; ++e) {
            const uint32_t u = t[static_cast<size_t>(e)];
            const uint32_t v = t[static_cast<size_t>((e + 1) % 3)];
            const UndirEdge key = (u < v) ? UndirEdge{u, v} : UndirEdge{v, u};
            auto& st = edges[key];
            ++st.total;
            if (u < v) ++st.forward;
            else ++st.reverse;
        }
    }

    rep.edge_count = edges.size();
    for (const auto& [key, st] : edges) {
        (void)key;
        if (st.total == 1) {
            ++rep.boundary_edge_count;
        } else if (st.total != 2 || st.forward != 1 || st.reverse != 1) {
            ++rep.non_manifold_edge_count;
        }
    }

    rep.euler_characteristic =
        static_cast<int64_t>(rep.vertex_count) -
        static_cast<int64_t>(rep.edge_count) +
        static_cast<int64_t>(rep.face_count);
    rep.is_watertight = !triangles.empty() && rep.boundary_edge_count == 0;
    rep.is_2_manifold = rep.is_watertight && rep.non_manifold_edge_count == 0;
    return rep;
}

} // namespace shunya::cad3d
