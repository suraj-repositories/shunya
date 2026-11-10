#include "shunya/cad2d/curve_2d.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace shunya::cad2d {

// =============================================================================
// Curve2D Base & Gauss-Legendre Quadrature
// =============================================================================

Vec2d Curve2D::tangent(double t) const {
    return derivative(t).normalized();
}

double Curve2D::curvature(double t) const {
    Vec2d d1 = derivative(t);
    Vec2d d2 = second_derivative(t);
    double speed_sq = d1.length_squared();
    if (speed_sq < 1e-24) return 0.0;
    double denom = speed_sq * std::sqrt(speed_sq);
    return std::abs(cross2d(d1, d2)) / denom;
}

std::vector<Point2D> Curve2D::sample_polyline(size_t segments) const {
    size_t n = std::max<size_t>(1, segments);
    std::vector<Point2D> pts;
    pts.reserve(n + 1);
    for (size_t i = 0; i <= n; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(n);
        pts.push_back(evaluate(t));
    }
    return pts;
}

double gauss_legendre_arc_length(const Curve2D& curve, double t0, double t1, size_t sub_intervals) {
    if (t1 < t0) std::swap(t0, t1);
    t0 = std::clamp(t0, 0.0, 1.0);
    t1 = std::clamp(t1, 0.0, 1.0);
    if (t1 - t0 <= 1e-15) return 0.0;

    static constexpr std::array<double, 8> kNodes = {
        0.095012509837637440185,
        0.281603550779258913230,
        0.458016777657227386342,
        0.617876244402643748447,
        0.755404408355003033895,
        0.865631202387831743880,
        0.944575023073232576078,
        0.989400934991649932596
    };
    static constexpr std::array<double, 8> kWeights = {
        0.189450610455068496285,
        0.182603415044923588867,
        0.169156519395002538189,
        0.149595988816576732081,
        0.124628971255533872052,
        0.095158511682492784810,
        0.062253523938647892863,
        0.027152459411754094852
    };

    size_t subs = std::max<size_t>(1, sub_intervals);
    double h = (t1 - t0) / static_cast<double>(subs);
    double total = 0.0;

    for (size_t s = 0; s < subs; ++s) {
        double a = t0 + static_cast<double>(s) * h;
        double b = a + h;
        double mid = 0.5 * (a + b);
        double half = 0.5 * (b - a);
        double sum = 0.0;
        for (size_t k = 0; k < 8; ++k) {
            double dx = half * kNodes[k];
            sum += kWeights[k] * (curve.derivative(mid - dx).length() + curve.derivative(mid + dx).length());
        }
        total += half * sum;
    }
    return total;
}

// =============================================================================
// LineSegment2D
// =============================================================================

Point2D LineSegment2D::evaluate(double t) const {
    return p0 + (p1 - p0) * std::clamp(t, 0.0, 1.0);
}

Vec2d LineSegment2D::derivative(double t) const {
    (void)t;
    return p1 - p0;
}

Vec2d LineSegment2D::second_derivative(double t) const {
    (void)t;
    return {0.0, 0.0};
}

double LineSegment2D::total_length() const {
    return (p1 - p0).length();
}

double LineSegment2D::arc_length(double t0, double t1) const {
    t0 = std::clamp(t0, 0.0, 1.0);
    t1 = std::clamp(t1, 0.0, 1.0);
    return std::abs(t1 - t0) * total_length();
}

BBox2D LineSegment2D::bounding_box() const {
    BBox2D box;
    box.expand(p0);
    box.expand(p1);
    return box;
}

Point2D LineSegment2D::closest_point(const Point2D& query, double* out_t) const {
    Vec2d d = p1 - p0;
    double len_sq = d.length_squared();
    double t = 0.0;
    if (len_sq > 1e-24) {
        t = std::clamp((query - p0).dot(d) / len_sq, 0.0, 1.0);
    }
    if (out_t) *out_t = t;
    return p0 + d * t;
}

std::vector<Point2D> LineSegment2D::sample_polyline(size_t segments) const {
    (void)segments;
    return {p0, p1};
}

std::unique_ptr<Curve2D> LineSegment2D::clone() const {
    return std::make_unique<LineSegment2D>(*this);
}

// =============================================================================
// Polyline2D
// =============================================================================

