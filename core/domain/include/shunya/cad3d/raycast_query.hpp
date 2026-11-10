#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace shunya::cad3d {

struct Ray3D {
    Point3D origin{0.0, 0.0, 0.0};
    Vec3d direction{0.0, 0.0, 1.0};
};

struct RayHit3D {
    bool hit{false};
    double distance{std::numeric_limits<double>::infinity()};
    Point3D point{0.0, 0.0, 0.0};
    Vec3d normal{0.0, 0.0, 1.0};
    double u{0.0};
    double v{0.0};
    uint32_t triangle_index{0};
};

/**
 * @brief Branchless slab-method AABB intersection with precomputed inverse direction.
 */
[[nodiscard]] inline bool intersect_ray_aabb_fast(
    const Ray3D& ray,
    const Vec3d& inv_dir,
    const BBox3D& box,
    double max_t
) noexcept {
    const double tx1 = (box.min.x - ray.origin.x) * inv_dir.x;
    const double tx2 = (box.max.x - ray.origin.x) * inv_dir.x;
    double tmin = std::min(tx1, tx2);
    double tmax = std::max(tx1, tx2);

    const double ty1 = (box.min.y - ray.origin.y) * inv_dir.y;
    const double ty2 = (box.max.y - ray.origin.y) * inv_dir.y;
    tmin = std::max(tmin, std::min(ty1, ty2));
    tmax = std::min(tmax, std::max(ty1, ty2));

    const double tz1 = (box.min.z - ray.origin.z) * inv_dir.z;
    const double tz2 = (box.max.z - ray.origin.z) * inv_dir.z;
    tmin = std::max(tmin, std::min(tz1, tz2));
    tmax = std::min(tmax, std::max(tz1, tz2));

    return tmax >= std::max(0.0, tmin) && tmin <= max_t;
}

/**
 * @brief Möller-Trumbore ray-triangle intersection returning distance t and barycentric (u, v).
 */
[[nodiscard]] inline bool intersect_ray_triangle_mt(
    const Ray3D& ray,
    const Point3D& v0,
    const Point3D& v1,
    const Point3D& v2,
    double max_t,
    double& out_t,
    double& out_u,
    double& out_v
) noexcept {
    const Vec3d e1 = v1 - v0;
    const Vec3d e2 = v2 - v0;
    const Vec3d pvec = ray.direction.cross(e2);
    const double det = e1.dot(pvec);
    if (std::abs(det) <= 1e-12) return false;

    const double inv_det = 1.0 / det;
    const Vec3d tvec = ray.origin - v0;
    const double u = tvec.dot(pvec) * inv_det;
    if (u < -1e-9 || u > 1.0 + 1e-9) return false;

    const Vec3d qvec = tvec.cross(e1);
    const double v = ray.direction.dot(qvec) * inv_det;
    if (v < -1e-9 || (u + v) > 1.0 + 1e-9) return false;

    const double t = e2.dot(qvec) * inv_det;
    if (t <= 1e-9 || t >= max_t) return false;

    out_t = t;
    out_u = u;
    out_v = v;
    return true;
}

} // namespace shunya::cad3d
