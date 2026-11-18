#include "shunya/rig3d/organic_mesh.hpp"

#include <cmath>
#include <map>
#include <tuple>
#include <vector>

namespace shunya::rig3d {

// =============================================================================
// QuadSphereBuilder Implementation (PH4-11.02.001)
// =============================================================================
SolidMesh3D QuadSphereBuilder::build() {
    return build(Params{});
}

SolidMesh3D QuadSphereBuilder::build(const Params& params) {
    SolidMesh3D mesh;
    const uint32_t n = std::max(1u, std::min(params.subdivisions, 32u));
    const double r = std::max(1e-6, params.radius);

    struct FaceDef {
        Vec3d origin;
        Vec3d u_dir;
        Vec3d v_dir;
    };

    const std::vector<FaceDef> faces = {
        // +Z face
        {Vec3d(-1, -1,  1), Vec3d( 2,  0,  0), Vec3d( 0,  2,  0)},
        // -Z face
        {Vec3d( 1, -1, -1), Vec3d(-2,  0,  0), Vec3d( 0,  2,  0)},
        // +X face
        {Vec3d( 1, -1,  1), Vec3d( 0,  2,  0), Vec3d( 0,  0, -2)},
        // -X face
        {Vec3d(-1, -1, -1), Vec3d( 0,  2,  0), Vec3d( 0,  0,  2)},
        // +Y face
        {Vec3d(-1,  1,  1), Vec3d( 2,  0,  0), Vec3d( 0,  0, -2)},
        // -Y face
        {Vec3d(-1, -1, -1), Vec3d( 2,  0,  0), Vec3d( 0,  0,  2)}
    };

    std::map<std::tuple<int64_t, int64_t, int64_t>, uint32_t> vertex_map;
    constexpr double quant = 1e6;

    auto get_or_add_vertex = [&](const Point3D& p) -> uint32_t {
        const int64_t qx = static_cast<int64_t>(std::round(p.x * quant));
        const int64_t qy = static_cast<int64_t>(std::round(p.y * quant));
        const int64_t qz = static_cast<int64_t>(std::round(p.z * quant));
        const auto key = std::make_tuple(qx, qy, qz);

        auto it = vertex_map.find(key);
        if (it != vertex_map.end()) {
            return it->second;
        }

        const uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(p);
        Vec3d norm(p.x - params.center.x, p.y - params.center.y, p.z - params.center.z);
        mesh.normals.push_back(norm.normalized());
        vertex_map[key] = idx;
        return idx;
    };

    for (const auto& f : faces) {
        for (uint32_t j = 0; j < n; ++j) {
            for (uint32_t i = 0; i < n; ++i) {
                const double u0 = static_cast<double>(i) / static_cast<double>(n);
                const double u1 = static_cast<double>(i + 1) / static_cast<double>(n);
                const double v0 = static_cast<double>(j) / static_cast<double>(n);
                const double v1 = static_cast<double>(j + 1) / static_cast<double>(n);

                auto cube_pt = [&](double u, double v) -> Point3D {
                    Vec3d p = f.origin + f.u_dir * u + f.v_dir * v;
                    const double len = p.length();
                    const double s = (len > 1e-12) ? (r / len) : 1.0;
                    return Point3D(
                        params.center.x + p.x * s,
                        params.center.y + p.y * s,
                        params.center.z + p.z * s
                    );
                };

                const uint32_t idx00 = get_or_add_vertex(cube_pt(u0, v0));
                const uint32_t idx10 = get_or_add_vertex(cube_pt(u1, v0));
                const uint32_t idx11 = get_or_add_vertex(cube_pt(u1, v1));
                const uint32_t idx01 = get_or_add_vertex(cube_pt(u0, v1));

                // Two triangles for the quad cell
                mesh.triangles.push_back({idx00, idx10, idx11});
                mesh.triangles.push_back({idx00, idx11, idx01});
            }
        }
    }

    return mesh;
}

// =============================================================================
// IcosphereBuilder Implementation (PH4-11.02.001)
// =============================================================================
SolidMesh3D IcosphereBuilder::build() {
    return build(Params{});
}

SolidMesh3D IcosphereBuilder::build(const Params& params) {
    SolidMesh3D mesh;
    const double r = std::max(1e-6, params.radius);
    const double phi = (1.0 + std::sqrt(5.0)) * 0.5;

    const std::vector<Vec3d> base_pts = {
        Vec3d(-1,  phi,  0), Vec3d( 1,  phi,  0), Vec3d(-1, -phi,  0), Vec3d( 1, -phi,  0),
        Vec3d( 0, -1,  phi), Vec3d( 0,  1,  phi), Vec3d( 0, -1, -phi), Vec3d( 0,  1, -phi),
        Vec3d( phi,  0, -1), Vec3d( phi,  0,  1), Vec3d(-phi,  0, -1), Vec3d(-phi,  0,  1)
    };

    for (const auto& bp : base_pts) {
        const double len = bp.length();
        const double s = r / len;
        mesh.vertices.push_back(Point3D(
            params.center.x + bp.x * s,
            params.center.y + bp.y * s,
            params.center.z + bp.z * s
        ));
        mesh.normals.push_back(bp / len);
    }

    std::vector<std::array<uint32_t, 3>> current_triangles = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };

    const uint32_t levels = std::min(params.resolution_level, 6u);
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpoint_cache;

    auto get_midpoint = [&](uint32_t i1, uint32_t i2) -> uint32_t {
        const uint32_t u = std::min(i1, i2);
        const uint32_t w = std::max(i1, i2);
        const auto key = std::make_pair(u, w);

        auto it = midpoint_cache.find(key);
        if (it != midpoint_cache.end()) {
            return it->second;
        }

        const Point3D& p1 = mesh.vertices[u];
        const Point3D& p2 = mesh.vertices[w];
        Vec3d mid(
            ((p1.x + p2.x) * 0.5) - params.center.x,
            ((p1.y + p2.y) * 0.5) - params.center.y,
            ((p1.z + p2.z) * 0.5) - params.center.z
        );
        const double len = mid.length();
        const double s = (len > 1e-12) ? (r / len) : 1.0;

        const uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(Point3D(
            params.center.x + mid.x * s,
            params.center.y + mid.y * s,
            params.center.z + mid.z * s
        ));
        mesh.normals.push_back(mid / len);

        midpoint_cache[key] = idx;
        return idx;
    };

    for (uint32_t lvl = 0; lvl < levels; ++lvl) {
        std::vector<std::array<uint32_t, 3>> new_tris;
        new_tris.reserve(current_triangles.size() * 4);

        for (const auto& tri : current_triangles) {
            const uint32_t a = get_midpoint(tri[0], tri[1]);
            const uint32_t b = get_midpoint(tri[1], tri[2]);
            const uint32_t c = get_midpoint(tri[2], tri[0]);

            new_tris.push_back({tri[0], a, c});
            new_tris.push_back({tri[1], b, a});
            new_tris.push_back({tri[2], c, b});
            new_tris.push_back({a, b, c});
        }

        current_triangles = std::move(new_tris);
    }

    mesh.triangles = std::move(current_triangles);
    return mesh;
}

} // namespace shunya::rig3d