namespace {

std::vector<double> compute_cumulative_lengths(const std::vector<Point2D>& pts, bool closed) {
    std::vector<double> cum;
    if (pts.size() < 2) return cum;
    cum.reserve(pts.size() + (closed ? 1 : 0));
    cum.push_back(0.0);
    double acc = 0.0;
    for (size_t i = 1; i < pts.size(); ++i) {
        acc += (pts[i] - pts[i - 1]).length();
        cum.push_back(acc);
    }
    if (closed) {
        acc += (pts.front() - pts.back()).length();
        cum.push_back(acc);
    }
    return cum;
}

} // namespace

Point2D Polyline2D::evaluate(double t) const {
    if (points.empty()) return {0.0, 0.0};
    if (points.size() == 1) return points.front();
    t = std::clamp(t, 0.0, 1.0);

    auto cum = compute_cumulative_lengths(points, closed);
    double total = cum.back();
    if (total <= 1e-15) return points.front();

    double target = t * total;
    size_t seg_count = cum.size() - 1;
    for (size_t i = 0; i < seg_count; ++i) {
        if (target <= cum[i + 1] || i + 1 == seg_count) {
            double seg_len = cum[i + 1] - cum[i];
            double local_t = seg_len > 1e-15 ? (target - cum[i]) / seg_len : 0.0;
            const Point2D& a = points[i];
            const Point2D& b = (i + 1 < points.size()) ? points[i + 1] : points.front();
            return a + (b - a) * std::clamp(local_t, 0.0, 1.0);
        }
    }
    return closed ? points.front() : points.back();
}

Vec2d Polyline2D::derivative(double t) const {
    if (points.size() < 2) return {0.0, 0.0};
    t = std::clamp(t, 0.0, 1.0);

    auto cum = compute_cumulative_lengths(points, closed);
    double total = cum.back();
    if (total <= 1e-15) return {0.0, 0.0};

    double target = t * total;
    size_t seg_count = cum.size() - 1;
    for (size_t i = 0; i < seg_count; ++i) {
        if (target <= cum[i + 1] || i + 1 == seg_count) {
            const Point2D& a = points[i];
            const Point2D& b = (i + 1 < points.size()) ? points[i + 1] : points.front();
            Vec2d dir = (b - a).normalized();
            return dir * total;
        }
    }
    return {0.0, 0.0};
}

double Polyline2D::total_length() const {
    auto cum = compute_cumulative_lengths(points, closed);
    return cum.empty() ? 0.0 : cum.back();
}

double Polyline2D::arc_length(double t0, double t1) const {
    t0 = std::clamp(t0, 0.0, 1.0);
    t1 = std::clamp(t1, 0.0, 1.0);
    return std::abs(t1 - t0) * total_length();
}

BBox2D Polyline2D::bounding_box() const {
    BBox2D box;
    for (const auto& p : points) box.expand(p);
    return box;
}

Point2D Polyline2D::closest_point(const Point2D& query, double* out_t) const {
    if (points.empty()) {
        if (out_t) *out_t = 0.0;
        return {0.0, 0.0};
    }
    if (points.size() == 1) {
        if (out_t) *out_t = 0.0;
        return points.front();
    }

    auto cum = compute_cumulative_lengths(points, closed);
    double total = cum.back();
    size_t seg_count = cum.size() - 1;

    double best_dist_sq = std::numeric_limits<double>::infinity();
    Point2D best_pt = points.front();
    double best_t = 0.0;

    for (size_t i = 0; i < seg_count; ++i) {
        const Point2D& a = points[i];
        const Point2D& b = (i + 1 < points.size()) ? points[i + 1] : points.front();
        LineSegment2D seg(a, b);
        double local_t = 0.0;
        Point2D p = seg.closest_point(query, &local_t);
        double d_sq = (query - p).length_squared();
        if (d_sq < best_dist_sq) {
            best_dist_sq = d_sq;
            best_pt = p;
            if (total > 1e-15) {
                double seg_len = cum[i + 1] - cum[i];
                best_t = (cum[i] + local_t * seg_len) / total;
            }
        }
    }

    if (out_t) *out_t = best_t;
    return best_pt;
}

std::vector<Point2D> Polyline2D::sample_polyline(size_t segments) const {
    (void)segments;
    if (!closed || points.size() < 2) return points;
    std::vector<Point2D> out = points;
    out.push_back(points.front());
    return out;
}

std::unique_ptr<Curve2D> Polyline2D::clone() const {
    return std::make_unique<Polyline2D>(*this);
}

