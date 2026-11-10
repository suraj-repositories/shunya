#include "shunya/cad3d/revolve_feature.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace shunya::cad3d {

namespace {

Point3D rotate_around_axis(
    const Point3D& pt,
    const Point3D& axis_origin,
    const Vec3d& unit_axis,
    double angle_rad
) noexcept {
    const Vec3d p = pt - axis_origin;
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    const double dot_val = unit_axis.dot(p);
    const Vec3d rotated = p * c + unit_axis.cross(p) * s + unit_axis * (dot_val * (1.0 - c));
    return axis_origin + rotated;
}

cad2d::Path2D clean_loop_for_revolve(const cad2d::Path2D& in, bool want_ccw) {
    cad2d::Path2D clean;
    clean.reserve(in.size());
    for (const auto& pt : in) {
        if (clean.empty() || !clean.back().approx_equals(pt, 1e-10)) {
            clean.push_back(pt);
        }
    }
    if (clean.size() >= 2 && clean.front().approx_equals(clean.back(), 1e-10)) {
        clean.pop_back();
    }
    if (clean.size() < 3) return {};
    const double area = cad2d::PolygonBooleanEngine::signed_area(clean);
    if ((want_ccw && area < 0.0) || (!want_ccw && area > 0.0)) {
        std::reverse(clean.begin(), clean.end());
    }
    return clean;
}

} // namespace

SolidMesh3D RevolveEngine::revolve(
    const cad2d::Path2D& outer,
    const cad2d::Paths2D& holes,
    const RevolveParams& params
) {
    SolidMesh3D mesh;
    const cad2d::Path2D clean_outer = clean_loop_for_revolve(outer, true);
    if (clean_outer.size() < 3) return mesh;

    cad2d::Paths2D clean_holes;
    for (const auto& h : holes) {
        auto ch = clean_loop_for_revolve(h, false);
        if (ch.size() >= 3) clean_holes.push_back(std::move(ch));
    }

    const Vec3d axis_dir = params.axis.direction.normalized();
    if (axis_dir.length_squared() <= 1e-12) return mesh;

    const double clamped_angle = std::clamp(
        std::abs(params.angle_rad),
        1e-6,
        2.0 * core::base::math::PI
    );
    const bool is_full = std::abs(clamped_angle - 2.0 * core::base::math::PI) <= 1e-7;
    const size_t segs = std::max<size_t>(3, params.angular_segments);

    const TessellatedPolygon2D tess = PolygonTessellator::triangulate(clean_outer, clean_holes);
    if (tess.vertices.empty() || tess.triangles.empty()) return mesh;

    // Precompute profile 3D points on initial slice
    const size_t n_pts = tess.vertices.size();
    std::vector<Point3D> profile_3d;
    profile_3d.reserve(n_pts);
    for (const auto& p2 : tess.vertices) {
        profile_3d.push_back(params.plane.to_3d(p2, 0.0));
    }

    const size_t slice_count = is_full ? segs : (segs + 1);
    const double signed_angle = (params.angle_rad >= 0.0) ? clamped_angle : -clamped_angle;

    // Determine whether initial revolution velocity aligns with +plane.normal() so caps & sides match 100% consistently
    Point3D centroid_3d{0.0, 0.0, 0.0};
    for (const auto& p : profile_3d) centroid_3d += p;
    centroid_3d = centroid_3d * (1.0 / static_cast<double>(profile_3d.size()));
    const Vec3d init_sweep_vel = axis_dir.cross(centroid_3d - params.axis.origin) * (signed_angle >= 0.0 ? 1.0 : -1.0);
    const bool forward_matches_normal = params.plane.normal().dot(init_sweep_vel) >= 0.0;

    mesh.vertices.reserve(slice_count * n_pts);
    for (size_t s = 0; s < slice_count; ++s) {
        const double theta = signed_angle * (static_cast<double>(s) / static_cast<double>(segs));
        for (size_t i = 0; i < n_pts; ++i) {
            mesh.add_vertex(rotate_around_axis(profile_3d[i], params.axis.origin, axis_dir, theta));
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> loop_spans;
    uint32_t cursor = 0;
    for (const uint32_t count : tess.loop_sizes) {
        loop_spans.push_back({cursor, count});
        cursor += count;
    }

    const size_t quad_steps = is_full ? segs : segs;
    for (size_t s = 0; s < quad_steps; ++s) {
        const uint32_t base0 = static_cast<uint32_t>(s * n_pts);
        const uint32_t base1 = static_cast<uint32_t>(((s + 1) % slice_count) * n_pts);
        for (const auto& [offset, count] : loop_spans) {
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t a0 = base0 + offset + i;
                const uint32_t b0 = base0 + offset + ((i + 1) % count);
                const uint32_t a1 = base1 + offset + i;
                const uint32_t b1 = base1 + offset + ((i + 1) % count);
                if (forward_matches_normal) {
                    mesh.add_triangle(a0, b0, b1);
                    mesh.add_triangle(a0, b1, a1);
                } else {
                    mesh.add_triangle(a0, b1, b0);
                    mesh.add_triangle(a0, a1, b1);
                }
            }
        }
    }

    if (!is_full) {
        const uint32_t end_base = static_cast<uint32_t>(segs * n_pts);
        for (const auto& tri : tess.triangles) {
            if (forward_matches_normal) {
                mesh.add_triangle(tri[0], tri[2], tri[1]);
                mesh.add_triangle(end_base + tri[0], end_base + tri[1], end_base + tri[2]);
            } else {
                mesh.add_triangle(tri[0], tri[1], tri[2]);
                mesh.add_triangle(end_base + tri[0], end_base + tri[2], end_base + tri[1]);
            }
        }
    }

    // Weld degenerate pole vertices (r == 0) and seam, drop zero-area triangles, ensure outward winding
    mesh.weld_vertices(1e-7);
    mesh.ensure_outward_winding();
    return mesh;
}

} // namespace shunya::cad3d
