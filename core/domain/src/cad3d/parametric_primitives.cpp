#include "shunya/cad3d/parametric_primitives.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace shunya::cad3d {

namespace {

constexpr double PI = std::numbers::pi;
constexpr double TWO_PI = 2.0 * std::numbers::pi;

} // namespace

// ============================================================================
// Parametric Primitive Generator Implementations
// ============================================================================

SolidMesh3D ParametricPrimitiveGenerator::generate_cube(const CubeParams& p) {
    SolidMesh3D mesh;
    const double hx = p.width * 0.5;
    const double hy = p.height * 0.5;
    const double hz = p.depth * 0.5;

    const uint32_t sx = std::max(1u, p.subdivisions_x);
    const uint32_t sy = std::max(1u, p.subdivisions_y);
    const uint32_t sz = std::max(1u, p.subdivisions_z);

    auto add_face = [&](const Point3D& origin, const Vec3d& u_vec, const Vec3d& v_vec,
                        const Vec3d& normal, uint32_t u_segs, uint32_t v_segs) {
        uint32_t start_idx = static_cast<uint32_t>(mesh.vertices.size());
        for (uint32_t j = 0; j <= v_segs; ++j) {
            double v = static_cast<double>(j) / v_segs;
            for (uint32_t i = 0; i <= u_segs; ++i) {
                double u = static_cast<double>(i) / u_segs;
                Point3D pos = origin + u_vec * u + v_vec * v;
                mesh.vertices.push_back(pos);
                mesh.normals.push_back(normal);
                mesh.uvs.push_back(Vec2d{u, v});
            }
        }

        uint32_t row_stride = u_segs + 1;
        for (uint32_t j = 0; j < v_segs; ++j) {
            for (uint32_t i = 0; i < u_segs; ++i) {
                uint32_t i0 = start_idx + j * row_stride + i;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + row_stride;
                uint32_t i3 = i2 + 1;
                mesh.add_triangle(i0, i1, i3);
                mesh.add_triangle(i0, i3, i2);
            }
        }
    };

    // Front (+Z)
    add_face(Point3D{-hx, -hy, hz}, Vec3d{2.0 * hx, 0.0, 0.0}, Vec3d{0.0, 2.0 * hy, 0.0}, Vec3d{0.0, 0.0, 1.0}, sx, sy);
    // Back (-Z)
    add_face(Point3D{hx, -hy, -hz}, Vec3d{-2.0 * hx, 0.0, 0.0}, Vec3d{0.0, 2.0 * hy, 0.0}, Vec3d{0.0, 0.0, -1.0}, sx, sy);
    // Right (+X)
    add_face(Point3D{hx, -hy, hz}, Vec3d{0.0, 0.0, -2.0 * hz}, Vec3d{0.0, 2.0 * hy, 0.0}, Vec3d{1.0, 0.0, 0.0}, sz, sy);
    // Left (-X)
    add_face(Point3D{-hx, -hy, -hz}, Vec3d{0.0, 0.0, 2.0 * hz}, Vec3d{0.0, 2.0 * hy, 0.0}, Vec3d{-1.0, 0.0, 0.0}, sz, sy);
    // Top (+Y)
    add_face(Point3D{-hx, hy, hz}, Vec3d{2.0 * hx, 0.0, 0.0}, Vec3d{0.0, 0.0, -2.0 * hz}, Vec3d{0.0, 1.0, 0.0}, sx, sz);
    // Bottom (-Y)
    add_face(Point3D{-hx, -hy, -hz}, Vec3d{2.0 * hx, 0.0, 0.0}, Vec3d{0.0, 0.0, 2.0 * hz}, Vec3d{0.0, -1.0, 0.0}, sx, sz);

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_sphere(const SphereParams& p) {
    UvSphereParams uv_p;
    uv_p.radius = p.radius;
    uv_p.segments = std::max(4u, p.subdivisions_u);
    uv_p.rings = std::max(3u, p.subdivisions_v);
    return generate_uv_sphere(uv_p);
}

SolidMesh3D ParametricPrimitiveGenerator::generate_uv_sphere(const UvSphereParams& p) {
    SolidMesh3D mesh;
    const uint32_t segs = std::max(4u, p.segments);
    const uint32_t rings = std::max(3u, p.rings);
    const double r = std::max(1e-6, p.radius);

    for (uint32_t lat = 0; lat <= rings; ++lat) {
        double v = static_cast<double>(lat) / rings;
        double theta = v * PI; // 0 (north pole) to PI (south pole)
        double sin_theta = std::sin(theta);
        double cos_theta = std::cos(theta);

        for (uint32_t lon = 0; lon <= segs; ++lon) {
            double u = static_cast<double>(lon) / segs;
            double phi = u * TWO_PI;
            double sin_phi = std::sin(phi);
            double cos_phi = std::cos(phi);

            Vec3d norm{sin_theta * cos_phi, sin_theta * sin_phi, cos_theta};
            Point3D pos = norm * r;

            mesh.vertices.push_back(pos);
            mesh.normals.push_back(norm);
            mesh.uvs.push_back(Vec2d{u, 1.0 - v});
        }
    }

    uint32_t row_stride = segs + 1;
    for (uint32_t lat = 0; lat < rings; ++lat) {
        for (uint32_t lon = 0; lon < segs; ++lon) {
            uint32_t i0 = lat * row_stride + lon;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (lat + 1) * row_stride + lon;
            uint32_t i3 = i2 + 1;

            if (lat == 0) {
                // North pole cap triangle
                mesh.add_triangle(i0, i2, i3);
            } else if (lat == rings - 1) {
                // South pole cap triangle
                mesh.add_triangle(i0, i2, i1);
            } else {
                // Quad
                mesh.add_triangle(i0, i2, i1);
                mesh.add_triangle(i1, i2, i3);
            }
        }
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_icosphere(const IcosphereParams& p) {
    SolidMesh3D mesh;
    const double r = std::max(1e-6, p.radius);
    const double phi = (1.0 + std::sqrt(5.0)) * 0.5;

    // 12 base vertices of icosahedron
    std::vector<Point3D> base_verts = {
        {-1.0,  phi, 0.0}, { 1.0,  phi, 0.0}, {-1.0, -phi, 0.0}, { 1.0, -phi, 0.0},
        {0.0, -1.0,  phi}, {0.0,  1.0,  phi}, {0.0, -1.0, -phi}, {0.0,  1.0, -phi},
        { phi, 0.0, -1.0}, { phi, 0.0,  1.0}, {-phi, 0.0, -1.0}, {-phi, 0.0,  1.0}
    };

    for (const auto& v : base_verts) {
        Vec3d n = v.normalized();
        mesh.vertices.push_back(n * r);
        mesh.normals.push_back(n);
        double u = 0.5 + std::atan2(n.y, n.x) / TWO_PI;
        double uv_v = 0.5 - std::asin(std::clamp(n.z, -1.0, 1.0)) / PI;
        mesh.uvs.push_back(Vec2d{u, uv_v});
    }

    // 20 base faces
    std::vector<std::array<uint32_t, 3>> faces = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };

    // Recursive edge subdivision with midpoint vertex deduplication
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpoint_cache;

    auto get_midpoint = [&](uint32_t i1, uint32_t i2) -> uint32_t {
        if (i1 > i2) std::swap(i1, i2);
        auto key = std::make_pair(i1, i2);
        auto it = midpoint_cache.find(key);
        if (it != midpoint_cache.end()) {
            return it->second;
        }

        Point3D mid = (mesh.vertices[i1] + mesh.vertices[i2]) * 0.5;
        Vec3d norm = mid.normalized();
        Point3D pos = norm * r;

        uint32_t new_idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(pos);
        mesh.normals.push_back(norm);
        double u = 0.5 + std::atan2(norm.y, norm.x) / TWO_PI;
        double uv_v = 0.5 - std::asin(std::clamp(norm.z, -1.0, 1.0)) / PI;
        mesh.uvs.push_back(Vec2d{u, uv_v});

        midpoint_cache[key] = new_idx;
        return new_idx;
    };

    uint32_t subs = std::min(5u, p.subdivisions);
    for (uint32_t sub = 0; sub < subs; ++sub) {
        std::vector<std::array<uint32_t, 3>> next_faces;
        next_faces.reserve(faces.size() * 4);
        for (const auto& tri : faces) {
            uint32_t a = get_midpoint(tri[0], tri[1]);
            uint32_t b = get_midpoint(tri[1], tri[2]);
            uint32_t c = get_midpoint(tri[2], tri[0]);

            next_faces.push_back({tri[0], a, c});
            next_faces.push_back({tri[1], b, a});
            next_faces.push_back({tri[2], c, b});
            next_faces.push_back({a, b, c});
        }
        faces = std::move(next_faces);
    }

    for (const auto& tri : faces) {
        mesh.add_triangle(tri[0], tri[1], tri[2]);
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_cylinder(const CylinderParams& p) {
    SolidMesh3D mesh;
    const uint32_t radial_segs = std::max(3u, p.radial_segments);
    const uint32_t height_segs = std::max(1u, p.height_segments);
    const double r = std::max(1e-6, p.radius);
    const double h = std::max(1e-6, p.height);
    const double half_h = h * 0.5;

    // Side vertices
    for (uint32_t j = 0; j <= height_segs; ++j) {
        double v = static_cast<double>(j) / height_segs;
        double z = -half_h + v * h;
        for (uint32_t i = 0; i <= radial_segs; ++i) {
            double u = static_cast<double>(i) / radial_segs;
            double theta = u * TWO_PI;
            double cos_th = std::cos(theta);
            double sin_th = std::sin(theta);

            mesh.vertices.push_back(Point3D{r * cos_th, r * sin_th, z});
            mesh.normals.push_back(Vec3d{cos_th, sin_th, 0.0});
            mesh.uvs.push_back(Vec2d{u, v});
        }
    }

    // Side triangles
    uint32_t stride = radial_segs + 1;
    for (uint32_t j = 0; j < height_segs; ++j) {
        for (uint32_t i = 0; i < radial_segs; ++i) {
            uint32_t i0 = j * stride + i;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (j + 1) * stride + i;
            uint32_t i3 = i2 + 1;
            mesh.add_triangle(i0, i1, i3);
            mesh.add_triangle(i0, i3, i2);
        }
    }

    // Caps
    if (p.cap_fill != CapFillType::None) {
        // Top cap (+Z)
        uint32_t top_center_idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(Point3D{0.0, 0.0, half_h});
        mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
        mesh.uvs.push_back(Vec2d{0.5, 0.5});

        uint32_t top_rim_start = static_cast<uint32_t>(mesh.vertices.size());
        for (uint32_t i = 0; i < radial_segs; ++i) {
            double theta = (static_cast<double>(i) / radial_segs) * TWO_PI;
            double cos_th = std::cos(theta);
            double sin_th = std::sin(theta);
            mesh.vertices.push_back(Point3D{r * cos_th, r * sin_th, half_h});
            mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
            mesh.uvs.push_back(Vec2d{0.5 + 0.5 * cos_th, 0.5 + 0.5 * sin_th});
        }

        for (uint32_t i = 0; i < radial_segs; ++i) {
            uint32_t nxt = (i + 1) % radial_segs;
            mesh.add_triangle(top_center_idx, top_rim_start + i, top_rim_start + nxt);
        }

        // Bottom cap (-Z)
        uint32_t bot_center_idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(Point3D{0.0, 0.0, -half_h});
        mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
        mesh.uvs.push_back(Vec2d{0.5, 0.5});

        uint32_t bot_rim_start = static_cast<uint32_t>(mesh.vertices.size());
        for (uint32_t i = 0; i < radial_segs; ++i) {
            double theta = (static_cast<double>(i) / radial_segs) * TWO_PI;
            double cos_th = std::cos(theta);
            double sin_th = std::sin(theta);
            mesh.vertices.push_back(Point3D{r * cos_th, r * sin_th, -half_h});
            mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
            mesh.uvs.push_back(Vec2d{0.5 + 0.5 * cos_th, 0.5 - 0.5 * sin_th});
        }

        for (uint32_t i = 0; i < radial_segs; ++i) {
            uint32_t nxt = (i + 1) % radial_segs;
            mesh.add_triangle(bot_center_idx, bot_rim_start + nxt, bot_rim_start + i);
        }
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_cone(const ConeParams& p) {
    SolidMesh3D mesh;
    const uint32_t radial_segs = std::max(3u, p.radial_segments);
    const double r = std::max(1e-6, p.radius);
    const double h = std::max(1e-6, p.height);
    const double half_h = h * 0.5;

    // Apex
    Point3D apex{0.0, 0.0, half_h};
    double slant_len = std::sqrt(r * r + h * h);
    double slant_cos = r / slant_len;
    double slant_sin = h / slant_len;

    // Sides
    for (uint32_t i = 0; i < radial_segs; ++i) {
        double theta0 = (static_cast<double>(i) / radial_segs) * TWO_PI;
        double theta1 = (static_cast<double>(i + 1) / radial_segs) * TWO_PI;

        Vec3d n0{slant_sin * std::cos(theta0), slant_sin * std::sin(theta0), slant_cos};
        Vec3d n1{slant_sin * std::cos(theta1), slant_sin * std::sin(theta1), slant_cos};
        Vec3d n_mid = ((n0 + n1) * 0.5).normalized();

        Point3D p0{r * std::cos(theta0), r * std::sin(theta0), -half_h};
        Point3D p1{r * std::cos(theta1), r * std::sin(theta1), -half_h};

        uint32_t idx_apex = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(apex);
        mesh.normals.push_back(n_mid);
        mesh.uvs.push_back(Vec2d{0.5, 1.0});

        uint32_t idx_p0 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(p0);
        mesh.normals.push_back(n0);
        mesh.uvs.push_back(Vec2d{static_cast<double>(i) / radial_segs, 0.0});

        uint32_t idx_p1 = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(p1);
        mesh.normals.push_back(n1);
        mesh.uvs.push_back(Vec2d{static_cast<double>(i + 1) / radial_segs, 0.0});

        mesh.add_triangle(idx_apex, idx_p0, idx_p1);
    }

    // Base cap (-Z)
    if (p.cap_fill != CapFillType::None) {
        uint32_t center_idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(Point3D{0.0, 0.0, -half_h});
        mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
        mesh.uvs.push_back(Vec2d{0.5, 0.5});

        uint32_t rim_start = static_cast<uint32_t>(mesh.vertices.size());
        for (uint32_t i = 0; i < radial_segs; ++i) {
            double theta = (static_cast<double>(i) / radial_segs) * TWO_PI;
            double cos_th = std::cos(theta);
            double sin_th = std::sin(theta);
            mesh.vertices.push_back(Point3D{r * cos_th, r * sin_th, -half_h});
            mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
            mesh.uvs.push_back(Vec2d{0.5 + 0.5 * cos_th, 0.5 - 0.5 * sin_th});
        }

        for (uint32_t i = 0; i < radial_segs; ++i) {
            uint32_t nxt = (i + 1) % radial_segs;
            mesh.add_triangle(center_idx, rim_start + nxt, rim_start + i);
        }
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_torus(const TorusParams& p) {
    SolidMesh3D mesh;
    const uint32_t major_segs = std::max(3u, p.major_segments);
    const uint32_t minor_segs = std::max(3u, p.minor_segments);
    const double R = std::max(1e-6, p.major_radius);
    const double r = std::max(1e-6, p.minor_radius);

    for (uint32_t j = 0; j <= major_segs; ++j) {
        double u = static_cast<double>(j) / major_segs;
        double phi = u * TWO_PI;
        double cos_phi = std::cos(phi);
        double sin_phi = std::sin(phi);

        for (uint32_t i = 0; i <= minor_segs; ++i) {
            double v = static_cast<double>(i) / minor_segs;
            double theta = v * TWO_PI;
            double cos_th = std::cos(theta);
            double sin_th = std::sin(theta);

            Point3D pos{
                (R + r * cos_th) * cos_phi,
                (R + r * cos_th) * sin_phi,
                r * sin_th
            };

            Vec3d norm{
                cos_th * cos_phi,
                cos_th * sin_phi,
                sin_th
            };

            mesh.vertices.push_back(pos);
            mesh.normals.push_back(norm);
            mesh.uvs.push_back(Vec2d{u, v});
        }
    }

    uint32_t stride = minor_segs + 1;
    for (uint32_t j = 0; j < major_segs; ++j) {
        for (uint32_t i = 0; i < minor_segs; ++i) {
            uint32_t i0 = j * stride + i;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (j + 1) * stride + i;
            uint32_t i3 = i2 + 1;

            mesh.add_triangle(i0, i1, i3);
            mesh.add_triangle(i0, i3, i2);
        }
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_plane(const PlaneParams& p) {
    SolidMesh3D mesh;
    const uint32_t sx = std::max(1u, p.segments_x);
    const uint32_t sy = std::max(1u, p.segments_y);
    const double hx = p.width * 0.5;
    const double hy = p.height * 0.5;

    for (uint32_t j = 0; j <= sy; ++j) {
        double v = static_cast<double>(j) / sy;
        double y = -hy + v * p.height;
        for (uint32_t i = 0; i <= sx; ++i) {
            double u = static_cast<double>(i) / sx;
            double x = -hx + u * p.width;

            mesh.vertices.push_back(Point3D{x, y, 0.0});
            mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
            mesh.uvs.push_back(Vec2d{u, v});
        }
    }

    uint32_t stride = sx + 1;
    for (uint32_t j = 0; j < sy; ++j) {
        for (uint32_t i = 0; i < sx; ++i) {
            uint32_t i0 = j * stride + i;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (j + 1) * stride + i;
            uint32_t i3 = i2 + 1;

            mesh.add_triangle(i0, i1, i3);
            mesh.add_triangle(i0, i3, i2);
        }
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_circle(const CircleParams& p) {
    SolidMesh3D mesh;
    const uint32_t segs = std::max(3u, p.segments);
    const double r = std::max(1e-6, p.radius);

    uint32_t center_idx = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Point3D{0.0, 0.0, 0.0});
    mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
    mesh.uvs.push_back(Vec2d{0.5, 0.5});

    for (uint32_t i = 0; i < segs; ++i) {
        double theta = (static_cast<double>(i) / segs) * TWO_PI;
        double cos_th = std::cos(theta);
        double sin_th = std::sin(theta);
        mesh.vertices.push_back(Point3D{r * cos_th, r * sin_th, 0.0});
        mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.5 * cos_th, 0.5 + 0.5 * sin_th});
    }

    if (p.fill == CircleFillType::TriangleFan) {
        for (uint32_t i = 0; i < segs; ++i) {
            uint32_t i0 = center_idx;
            uint32_t i1 = 1 + i;
            uint32_t i2 = 1 + (i + 1) % segs;
            mesh.add_triangle(i0, i1, i2);
        }
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_grid(const GridParams& p) {
    PlaneParams pl;
    pl.width = p.width;
    pl.height = p.height;
    pl.segments_x = std::max(1u, p.subdivisions_x);
    pl.segments_y = std::max(1u, p.subdivisions_y);
    return generate_plane(pl);
}

SolidMesh3D ParametricPrimitiveGenerator::generate_capsule(const CapsuleParams& p) {
    SolidMesh3D mesh;
    const uint32_t radial_segs = std::max(4u, p.radial_segments);
    const uint32_t rings = std::max(2u, p.rings);
    const double r = std::max(1e-6, p.radius);
    const double ch = std::max(0.0, p.cylinder_height);
    const double half_ch = ch * 0.5;

    // Top hemisphere (+Z)
    for (uint32_t lat = 0; lat <= rings; ++lat) {
        double v = static_cast<double>(lat) / rings;
        double theta = v * (0.5 * PI); // 0 (pole) to PI/2 (equator)
        double sin_th = std::sin(theta);
        double cos_th = std::cos(theta);

        for (uint32_t lon = 0; lon <= radial_segs; ++lon) {
            double u = static_cast<double>(lon) / radial_segs;
            double phi = u * TWO_PI;
            double sin_phi = std::sin(phi);
            double cos_phi = std::cos(phi);

            Vec3d norm{sin_th * cos_phi, sin_th * sin_phi, cos_th};
            Point3D pos{norm.x * r, norm.y * r, half_ch + norm.z * r};

            mesh.vertices.push_back(pos);
            mesh.normals.push_back(norm);
            mesh.uvs.push_back(Vec2d{u, 0.75 + 0.25 * (1.0 - v)});
        }
    }

    // Bottom hemisphere (-Z)
    for (uint32_t lat = 0; lat <= rings; ++lat) {
        double v = static_cast<double>(lat) / rings;
        double theta = (0.5 * PI) + v * (0.5 * PI); // PI/2 (equator) to PI (south pole)
        double sin_th = std::sin(theta);
        double cos_th = std::cos(theta);

        for (uint32_t lon = 0; lon <= radial_segs; ++lon) {
            double u = static_cast<double>(lon) / radial_segs;
            double phi = u * TWO_PI;
            double sin_phi = std::sin(phi);
            double cos_phi = std::cos(phi);

            Vec3d norm{sin_th * cos_phi, sin_th * sin_phi, cos_th};
            Point3D pos{norm.x * r, norm.y * r, -half_ch + norm.z * r};

            mesh.vertices.push_back(pos);
            mesh.normals.push_back(norm);
            mesh.uvs.push_back(Vec2d{u, 0.25 * (1.0 - v)});
        }
    }

    // Connect top hemisphere
    uint32_t stride = radial_segs + 1;
    for (uint32_t lat = 0; lat < rings; ++lat) {
        for (uint32_t lon = 0; lon < radial_segs; ++lon) {
            uint32_t i0 = lat * stride + lon;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (lat + 1) * stride + lon;
            uint32_t i3 = i2 + 1;

            if (lat == 0) {
                mesh.add_triangle(i0, i2, i3);
            } else {
                mesh.add_triangle(i0, i2, i1);
                mesh.add_triangle(i1, i2, i3);
            }
        }
    }

    // Connect cylinder middle section between top equator and bottom equator
    uint32_t top_equator_start = rings * stride;
    uint32_t bot_equator_start = (rings + 1) * stride;

    for (uint32_t lon = 0; lon < radial_segs; ++lon) {
        uint32_t t0 = top_equator_start + lon;
        uint32_t t1 = t0 + 1;
        uint32_t b0 = bot_equator_start + lon;
        uint32_t b1 = b0 + 1;

        mesh.add_triangle(t0, b0, t1);
        mesh.add_triangle(t1, b0, b1);
    }

    // Connect bottom hemisphere
    uint32_t bot_start = bot_equator_start;
    for (uint32_t lat = 0; lat < rings; ++lat) {
        for (uint32_t lon = 0; lon < radial_segs; ++lon) {
            uint32_t i0 = bot_start + lat * stride + lon;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = bot_start + (lat + 1) * stride + lon;
            uint32_t i3 = i2 + 1;

            if (lat == rings - 1) {
                mesh.add_triangle(i0, i2, i1);
            } else {
                mesh.add_triangle(i0, i2, i1);
                mesh.add_triangle(i1, i2, i3);
            }
        }
    }

    return mesh;
}

// ============================================================================
// Engineering Fasteners (PH4-05.01.003)
// ============================================================================

SolidMesh3D ParametricPrimitiveGenerator::generate_hex_bolt(const HexBoltParams& p) {
    SolidMesh3D mesh;
    const double shank_r = p.nominal_diameter * 0.5;
    const double hex_r = (p.head_across_flats * 0.5) / std::cos(PI / 6.0); // circumradius
    const double k = p.head_height;
    const double L = p.total_length;

    // Hex head (prism centered from Z = 0 to Z = k)
    // Top face of hex head (+Z = k)
    uint32_t head_top_center = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Point3D{0.0, 0.0, k});
    mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
    mesh.uvs.push_back(Vec2d{0.5, 0.5});

    uint32_t head_top_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < 6; ++i) {
        double th = (static_cast<double>(i) / 6.0) * TWO_PI;
        mesh.vertices.push_back(Point3D{hex_r * std::cos(th), hex_r * std::sin(th), k});
        mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.5 * std::cos(th), 0.5 + 0.5 * std::sin(th)});
    }
    for (uint32_t i = 0; i < 6; ++i) {
        mesh.add_triangle(head_top_center, head_top_rim + i, head_top_rim + (i + 1) % 6);
    }

    // Head side faces
    uint32_t head_bot_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < 6; ++i) {
        double th = (static_cast<double>(i) / 6.0) * TWO_PI;
        mesh.vertices.push_back(Point3D{hex_r * std::cos(th), hex_r * std::sin(th), 0.0});
        mesh.normals.push_back(Vec3d{std::cos(th), std::sin(th), 0.0});
        mesh.uvs.push_back(Vec2d{static_cast<double>(i) / 6.0, 0.0});
    }
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t nxt = (i + 1) % 6;
        uint32_t t0 = head_top_rim + i;
        uint32_t t1 = head_top_rim + nxt;
        uint32_t b0 = head_bot_rim + i;
        uint32_t b1 = head_bot_rim + nxt;
        mesh.add_triangle(t0, b0, t1);
        mesh.add_triangle(t1, b0, b1);
    }

    // Cylindrical shank from Z = 0 to Z = -L
    const uint32_t shank_segs = 24;
    // Under-head annular face connecting hex_bot_rim to shank_top
    uint32_t shank_top_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < shank_segs; ++i) {
        double th = (static_cast<double>(i) / shank_segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{shank_r * std::cos(th), shank_r * std::sin(th), 0.0});
        mesh.normals.push_back(Vec3d{std::cos(th), std::sin(th), 0.0});
        mesh.uvs.push_back(Vec2d{static_cast<double>(i) / shank_segs, 1.0});
    }

    // Bottom of shank
    uint32_t shank_bot_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < shank_segs; ++i) {
        double th = (static_cast<double>(i) / shank_segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{shank_r * std::cos(th), shank_r * std::sin(th), -L});
        mesh.normals.push_back(Vec3d{std::cos(th), std::sin(th), 0.0});
        mesh.uvs.push_back(Vec2d{static_cast<double>(i) / shank_segs, 0.0});
    }

    // Shank cylinder quads
    for (uint32_t i = 0; i < shank_segs; ++i) {
        uint32_t nxt = (i + 1) % shank_segs;
        uint32_t t0 = shank_top_rim + i;
        uint32_t t1 = shank_top_rim + nxt;
        uint32_t b0 = shank_bot_rim + i;
        uint32_t b1 = shank_bot_rim + nxt;
        mesh.add_triangle(t0, b0, t1);
        mesh.add_triangle(t1, b0, b1);
    }

    // Shank bottom tip cap (-Z)
    uint32_t shank_tip_center = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Point3D{0.0, 0.0, -L});
    mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
    mesh.uvs.push_back(Vec2d{0.5, 0.5});
    for (uint32_t i = 0; i < shank_segs; ++i) {
        uint32_t nxt = (i + 1) % shank_segs;
        mesh.add_triangle(shank_tip_center, shank_bot_rim + nxt, shank_bot_rim + i);
    }

    // Cap between hex bottom and shank top
    // Approximate with fan to center
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t nxt = (i + 1) % 6;
        mesh.add_triangle(head_bot_rim + nxt, head_bot_rim + i, shank_top_rim + (i * shank_segs / 6));
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_washer(const WasherParams& p) {
    SolidMesh3D mesh;
    const double r_in = std::max(1e-6, p.inner_diameter * 0.5);
    const double r_out = std::max(r_in + 1e-4, p.outer_diameter * 0.5);
    const double thk = std::max(1e-6, p.thickness);
    const double half_t = thk * 0.5;
    const uint32_t segs = 32;

    // Top annular face (+Z = half_t)
    uint32_t top_in_start = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < segs; ++i) {
        double th = (static_cast<double>(i) / segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{r_in * std::cos(th), r_in * std::sin(th), half_t});
        mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.3 * std::cos(th), 0.5 + 0.3 * std::sin(th)});
    }

    uint32_t top_out_start = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < segs; ++i) {
        double th = (static_cast<double>(i) / segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{r_out * std::cos(th), r_out * std::sin(th), half_t});
        mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.5 * std::cos(th), 0.5 + 0.5 * std::sin(th)});
    }

    for (uint32_t i = 0; i < segs; ++i) {
        uint32_t nxt = (i + 1) % segs;
        uint32_t i_in = top_in_start + i;
        uint32_t n_in = top_in_start + nxt;
        uint32_t i_out = top_out_start + i;
        uint32_t n_out = top_out_start + nxt;
        mesh.add_triangle(i_in, i_out, n_out);
        mesh.add_triangle(i_in, n_out, n_in);
    }

    // Bottom annular face (-Z = -half_t)
    uint32_t bot_in_start = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < segs; ++i) {
        double th = (static_cast<double>(i) / segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{r_in * std::cos(th), r_in * std::sin(th), -half_t});
        mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.3 * std::cos(th), 0.5 - 0.3 * std::sin(th)});
    }

    uint32_t bot_out_start = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < segs; ++i) {
        double th = (static_cast<double>(i) / segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{r_out * std::cos(th), r_out * std::sin(th), -half_t});
        mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.5 * std::cos(th), 0.5 - 0.5 * std::sin(th)});
    }

    for (uint32_t i = 0; i < segs; ++i) {
        uint32_t nxt = (i + 1) % segs;
        uint32_t i_in = bot_in_start + i;
        uint32_t n_in = bot_in_start + nxt;
        uint32_t i_out = bot_out_start + i;
        uint32_t n_out = bot_out_start + nxt;
        mesh.add_triangle(i_in, n_out, i_out);
        mesh.add_triangle(i_in, n_in, n_out);
    }

    // Outer cylindrical wall
    for (uint32_t i = 0; i < segs; ++i) {
        uint32_t nxt = (i + 1) % segs;
        uint32_t t0 = top_out_start + i;
        uint32_t t1 = top_out_start + nxt;
        uint32_t b0 = bot_out_start + i;
        uint32_t b1 = bot_out_start + nxt;
        mesh.add_triangle(t0, b0, t1);
        mesh.add_triangle(t1, b0, b1);
    }

    // Inner cylindrical wall (facing inward)
    for (uint32_t i = 0; i < segs; ++i) {
        uint32_t nxt = (i + 1) % segs;
        uint32_t t0 = top_in_start + i;
        uint32_t t1 = top_in_start + nxt;
        uint32_t b0 = bot_in_start + i;
        uint32_t b1 = bot_in_start + nxt;
        mesh.add_triangle(t0, t1, b0);
        mesh.add_triangle(t1, b1, b0);
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_nut(const NutParams& p) {
    SolidMesh3D mesh;
    const double hex_r = (p.across_flats * 0.5) / std::cos(PI / 6.0);
    const double r_in = p.nominal_diameter * 0.5;
    const double half_h = p.height * 0.5;
    const uint32_t inner_segs = 18;

    // Top hex rim with bore
    uint32_t top_hex_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < 6; ++i) {
        double th = (static_cast<double>(i) / 6.0) * TWO_PI;
        mesh.vertices.push_back(Point3D{hex_r * std::cos(th), hex_r * std::sin(th), half_h});
        mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.5 * std::cos(th), 0.5 + 0.5 * std::sin(th)});
    }

    uint32_t top_in_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < inner_segs; ++i) {
        double th = (static_cast<double>(i) / inner_segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{r_in * std::cos(th), r_in * std::sin(th), half_h});
        mesh.normals.push_back(Vec3d{0.0, 0.0, 1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.25 * std::cos(th), 0.5 + 0.25 * std::sin(th)});
    }

    // Triangulate top face between 6 outer vertices and inner_segs inner vertices
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t nxt = (i + 1) % 6;
        uint32_t in_i = (i * inner_segs) / 6;
        uint32_t in_nxt = ((i + 1) * inner_segs) / 6;
        mesh.add_triangle(top_hex_rim + i, top_in_rim + in_i, top_hex_rim + nxt);
        mesh.add_triangle(top_hex_rim + nxt, top_in_rim + in_i, top_in_rim + in_nxt);
    }

    // Bottom hex rim with bore
    uint32_t bot_hex_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < 6; ++i) {
        double th = (static_cast<double>(i) / 6.0) * TWO_PI;
        mesh.vertices.push_back(Point3D{hex_r * std::cos(th), hex_r * std::sin(th), -half_h});
        mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.5 * std::cos(th), 0.5 - 0.5 * std::sin(th)});
    }

    uint32_t bot_in_rim = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t i = 0; i < inner_segs; ++i) {
        double th = (static_cast<double>(i) / inner_segs) * TWO_PI;
        mesh.vertices.push_back(Point3D{r_in * std::cos(th), r_in * std::sin(th), -half_h});
        mesh.normals.push_back(Vec3d{0.0, 0.0, -1.0});
        mesh.uvs.push_back(Vec2d{0.5 + 0.25 * std::cos(th), 0.5 - 0.25 * std::sin(th)});
    }

    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t nxt = (i + 1) % 6;
        uint32_t in_i = (i * inner_segs) / 6;
        uint32_t in_nxt = ((i + 1) * inner_segs) / 6;
        mesh.add_triangle(bot_hex_rim + i, bot_hex_rim + nxt, bot_in_rim + in_i);
        mesh.add_triangle(bot_hex_rim + nxt, bot_in_rim + in_nxt, bot_in_rim + in_i);
    }

    // Outer 6 hex side faces
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t nxt = (i + 1) % 6;
        uint32_t t0 = top_hex_rim + i;
        uint32_t t1 = top_hex_rim + nxt;
        uint32_t b0 = bot_hex_rim + i;
        uint32_t b1 = bot_hex_rim + nxt;
        mesh.add_triangle(t0, b0, t1);
        mesh.add_triangle(t1, b0, b1);
    }

    // Inner bore cylinder
    for (uint32_t i = 0; i < inner_segs; ++i) {
        uint32_t nxt = (i + 1) % inner_segs;
        uint32_t t0 = top_in_rim + i;
        uint32_t t1 = top_in_rim + nxt;
        uint32_t b0 = bot_in_rim + i;
        uint32_t b1 = bot_in_rim + nxt;
        mesh.add_triangle(t0, t1, b0);
        mesh.add_triangle(t1, b1, b0);
    }

    return mesh;
}

