#include "shunya/cad2d/fillet_chamfer.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

core::base::Result<FilletCornerResult> FilletChamferEngine::fillet_corner(
    const Point2D& a,
    const Point2D& vertex,
    const Point2D& b,
    double radius
) {
    if (radius <= 1e-12) {
        return core::base::Result<FilletCornerResult>::err({
            .code = 4101,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Fillet radius must be strictly positive",
            .details = ""
        });
    }

    Vec2d va = a - vertex;
    Vec2d vb = b - vertex;
    double len_a = va.length();
    double len_b = vb.length();
    if (len_a <= 1e-12 || len_b <= 1e-12) {
        return core::base::Result<FilletCornerResult>::err({
            .code = 4102,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Degenerate zero-length leg in fillet_corner",
            .details = ""
        });
    }

    Vec2d u = va / len_a;
    Vec2d v = vb / len_b;
    double c = cross2d(u, v);
    double d = std::clamp(u.dot(v), -1.0, 1.0);

    // Parallel or anti-parallel legs cannot be filleted with a finite arc
    if (std::abs(c) <= 1e-9) {
        return core::base::Result<FilletCornerResult>::err({
            .code = 4103,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Collinear or parallel segments cannot be filleted",
            .details = ""
        });
    }

    double alpha = std::acos(d); // Interior angle between u and v in (0, pi)
    double half_alpha = alpha * 0.5;
    double tan_half = std::tan(half_alpha);
    double sin_half = std::sin(half_alpha);
    if (tan_half <= 1e-12 || sin_half <= 1e-12) {
        return core::base::Result<FilletCornerResult>::err({
            .code = 4103,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Corner angle too narrow for fillet",
            .details = ""
        });
    }

    double trim_dist = radius / tan_half;
    if (trim_dist > len_a + 1e-9 || trim_dist > len_b + 1e-9) {
        return core::base::Result<FilletCornerResult>::err({
            .code = 4104,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Fillet radius exceeds available segment length",
            .details = ""
        });
    }

    Point2D t1 = vertex + u * trim_dist;
    Point2D t2 = vertex + v * trim_dist;

    Vec2d bisector = (u + v).normalized();
    Point2D center = vertex + bisector * (radius / sin_half);

    double start_ang = std::atan2(t1.y - center.y, t1.x - center.x);
    double end_ang = std::atan2(t2.y - center.y, t2.x - center.x);
    double sweep = end_ang - start_ang;
    constexpr double kPi = core::base::math::PI;
    while (sweep > kPi) sweep -= 2.0 * kPi;
    while (sweep <= -kPi) sweep += 2.0 * kPi;

    FilletCornerResult out;
    out.trimmed_leg1 = LineSegment2D(a, t1);
    out.arc = Arc2D(center, radius, start_ang, sweep);
    out.trimmed_leg2 = LineSegment2D(t2, b);
    out.trim_distance = trim_dist;
    return core::base::Result<FilletCornerResult>::ok(std::move(out));
}

core::base::Result<ChamferCornerResult> FilletChamferEngine::chamfer_corner(
    const Point2D& a,
    const Point2D& vertex,
    const Point2D& b,
    double dist_a,
    double dist_b
) {
    if (dist_a <= 1e-12 || dist_b <= 1e-12) {
        return core::base::Result<ChamferCornerResult>::err({
            .code = 4105,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Chamfer setback distances must be positive",
            .details = ""
        });
    }

    Vec2d va = a - vertex;
    Vec2d vb = b - vertex;
    double len_a = va.length();
    double len_b = vb.length();
    if (dist_a > len_a + 1e-9 || dist_b > len_b + 1e-9) {
        return core::base::Result<ChamferCornerResult>::err({
            .code = 4106,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Chamfer setback exceeds available segment length",
            .details = ""
        });
    }

    Point2D p1 = vertex + (va / len_a) * dist_a;
    Point2D p2 = vertex + (vb / len_b) * dist_b;

    ChamferCornerResult out;
    out.trimmed_leg1 = LineSegment2D(a, p1);
    out.bevel = LineSegment2D(p1, p2);
    out.trimmed_leg2 = LineSegment2D(p2, b);
    return core::base::Result<ChamferCornerResult>::ok(std::move(out));
}

Path2D FilletChamferEngine::fillet_polygon(
    const Path2D& polygon,
    double radius,
    size_t arc_segments
) {
    if (polygon.size() < 3 || radius <= 1e-12) return polygon;

    Path2D out;
    size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const Point2D& prev = polygon[(i + n - 1) % n];
        const Point2D& curr = polygon[i];
        const Point2D& next = polygon[(i + 1) % n];

        Point2D mid_prev = (prev + curr) * 0.5;
        Point2D mid_next = (curr + next) * 0.5;
        auto res = fillet_corner(mid_prev, curr, mid_next, radius);
        if (res.is_ok()) {
            auto arc_pts = res.value().arc.sample_polyline(std::max<size_t>(2, arc_segments));
            for (const auto& p : arc_pts) {
                if (out.empty() || !out.back().approx_equals(p, 1e-9)) {
                    out.push_back(p);
                }
            }
        } else {
            if (out.empty() || !out.back().approx_equals(curr, 1e-9)) {
                out.push_back(curr);
            }
        }
    }
    if (out.size() >= 2 && out.front().approx_equals(out.back(), 1e-9)) {
        out.pop_back();
    }
    return out;
}

} // namespace shunya::cad2d
