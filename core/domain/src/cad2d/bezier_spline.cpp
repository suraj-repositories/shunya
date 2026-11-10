#include "shunya/cad2d/bezier_spline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace shunya::cad2d {

Point2D BezierCubic2D::evaluate(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    double u = 1.0 - t;
    double uu = u * u;
    double tt = t * t;
    return p0 * (uu * u) + p1 * (3.0 * uu * t) + p2 * (3.0 * u * tt) + p3 * (tt * t);
}

Vec2d BezierCubic2D::derivative(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    double u = 1.0 - t;
    return (p1 - p0) * (3.0 * u * u) + (p2 - p1) * (6.0 * u * t) + (p3 - p2) * (3.0 * t * t);
}

Vec2d BezierCubic2D::second_derivative(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    double u = 1.0 - t;
    return (p2 - p1 * 2.0 + p0) * (6.0 * u) + (p3 - p2 * 2.0 + p1) * (6.0 * t);
}

double BezierCubic2D::total_length() const {
    return arc_length(0.0, 1.0);
}

double BezierCubic2D::arc_length(double t0, double t1) const {
    return gauss_legendre_arc_length(*this, t0, t1, 8);
}

BBox2D BezierCubic2D::control_polygon_bbox() const noexcept {
    BBox2D box;
    box.expand(p0);
    box.expand(p1);
    box.expand(p2);
    box.expand(p3);
    return box;
}

namespace {

void check_quadratic_extrema_1d(double c0, double c1, double c2, double c3, double& out_min, double& out_max) {
    // Derivative coefficients: 3*(a*t^2 + b*t + c) where
    // a = -c0 + 3*c1 - 3*c2 + c3
    // b = 2*(c0 - 2*c1 + c2)
    // c = c1 - c0
    double a = -c0 + 3.0 * c1 - 3.0 * c2 + c3;
    double b = 2.0 * (c0 - 2.0 * c1 + c2);
    double c = c1 - c0;

    auto eval_coord = [&](double t) {
        double u = 1.0 - t;
        double v = u*u*u*c0 + 3.0*u*u*t*c1 + 3.0*u*t*t*c2 + t*t*t*c3;
        if (v < out_min) out_min = v;
        if (v > out_max) out_max = v;
    };

    if (std::abs(a) < 1e-14) {
        if (std::abs(b) > 1e-14) {
            double t = -c / b;
            if (t > 0.0 && t < 1.0) eval_coord(t);
        }
        return;
    }

    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return;
    double sqrt_d = std::sqrt(disc);
    double t1 = (-b - sqrt_d) / (2.0 * a);
    double t2 = (-b + sqrt_d) / (2.0 * a);
    if (t1 > 0.0 && t1 < 1.0) eval_coord(t1);
    if (t2 > 0.0 && t2 < 1.0) eval_coord(t2);
}

} // namespace

BBox2D BezierCubic2D::bounding_box() const {
    BBox2D box;
    box.expand(p0);
    box.expand(p3);

    check_quadratic_extrema_1d(p0.x, p1.x, p2.x, p3.x, box.min.x, box.max.x);
    check_quadratic_extrema_1d(p0.y, p1.y, p2.y, p3.y, box.min.y, box.max.y);
    return box;
}

Point2D BezierCubic2D::closest_point(const Point2D& query, double* out_t) const {
    const size_t kSeeds = 64;
    double best_t = 0.0;
    double best_d_sq = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i <= kSeeds; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(kSeeds);
        double d_sq = (evaluate(t) - query).length_squared();
        if (d_sq < best_d_sq) {
            best_d_sq = d_sq;
            best_t = t;
        }
    }

    double t = best_t;
    for (int iter = 0; iter < 16; ++iter) {
        Vec2d r = evaluate(t) - query;
        Vec2d d1 = derivative(t);
        Vec2d d2 = second_derivative(t);
        double g = r.dot(d1);
        double h = d1.dot(d1) + r.dot(d2);
        if (std::abs(h) < 1e-18) break;
        double next_t = std::clamp(t - g / h, 0.0, 1.0);
        if (std::abs(next_t - t) < 1e-13) {
            t = next_t;
            break;
        }
        t = next_t;
    }

    // Check endpoints explicitly as well
    Point2D pt = evaluate(t);
    double d_pt = (pt - query).length_squared();
    double d0 = (p0 - query).length_squared();
    double d1 = (p3 - query).length_squared();
    if (d0 < d_pt) { d_pt = d0; t = 0.0; pt = p0; }
    if (d1 < d_pt) { t = 1.0; pt = p3; }

    if (out_t) *out_t = t;
    return pt;
}

std::pair<BezierCubic2D, BezierCubic2D> BezierCubic2D::split(double t) const noexcept {
    t = std::clamp(t, 0.0, 1.0);
    auto lerp_pt = [t](const Point2D& a, const Point2D& b) -> Point2D {
        return a + (b - a) * t;
    };
    Point2D q0 = lerp_pt(p0, p1);
    Point2D q1 = lerp_pt(p1, p2);
    Point2D q2 = lerp_pt(p2, p3);
    Point2D r0 = lerp_pt(q0, q1);
    Point2D r1 = lerp_pt(q1, q2);
    Point2D s0 = lerp_pt(r0, r1);
    return {BezierCubic2D{p0, q0, r0, s0}, BezierCubic2D{s0, r1, q2, p3}};
}

std::unique_ptr<Curve2D> BezierCubic2D::clone() const {
    return std::make_unique<BezierCubic2D>(*this);
}

// =============================================================================
// BezierSpline2D
// =============================================================================

Point2D BezierSpline2D::evaluate(double t) const {
    if (segments.empty()) return {0.0, 0.0};
    t = std::clamp(t, 0.0, 1.0);
    double scaled = t * static_cast<double>(segments.size());
    size_t idx = std::min(segments.size() - 1, static_cast<size_t>(scaled));
    double local_t = scaled - static_cast<double>(idx);
    return segments[idx].evaluate(local_t);
}

double BezierSpline2D::total_length() const {
    double sum = 0.0;
    for (const auto& seg : segments) sum += seg.total_length();
    return sum;
}

BBox2D BezierSpline2D::bounding_box() const {
    BBox2D box;
    for (const auto& seg : segments) box.expand(seg.bounding_box());
    return box;
}

Point2D BezierSpline2D::closest_point(const Point2D& query, double* out_t) const {
    if (segments.empty()) {
        if (out_t) *out_t = 0.0;
        return {0.0, 0.0};
    }
    double best_d_sq = std::numeric_limits<double>::infinity();
    Point2D best_pt{};
    double best_t = 0.0;

    for (size_t i = 0; i < segments.size(); ++i) {
        double local_t = 0.0;
        Point2D pt = segments[i].closest_point(query, &local_t);
        double d_sq = (pt - query).length_squared();
        if (d_sq < best_d_sq) {
            best_d_sq = d_sq;
            best_pt = pt;
            best_t = (static_cast<double>(i) + local_t) / static_cast<double>(segments.size());
        }
    }
    if (out_t) *out_t = best_t;
    return best_pt;
}

std::vector<Point2D> BezierSpline2D::sample_polyline(size_t segments_per_curve) const {
    std::vector<Point2D> out;
    if (segments.empty()) return out;
    for (size_t i = 0; i < segments.size(); ++i) {
        auto pts = segments[i].sample_polyline(segments_per_curve);
        if (i > 0 && !pts.empty()) {
            out.insert(out.end(), pts.begin() + 1, pts.end());
        } else {
            out.insert(out.end(), pts.begin(), pts.end());
        }
    }
    return out;
}

} // namespace shunya::cad2d