SolidMesh3D ParametricPrimitiveGenerator::generate_pin(const PinParams& p) {
    CylinderParams cp;
    cp.radius = p.diameter * 0.5;
    cp.height = p.length;
    cp.radial_segments = 32;
    cp.height_segments = 1;
    cp.cap_fill = CapFillType::TriangleFan;
    return generate_cylinder(cp);
}

// ============================================================================
// Topology & Mesh Quality Validation (Task 9: PH4-05.01.004)
// ============================================================================

MeshQualityReport ParametricMeshValidator::validate_topology_and_attributes(
    const SolidMesh3D& mesh,
    bool closed_expected
) {
    MeshQualityReport rep;

    // 1. Validate normals
    if (mesh.normals.size() != mesh.vertices.size()) {
        rep.normals_valid = false;
        rep.zero_normals_count = mesh.vertices.size();
    } else {
        for (const auto& n : mesh.normals) {
            double len = n.length();
            if (!std::isfinite(len) || len < 0.5 || len > 1.5) {
                rep.normals_valid = false;
                ++rep.zero_normals_count;
            }
        }
    }

    // 2. Validate UV coordinates
    if (mesh.uvs.size() != mesh.vertices.size()) {
        rep.uvs_valid = false;
        rep.invalid_uvs_count = mesh.vertices.size();
    } else {
        for (const auto& uv : mesh.uvs) {
            if (!std::isfinite(uv.x) || !std::isfinite(uv.y) ||
                uv.x < -0.05 || uv.x > 1.05 || uv.y < -0.05 || uv.y > 1.05) {
                rep.uvs_valid = false;
                ++rep.invalid_uvs_count;
            }
        }
    }

    // 3. Topology & Manifold Analysis
    // Create a copy and weld vertices within tolerance to analyze connectivity
    SolidMesh3D topo_mesh = mesh;
    topo_mesh.weld_vertices(1e-4);

    auto m_rep = topo_mesh.analyze_manifold();
    rep.euler_characteristic = m_rep.euler_characteristic;

    if (closed_expected) {
        rep.is_watertight = m_rep.is_watertight;
        rep.is_2_manifold = m_rep.is_2_manifold;
    } else {
        // Open surfaces (Plane, Circle, Grid) have boundary edges but no non-manifold edges
        rep.is_watertight = false;
        rep.is_2_manifold = (m_rep.non_manifold_edge_count == 0);
    }

    return rep;
}