// =============================================================================
// Arc2D
// =============================================================================

namespace {

double normalize_angle_0_2pi(double a) noexcept {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    a = std::fmod(a, kTwoPi);
    if (a < 0.0) a += kTwoPi;
    return a;
}

bool angle_in_arc_sweep(double angle, double start, double sweep) noexcept {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    if (std::abs(sweep) >= kTwoPi - 1e-12) return true;
    double rel = normalize_angle_0_2pi(angle - start);
    if (sweep >= 0.0) {
        return rel <= sweep + 1e-12;
    } else {
        double rel_neg = normalize_angle_0_2pi(start - angle);
        return rel_neg <= (-sweep) + 1e-12;
    }
}

} // namespace

Point2D Arc2D::evaluate(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    double theta = start_angle + t * sweep_angle;
    return {center.x + radius * std::cos(theta), center.y + radius * std::sin(theta)};
}

Vec2d Arc2D::derivative(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    double theta = start_angle + t * sweep_angle;
    return {-radius * sweep_angle * std::sin(theta), radius * sweep_angle * std::cos(theta)};
}

Vec2d Arc2D::second_derivative(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    double theta = start_angle + t * sweep_angle;
    double s2 = sweep_angle * sweep_angle;
    return {-radius * s2 * std::cos(theta), -radius * s2 * std::sin(theta)};
}

double Arc2D::curvature(double t) const noexcept {
    (void)t;
    return radius > 1e-15 ? (1.0 / radius) : 0.0;
}

double Arc2D::total_length() const {
    return radius * std::abs(sweep_angle);
}

double Arc2D::arc_length(double t0, double t1) const {
    t0 = std::clamp(t0, 0.0, 1.0);
    t1 = std::clamp(t1, 0.0, 1.0);
    return std::abs(t1 - t0) * total_length();
}

BBox2D Arc2D::bounding_box() const {
    BBox2D box;
    box.expand(evaluate(0.0));
    box.expand(evaluate(1.0));

    constexpr double kHalfPi = 0.5 * core::base::math::PI;
    const std::array<double, 4> kCardinals = {0.0, kHalfPi, core::base::math::PI, 3.0 * kHalfPi};
    for (double a : kCardinals) {
        if (angle_in_arc_sweep(a, start_angle, sweep_angle)) {
            box.expand({center.x + radius * std::cos(a), center.y + radius * std::sin(a)});
        }
    }
    return box;
}

Point2D Arc2D::closest_point(const Point2D& query, double* out_t) const {
    if (std::abs(sweep_angle) < 1e-15 || radius < 1e-15) {
        if (out_t) *out_t = 0.0;
        return evaluate(0.0);
    }

    Vec2d rel = query - center;
    if (rel.length_squared() > 1e-24) {
        double angle = std::atan2(rel.y, rel.x);
        if (angle_in_arc_sweep(angle, start_angle, sweep_angle)) {
            double d_ang = (sweep_angle >= 0.0)
                ? normalize_angle_0_2pi(angle - start_angle)
                : -normalize_angle_0_2pi(start_angle - angle);
            double t = std::clamp(d_ang / sweep_angle, 0.0, 1.0);
            if (out_t) *out_t = t;
            return evaluate(t);
        }
    }

    Point2D p_start = evaluate(0.0);
    Point2D p_end = evaluate(1.0);
    if ((query - p_start).length_squared() <= (query - p_end).length_squared()) {
        if (out_t) *out_t = 0.0;
        return p_start;
    } else {
        if (out_t) *out_t = 1.0;
        return p_end;
    }
}

std::unique_ptr<Curve2D> Arc2D::clone() const {
    return std::make_unique<Arc2D>(*this);
}

// =============================================================================
// Circle2D
// =============================================================================

Point2D Circle2D::evaluate(double t) const {
    double theta = 2.0 * core::base::math::PI * std::clamp(t, 0.0, 1.0);
    return {center.x + radius * std::cos(theta), center.y + radius * std::sin(theta)};
}

Vec2d Circle2D::derivative(double t) const {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    double theta = kTwoPi * std::clamp(t, 0.0, 1.0);
    return {-radius * kTwoPi * std::sin(theta), radius * kTwoPi * std::cos(theta)};
}