// ============================================================================
// Analytical Calculations (Task 10: PH4-05.01.005)
// ============================================================================

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const CubeParams& p) {
    AnalyticalProperties props;
    props.volume = p.width * p.height * p.depth;
    props.surface_area = 2.0 * (p.width * p.height + p.height * p.depth + p.depth * p.width);
    const double hx = p.width * 0.5;
    const double hy = p.height * 0.5;
    const double hz = p.depth * 0.5;
    props.bbox.min = Point3D{-hx, -hy, -hz};
    props.bbox.max = Point3D{hx, hy, hz};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const SphereParams& p) {
    AnalyticalProperties props;
    props.volume = (4.0 / 3.0) * PI * std::pow(p.radius, 3);
    props.surface_area = 4.0 * PI * std::pow(p.radius, 2);
    props.bbox.min = Point3D{-p.radius, -p.radius, -p.radius};
    props.bbox.max = Point3D{p.radius, p.radius, p.radius};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const UvSphereParams& p) {
    SphereParams sp;
    sp.radius = p.radius;
    return compute_analytical(sp);
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const IcosphereParams& p) {
    SphereParams sp;
    sp.radius = p.radius;
    return compute_analytical(sp);
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const CylinderParams& p) {
    AnalyticalProperties props;
    props.volume = PI * p.radius * p.radius * p.height;
    double side_area = 2.0 * PI * p.radius * p.height;
    double cap_area = (p.cap_fill != CapFillType::None) ? (2.0 * PI * p.radius * p.radius) : 0.0;
    props.surface_area = side_area + cap_area;
    const double half_h = p.height * 0.5;
    props.bbox.min = Point3D{-p.radius, -p.radius, -half_h};
    props.bbox.max = Point3D{p.radius, p.radius, half_h};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const ConeParams& p) {
    AnalyticalProperties props;
    props.volume = (1.0 / 3.0) * PI * p.radius * p.radius * p.height;
    double slant = std::sqrt(p.radius * p.radius + p.height * p.height);
    double side_area = PI * p.radius * slant;
    double base_area = (p.cap_fill != CapFillType::None) ? (PI * p.radius * p.radius) : 0.0;
    props.surface_area = side_area + base_area;
    const double half_h = p.height * 0.5;
    props.bbox.min = Point3D{-p.radius, -p.radius, -half_h};
    props.bbox.max = Point3D{p.radius, p.radius, half_h};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const TorusParams& p) {
    AnalyticalProperties props;
    // Torus volume: V = 2 * pi^2 * R * r^2
    props.volume = 2.0 * PI * PI * p.major_radius * p.minor_radius * p.minor_radius;
    // Torus surface area: A = 4 * pi^2 * R * r
    props.surface_area = 4.0 * PI * PI * p.major_radius * p.minor_radius;
    double max_xy = p.major_radius + p.minor_radius;
    props.bbox.min = Point3D{-max_xy, -max_xy, -p.minor_radius};
    props.bbox.max = Point3D{max_xy, max_xy, p.minor_radius};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const PlaneParams& p) {
    AnalyticalProperties props;
    props.volume = 0.0;
    props.surface_area = p.width * p.height;
    const double hx = p.width * 0.5;
    const double hy = p.height * 0.5;
    props.bbox.min = Point3D{-hx, -hy, 0.0};
    props.bbox.max = Point3D{hx, hy, 0.0};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const CircleParams& p) {
    AnalyticalProperties props;
    props.volume = 0.0;
    props.surface_area = PI * p.radius * p.radius;
    props.bbox.min = Point3D{-p.radius, -p.radius, 0.0};
    props.bbox.max = Point3D{p.radius, p.radius, 0.0};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const GridParams& p) {
    PlaneParams pl;
    pl.width = p.width;
    pl.height = p.height;
    return compute_analytical(pl);
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const CapsuleParams& p) {
    AnalyticalProperties props;
    // Volume: cylinder + 2 hemispheres (1 full sphere)
    props.volume = PI * p.radius * p.radius * p.cylinder_height + (4.0 / 3.0) * PI * std::pow(p.radius, 3);
    // Surface area: cylinder wall + sphere
    props.surface_area = 2.0 * PI * p.radius * p.cylinder_height + 4.0 * PI * std::pow(p.radius, 2);
    const double half_extent_z = p.cylinder_height * 0.5 + p.radius;
    props.bbox.min = Point3D{-p.radius, -p.radius, -half_extent_z};
    props.bbox.max = Point3D{p.radius, p.radius, half_extent_z};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const HexBoltParams& p) {
    AnalyticalProperties props;
    // Hex prism volume: (sqrt(3)/2) * s^2 * k
    double hex_area = (std::sqrt(3.0) * 0.5) * p.head_across_flats * p.head_across_flats;
    double head_vol = hex_area * p.head_height;
    double shank_r = p.nominal_diameter * 0.5;
    double shank_vol = PI * shank_r * shank_r * p.total_length;
    props.volume = head_vol + shank_vol;

    // Surface area
    double hex_side = p.head_across_flats / std::sqrt(3.0);
    double head_sides = 6.0 * hex_side * p.head_height;
    double head_top = hex_area;
    double head_bot_exposed = hex_area - (PI * shank_r * shank_r);
    double shank_side = 2.0 * PI * shank_r * p.total_length;
    double shank_tip = PI * shank_r * shank_r;
    props.surface_area = head_sides + head_top + head_bot_exposed + shank_side + shank_tip;

    double hex_r = (p.head_across_flats * 0.5) / std::cos(PI / 6.0);
    props.bbox.min = Point3D{-hex_r, -hex_r, -p.total_length};
    props.bbox.max = Point3D{hex_r, hex_r, p.head_height};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const WasherParams& p) {
    AnalyticalProperties props;
    double r_in = p.inner_diameter * 0.5;
    double r_out = p.outer_diameter * 0.5;
    double face_area = PI * (r_out * r_out - r_in * r_in);
    props.volume = face_area * p.thickness;
    props.surface_area = 2.0 * face_area + 2.0 * PI * r_out * p.thickness + 2.0 * PI * r_in * p.thickness;
    const double half_t = p.thickness * 0.5;
    props.bbox.min = Point3D{-r_out, -r_out, -half_t};
    props.bbox.max = Point3D{r_out, r_out, half_t};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const NutParams& p) {
    AnalyticalProperties props;
    double hex_area = (std::sqrt(3.0) * 0.5) * p.across_flats * p.across_flats;
    double r_in = p.nominal_diameter * 0.5;
    double bore_vol = PI * r_in * r_in * p.height;
    props.volume = (hex_area * p.height) - bore_vol;

    double hex_side = p.across_flats / std::sqrt(3.0);
    double outer_sides = 6.0 * hex_side * p.height;
    double top_bot_faces = 2.0 * (hex_area - PI * r_in * r_in);
    double inner_bore_side = 2.0 * PI * r_in * p.height;
    props.surface_area = outer_sides + top_bot_faces + inner_bore_side;

    double hex_r = (p.across_flats * 0.5) / std::cos(PI / 6.0);
    const double half_h = p.height * 0.5;
    props.bbox.min = Point3D{-hex_r, -hex_r, -half_h};
    props.bbox.max = Point3D{hex_r, hex_r, half_h};
    return props;
}

AnalyticalProperties ParametricPrimitiveGenerator::compute_analytical(const PinParams& p) {
    CylinderParams cp;
    cp.radius = p.diameter * 0.5;
    cp.height = p.length;
    cp.cap_fill = CapFillType::TriangleFan;
    return compute_analytical(cp);
}

// ============================================================================
// Standard Fastener Presets
// ============================================================================

HexBoltParams ParametricPrimitiveGenerator::iso4014_hex_bolt(const std::string& size, double length) {
    HexBoltParams p;
    p.total_length = length;
    if (size == "M3") {
        p.nominal_diameter = 3.0; p.pitch = 0.5; p.head_across_flats = 5.5; p.head_height = 2.0; p.shank_length = length * 0.5;
    } else if (size == "M4") {
        p.nominal_diameter = 4.0; p.pitch = 0.7; p.head_across_flats = 7.0; p.head_height = 2.8; p.shank_length = length * 0.5;
    } else if (size == "M5") {
        p.nominal_diameter = 5.0; p.pitch = 0.8; p.head_across_flats = 8.0; p.head_height = 3.5; p.shank_length = length * 0.5;
    } else if (size == "M8") {
        p.nominal_diameter = 8.0; p.pitch = 1.25; p.head_across_flats = 13.0; p.head_height = 5.3; p.shank_length = length * 0.5;
    } else if (size == "M10") {
        p.nominal_diameter = 10.0; p.pitch = 1.5; p.head_across_flats = 16.0; p.head_height = 6.4; p.shank_length = length * 0.5;
    } else { // Default M6
        p.nominal_diameter = 6.0; p.pitch = 1.0; p.head_across_flats = 10.0; p.head_height = 4.0; p.shank_length = length * 0.5;
    }
    return p;
}