Vec2d Circle2D::second_derivative(double t) const {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    double theta = kTwoPi * std::clamp(t, 0.0, 1.0);
    return {-radius * kTwoPi * kTwoPi * std::cos(theta), -radius * kTwoPi * kTwoPi * std::sin(theta)};
}

double Circle2D::curvature(double t) const noexcept {
    (void)t;
    return radius > 1e-15 ? (1.0 / radius) : 0.0;
}

double Circle2D::total_length() const {
    return 2.0 * core::base::math::PI * radius;
}

double Circle2D::arc_length(double t0, double t1) const {
    t0 = std::clamp(t0, 0.0, 1.0);
    t1 = std::clamp(t1, 0.0, 1.0);
    return std::abs(t1 - t0) * total_length();
}

BBox2D Circle2D::bounding_box() const {
    return {{center.x - radius, center.y - radius}, {center.x + radius, center.y + radius}};
}

Point2D Circle2D::closest_point(const Point2D& query, double* out_t) const {
    Vec2d rel = query - center;
    double angle = (rel.length_squared() > 1e-24) ? std::atan2(rel.y, rel.x) : 0.0;
    double t = normalize_angle_0_2pi(angle) / (2.0 * core::base::math::PI);
    if (out_t) *out_t = t;
    return evaluate(t);
}

std::unique_ptr<Curve2D> Circle2D::clone() const {
    return std::make_unique<Circle2D>(*this);
}

// =============================================================================
// Ellipse2D
// =============================================================================

Point2D Ellipse2D::evaluate(double t) const {
    double theta = 2.0 * core::base::math::PI * std::clamp(t, 0.0, 1.0);
    double lx = rx * std::cos(theta);
    double ly = ry * std::sin(theta);
    double c = std::cos(rotation);
    double s = std::sin(rotation);
    return {center.x + lx * c - ly * s, center.y + lx * s + ly * c};
}

Vec2d Ellipse2D::derivative(double t) const {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    double theta = kTwoPi * std::clamp(t, 0.0, 1.0);
    double dlx = -rx * kTwoPi * std::sin(theta);
    double dly = ry * kTwoPi * std::cos(theta);
    double c = std::cos(rotation);
    double s = std::sin(rotation);
    return {dlx * c - dly * s, dlx * s + dly * c};
}

Vec2d Ellipse2D::second_derivative(double t) const {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    double theta = kTwoPi * std::clamp(t, 0.0, 1.0);
    double ddlx = -rx * kTwoPi * kTwoPi * std::cos(theta);
    double ddly = -ry * kTwoPi * kTwoPi * std::sin(theta);
    double c = std::cos(rotation);
    double s = std::sin(rotation);
    return {ddlx * c - ddly * s, ddlx * s + ddly * c};
}

double Ellipse2D::total_length() const {
    return arc_length(0.0, 1.0);
}

double Ellipse2D::arc_length(double t0, double t1) const {
    return gauss_legendre_arc_length(*this, t0, t1, 16);
}

double Ellipse2D::ramanujan_perimeter() const noexcept {
    double a = rx;
    double b = ry;
    if (a + b <= 1e-15) return 0.0;
    double h = std::pow(a - b, 2) / std::pow(a + b, 2);
    return core::base::math::PI * (a + b) * (1.0 + (3.0 * h) / (10.0 + std::sqrt(std::max(0.0, 4.0 - 3.0 * h))));
}

BBox2D Ellipse2D::bounding_box() const {
    double c = std::cos(rotation);
    double s = std::sin(rotation);
    double half_w = std::hypot(rx * c, ry * s);
    double half_h = std::hypot(rx * s, ry * c);
    return {{center.x - half_w, center.y - half_h}, {center.x + half_w, center.y + half_h}};
}

Point2D Ellipse2D::closest_point(const Point2D& query, double* out_t) const {
    // Coarse sampling + Newton-Raphson refinement on distance squared F(t) = 0.5 * |E(t) - Q|^2
    const size_t kSeeds = 64;
    double best_t = 0.0;
    double best_d_sq = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < kSeeds; ++i) {
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
        double step = g / h;
        t = std::fmod(t - step, 1.0);
        if (t < 0.0) t += 1.0;
        if (std::abs(step) < 1e-13) break;
    }

    if (out_t) *out_t = t;
    return evaluate(t);
}

std::unique_ptr<Curve2D> Ellipse2D::clone() const {
    return std::make_unique<Ellipse2D>(*this);
}

} // namespace shunya::cad2d