WasherParams ParametricPrimitiveGenerator::din125_washer(const std::string& size) {
    WasherParams p;
    if (size == "M3") {
        p.inner_diameter = 3.2; p.outer_diameter = 7.0; p.thickness = 0.5;
    } else if (size == "M4") {
        p.inner_diameter = 4.3; p.outer_diameter = 9.0; p.thickness = 0.8;
    } else if (size == "M5") {
        p.inner_diameter = 5.3; p.outer_diameter = 10.0; p.thickness = 1.0;
    } else if (size == "M8") {
        p.inner_diameter = 8.4; p.outer_diameter = 16.0; p.thickness = 1.6;
    } else if (size == "M10") {
        p.inner_diameter = 10.5; p.outer_diameter = 20.0; p.thickness = 2.0;
    } else { // Default M6
        p.inner_diameter = 6.4; p.outer_diameter = 12.0; p.thickness = 1.6;
    }
    return p;
}

NutParams ParametricPrimitiveGenerator::din934_nut(const std::string& size) {
    NutParams p;
    if (size == "M3") {
        p.nominal_diameter = 3.0; p.across_flats = 5.5; p.height = 2.4;
    } else if (size == "M4") {
        p.nominal_diameter = 4.0; p.across_flats = 7.0; p.height = 3.2;
    } else if (size == "M5") {
        p.nominal_diameter = 5.0; p.across_flats = 8.0; p.height = 4.0;
    } else if (size == "M8") {
        p.nominal_diameter = 8.0; p.across_flats = 13.0; p.height = 6.5;
    } else if (size == "M10") {
        p.nominal_diameter = 10.0; p.across_flats = 17.0; p.height = 8.0;
    } else { // Default M6
        p.nominal_diameter = 6.0; p.across_flats = 10.0; p.height = 5.0;
    }
    return p;
}

PinParams ParametricPrimitiveGenerator::iso8734_dowel_pin(double diameter, double length) {
    PinParams p;
    p.diameter = diameter;
    p.length = length;
    p.chamfer_length = diameter * 0.15;
    p.is_chamfered = true;
    return p;
}

// ============================================================================
// Non-Destructive Parametric Primitive Instance (Task 7: PH4-05.01.002)
// ============================================================================

ParametricPrimitive::ParametricPrimitive(PrimitiveType type)
    : type_(type), is_dirty_(true) {
    switch (type_) {
        case PrimitiveType::Cube: params_ = CubeParams{}; break;
        case PrimitiveType::Sphere: params_ = SphereParams{}; break;
        case PrimitiveType::UvSphere: params_ = UvSphereParams{}; break;
        case PrimitiveType::Icosphere: params_ = IcosphereParams{}; break;
        case PrimitiveType::Cylinder: params_ = CylinderParams{}; break;
        case PrimitiveType::Cone: params_ = ConeParams{}; break;
        case PrimitiveType::Torus: params_ = TorusParams{}; break;
        case PrimitiveType::Plane: params_ = PlaneParams{}; break;
        case PrimitiveType::Circle: params_ = CircleParams{}; break;
        case PrimitiveType::Grid: params_ = GridParams{}; break;
        case PrimitiveType::Capsule: params_ = CapsuleParams{}; break;
        case PrimitiveType::HexBolt: params_ = HexBoltParams{}; break;
        case PrimitiveType::Washer: params_ = WasherParams{}; break;
        case PrimitiveType::Nut: params_ = NutParams{}; break;
        case PrimitiveType::Pin: params_ = PinParams{}; break;
    }
}

ParametricPrimitive::ParametricPrimitive(PrimitiveType type, PrimitiveParamsVariant params)
    : type_(type), params_(std::move(params)), is_dirty_(true) {}

void ParametricPrimitive::set_params(PrimitiveParamsVariant new_params) {
    params_ = std::move(new_params);
    is_dirty_ = true;
}

bool ParametricPrimitive::has_property(const std::string& key) const {
    if (std::holds_alternative<CubeParams>(params_)) {
        return key == "width" || key == "height" || key == "depth";
    }
    if (std::holds_alternative<CylinderParams>(params_) || std::holds_alternative<ConeParams>(params_)) {
        return key == "radius" || key == "height";
    }
    if (std::holds_alternative<SphereParams>(params_) || std::holds_alternative<UvSphereParams>(params_) ||
        std::holds_alternative<IcosphereParams>(params_) || std::holds_alternative<CircleParams>(params_)) {
        return key == "radius";
    }
    if (std::holds_alternative<TorusParams>(params_)) {
        return key == "major_radius" || key == "minor_radius";
    }
    if (std::holds_alternative<PlaneParams>(params_) || std::holds_alternative<GridParams>(params_)) {
        return key == "width" || key == "height";
    }
    if (std::holds_alternative<CapsuleParams>(params_)) {
        return key == "radius" || key == "cylinder_height";
    }
    if (std::holds_alternative<WasherParams>(params_)) {
        return key == "inner_diameter" || key == "outer_diameter" || key == "thickness";
    }
    if (std::holds_alternative<NutParams>(params_)) {
        return key == "nominal_diameter" || key == "across_flats" || key == "height";
    }
    if (std::holds_alternative<HexBoltParams>(params_)) {
        return key == "nominal_diameter" || key == "total_length" || key == "shank_length" ||
               key == "head_across_flats" || key == "head_height";
    }
    if (std::holds_alternative<PinParams>(params_)) {
        return key == "diameter" || key == "length";
    }
    return false;
}

double ParametricPrimitive::get_double_property(const std::string& key, double default_val) const {
    if (std::holds_alternative<CubeParams>(params_)) {
        const auto& p = std::get<CubeParams>(params_);
        if (key == "width") return p.width;
        if (key == "height") return p.height;
        if (key == "depth") return p.depth;
    } else if (std::holds_alternative<CylinderParams>(params_)) {
        const auto& p = std::get<CylinderParams>(params_);
        if (key == "radius") return p.radius;
        if (key == "height") return p.height;
    } else if (std::holds_alternative<ConeParams>(params_)) {
        const auto& p = std::get<ConeParams>(params_);
        if (key == "radius") return p.radius;
        if (key == "height") return p.height;
    } else if (std::holds_alternative<SphereParams>(params_)) {
        const auto& p = std::get<SphereParams>(params_);
        if (key == "radius") return p.radius;
    } else if (std::holds_alternative<UvSphereParams>(params_)) {
        const auto& p = std::get<UvSphereParams>(params_);
        if (key == "radius") return p.radius;
    } else if (std::holds_alternative<IcosphereParams>(params_)) {
        const auto& p = std::get<IcosphereParams>(params_);
        if (key == "radius") return p.radius;
    } else if (std::holds_alternative<CircleParams>(params_)) {
        const auto& p = std::get<CircleParams>(params_);
        if (key == "radius") return p.radius;
    } else if (std::holds_alternative<TorusParams>(params_)) {
        const auto& p = std::get<TorusParams>(params_);
        if (key == "major_radius") return p.major_radius;
        if (key == "minor_radius") return p.minor_radius;
    } else if (std::holds_alternative<PlaneParams>(params_)) {
        const auto& p = std::get<PlaneParams>(params_);
        if (key == "width") return p.width;
        if (key == "height") return p.height;
    } else if (std::holds_alternative<GridParams>(params_)) {
        const auto& p = std::get<GridParams>(params_);
        if (key == "width") return p.width;
        if (key == "height") return p.height;
    } else if (std::holds_alternative<CapsuleParams>(params_)) {
        const auto& p = std::get<CapsuleParams>(params_);
        if (key == "radius") return p.radius;
        if (key == "cylinder_height") return p.cylinder_height;
    } else if (std::holds_alternative<WasherParams>(params_)) {
        const auto& p = std::get<WasherParams>(params_);
        if (key == "inner_diameter") return p.inner_diameter;
        if (key == "outer_diameter") return p.outer_diameter;
        if (key == "thickness") return p.thickness;
    } else if (std::holds_alternative<NutParams>(params_)) {
        const auto& p = std::get<NutParams>(params_);
        if (key == "nominal_diameter") return p.nominal_diameter;
        if (key == "across_flats") return p.across_flats;
        if (key == "height") return p.height;
    } else if (std::holds_alternative<HexBoltParams>(params_)) {
        const auto& p = std::get<HexBoltParams>(params_);
        if (key == "nominal_diameter") return p.nominal_diameter;
        if (key == "total_length") return p.total_length;
        if (key == "shank_length") return p.shank_length;
        if (key == "head_across_flats") return p.head_across_flats;
        if (key == "head_height") return p.head_height;
    } else if (std::holds_alternative<PinParams>(params_)) {
        const auto& p = std::get<PinParams>(params_);
        if (key == "diameter") return p.diameter;
        if (key == "length") return p.length;
    }
    return default_val;
}

void ParametricPrimitive::set_double_property(const std::string& key, double val) {
    if (std::holds_alternative<CubeParams>(params_)) {
        auto& p = std::get<CubeParams>(params_);
        if (key == "width") { p.width = val; is_dirty_ = true; }
        else if (key == "height") { p.height = val; is_dirty_ = true; }
        else if (key == "depth") { p.depth = val; is_dirty_ = true; }
    } else if (std::holds_alternative<CylinderParams>(params_)) {
        auto& p = std::get<CylinderParams>(params_);
        if (key == "radius") { p.radius = val; is_dirty_ = true; }
        else if (key == "height") { p.height = val; is_dirty_ = true; }
    } else if (std::holds_alternative<ConeParams>(params_)) {
        auto& p = std::get<ConeParams>(params_);
        if (key == "radius") { p.radius = val; is_dirty_ = true; }
        else if (key == "height") { p.height = val; is_dirty_ = true; }
    } else if (std::holds_alternative<SphereParams>(params_)) {
        auto& p = std::get<SphereParams>(params_);
        if (key == "radius") { p.radius = val; is_dirty_ = true; }
    } else if (std::holds_alternative<UvSphereParams>(params_)) {
        auto& p = std::get<UvSphereParams>(params_);
        if (key == "radius") { p.radius = val; is_dirty_ = true; }
    } else if (std::holds_alternative<IcosphereParams>(params_)) {
        auto& p = std::get<IcosphereParams>(params_);
        if (key == "radius") { p.radius = val; is_dirty_ = true; }
    } else if (std::holds_alternative<CircleParams>(params_)) {
        auto& p = std::get<CircleParams>(params_);
        if (key == "radius") { p.radius = val; is_dirty_ = true; }
    } else if (std::holds_alternative<TorusParams>(params_)) {
        auto& p = std::get<TorusParams>(params_);
        if (key == "major_radius") { p.major_radius = val; is_dirty_ = true; }
        else if (key == "minor_radius") { p.minor_radius = val; is_dirty_ = true; }
    } else if (std::holds_alternative<PlaneParams>(params_)) {
        auto& p = std::get<PlaneParams>(params_);
        if (key == "width") { p.width = val; is_dirty_ = true; }
        else if (key == "height") { p.height = val; is_dirty_ = true; }
    } else if (std::holds_alternative<GridParams>(params_)) {
        auto& p = std::get<GridParams>(params_);
        if (key == "width") { p.width = val; is_dirty_ = true; }
        else if (key == "height") { p.height = val; is_dirty_ = true; }
    } else if (std::holds_alternative<CapsuleParams>(params_)) {
        auto& p = std::get<CapsuleParams>(params_);
        if (key == "radius") { p.radius = val; is_dirty_ = true; }
        else if (key == "cylinder_height") { p.cylinder_height = val; is_dirty_ = true; }
    } else if (std::holds_alternative<WasherParams>(params_)) {
        auto& p = std::get<WasherParams>(params_);
        if (key == "inner_diameter") { p.inner_diameter = val; is_dirty_ = true; }
        else if (key == "outer_diameter") { p.outer_diameter = val; is_dirty_ = true; }
        else if (key == "thickness") { p.thickness = val; is_dirty_ = true; }
    } else if (std::holds_alternative<NutParams>(params_)) {
        auto& p = std::get<NutParams>(params_);
        if (key == "nominal_diameter") { p.nominal_diameter = val; is_dirty_ = true; }
        else if (key == "across_flats") { p.across_flats = val; is_dirty_ = true; }
        else if (key == "height") { p.height = val; is_dirty_ = true; }
    } else if (std::holds_alternative<HexBoltParams>(params_)) {
        auto& p = std::get<HexBoltParams>(params_);
        if (key == "nominal_diameter") { p.nominal_diameter = val; is_dirty_ = true; }
        else if (key == "total_length") { p.total_length = val; is_dirty_ = true; }
        else if (key == "shank_length") { p.shank_length = val; is_dirty_ = true; }
        else if (key == "head_across_flats") { p.head_across_flats = val; is_dirty_ = true; }
        else if (key == "head_height") { p.head_height = val; is_dirty_ = true; }
    } else if (std::holds_alternative<PinParams>(params_)) {
        auto& p = std::get<PinParams>(params_);
        if (key == "diameter") { p.diameter = val; is_dirty_ = true; }
        else if (key == "length") { p.length = val; is_dirty_ = true; }
    }
}

uint32_t ParametricPrimitive::get_uint_property(const std::string& key, uint32_t default_val) const {
    if (std::holds_alternative<CylinderParams>(params_)) {
        const auto& p = std::get<CylinderParams>(params_);
        if (key == "radial_segments") return p.radial_segments;
    } else if (std::holds_alternative<ConeParams>(params_)) {
        const auto& p = std::get<ConeParams>(params_);
        if (key == "radial_segments") return p.radial_segments;
    } else if (std::holds_alternative<UvSphereParams>(params_)) {
        const auto& p = std::get<UvSphereParams>(params_);
        if (key == "segments") return p.segments;
        if (key == "rings") return p.rings;
    } else if (std::holds_alternative<IcosphereParams>(params_)) {
        const auto& p = std::get<IcosphereParams>(params_);
        if (key == "subdivisions") return p.subdivisions;
    }
    return default_val;
}

void ParametricPrimitive::set_uint_property(const std::string& key, uint32_t val) {
    if (std::holds_alternative<CylinderParams>(params_)) {
        auto& p = std::get<CylinderParams>(params_);
        if (key == "radial_segments") { p.radial_segments = val; is_dirty_ = true; }
    } else if (std::holds_alternative<ConeParams>(params_)) {
        auto& p = std::get<ConeParams>(params_);
        if (key == "radial_segments") { p.radial_segments = val; is_dirty_ = true; }
    } else if (std::holds_alternative<UvSphereParams>(params_)) {
        auto& p = std::get<UvSphereParams>(params_);
        if (key == "segments") { p.segments = val; is_dirty_ = true; }
        else if (key == "rings") { p.rings = val; is_dirty_ = true; }
    } else if (std::holds_alternative<IcosphereParams>(params_)) {
        auto& p = std::get<IcosphereParams>(params_);
        if (key == "subdivisions") { p.subdivisions = val; is_dirty_ = true; }
    }
}

const SolidMesh3D& ParametricPrimitive::mesh() {
    if (!is_dirty_) {
        return cached_mesh_;
    }

    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, CubeParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_cube(arg);
        } else if constexpr (std::is_same_v<T, SphereParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_sphere(arg);
        } else if constexpr (std::is_same_v<T, UvSphereParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_uv_sphere(arg);
        } else if constexpr (std::is_same_v<T, IcosphereParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_icosphere(arg);
        } else if constexpr (std::is_same_v<T, CylinderParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_cylinder(arg);
        } else if constexpr (std::is_same_v<T, ConeParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_cone(arg);
        } else if constexpr (std::is_same_v<T, TorusParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_torus(arg);
        } else if constexpr (std::is_same_v<T, PlaneParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_plane(arg);
        } else if constexpr (std::is_same_v<T, CircleParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_circle(arg);
        } else if constexpr (std::is_same_v<T, GridParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_grid(arg);
        } else if constexpr (std::is_same_v<T, CapsuleParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_capsule(arg);
        } else if constexpr (std::is_same_v<T, HexBoltParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_hex_bolt(arg);
        } else if constexpr (std::is_same_v<T, WasherParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_washer(arg);
        } else if constexpr (std::is_same_v<T, NutParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_nut(arg);
        } else if constexpr (std::is_same_v<T, PinParams>) {
            cached_mesh_ = ParametricPrimitiveGenerator::generate_pin(arg);
        }
    }, params_);

    is_dirty_ = false;
    return cached_mesh_;
}

AnalyticalProperties ParametricPrimitive::analytical_properties() const {
    return std::visit([](auto&& arg) -> AnalyticalProperties {
        return ParametricPrimitiveGenerator::compute_analytical(arg);
    }, params_);
}

} // namespace shunya::cad3d
