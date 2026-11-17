#include "shunya/cad2d/advanced_curves.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <limits>
#include <algorithm>

namespace shunya::cad2d {

// =============================================================================
// CatmullRomSpline2D
// =============================================================================

CatmullRomSpline2D::CatmullRomSpline2D(std::vector<Point2D> pts, bool is_closed)
    : control_points(std::move(pts)), closed(is_closed) {}

void CatmullRomSpline2D::get_segment_points(size_t seg_idx, Point2D& p0, Point2D& p1, Point2D& p2, Point2D& p3) const {
    size_t n = control_points.size();
    if (n < 2) return;

    if (closed) {
        p0 = control_points[(seg_idx + n - 1) % n];
        p1 = control_points[seg_idx % n];
        p2 = control_points[(seg_idx + 1) % n];
        p3 = control_points[(seg_idx + 2) % n];
    } else {
        p1 = control_points[seg_idx];
        p2 = control_points[seg_idx + 1];
        if (seg_idx == 0) {
            p0 = p1 * 2.0 - p2;
        } else {
            p0 = control_points[seg_idx - 1];
        }
        if (seg_idx + 2 < n) {
            p3 = control_points[seg_idx + 2];
        } else {
            p3 = p2 * 2.0 - p1;
        }
    }
}

Point2D CatmullRomSpline2D::evaluate(double t) const {
    size_t n = control_points.size();
    if (n == 0) return {};
    if (n == 1) return control_points[0];
    if (n == 2) {
        return control_points[0] * (1.0 - t) + control_points[1] * t;
    }

    size_t m = closed ? n : (n - 1);
    double clamped_t = std::clamp(t, 0.0, 1.0);
    double u = clamped_t * static_cast<double>(m);
    size_t i = std::min(static_cast<size_t>(u), m - 1);
    double s = u - static_cast<double>(i);

    Point2D p0{}, p1{}, p2{}, p3{};
    get_segment_points(i, p0, p1, p2, p3);

    double s2 = s * s;
    double s3 = s2 * s;

    return (p1 * 2.0 +
           (p2 - p0) * s +
           (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * s2 +
           (p0 * -1.0 + p1 * 3.0 - p2 * 3.0 + p3) * s3) * 0.5;
}

Vec2d CatmullRomSpline2D::derivative(double t) const {
    size_t n = control_points.size();
    if (n < 2) return {0.0, 0.0};
    size_t m = closed ? n : (n - 1);
    double clamped_t = std::clamp(t, 0.0, 1.0);
    double u = clamped_t * static_cast<double>(m);
    size_t i = std::min(static_cast<size_t>(u), m - 1);
    double s = u - static_cast<double>(i);

    Point2D p0{}, p1{}, p2{}, p3{};
    get_segment_points(i, p0, p1, p2, p3);

    double s2 = s * s;

    Vec2d ds = ((p2 - p0) +
                (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * (2.0 * s) +
                (p0 * -1.0 + p1 * 3.0 - p2 * 3.0 + p3) * (3.0 * s2)) * 0.5;

    return ds * static_cast<double>(m);
}

Vec2d CatmullRomSpline2D::second_derivative(double t) const {
    size_t n = control_points.size();
    if (n < 2) return {0.0, 0.0};
    size_t m = closed ? n : (n - 1);
    double clamped_t = std::clamp(t, 0.0, 1.0);
    double u = clamped_t * static_cast<double>(m);
    size_t i = std::min(static_cast<size_t>(u), m - 1);
    double s = u - static_cast<double>(i);

    Point2D p0{}, p1{}, p2{}, p3{};
    get_segment_points(i, p0, p1, p2, p3);

    Vec2d ds2 = ((p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * 2.0 +
                 (p0 * -1.0 + p1 * 3.0 - p2 * 3.0 + p3) * (6.0 * s)) * 0.5;

    return ds2 * static_cast<double>(m * m);
}

double CatmullRomSpline2D::total_length() const {
    return arc_length(0.0, 1.0);
}

double CatmullRomSpline2D::arc_length(double t0, double t1) const {
    return gauss_legendre_arc_length(*this, t0, t1, 16);
}

BBox2D CatmullRomSpline2D::bounding_box() const {
    BBox2D box;
    const size_t samples = 64;
    for (size_t k = 0; k <= samples; ++k) {
        box.expand(evaluate(static_cast<double>(k) / static_cast<double>(samples)));
    }
    return box;
}

Point2D CatmullRomSpline2D::closest_point(const Point2D& query, double* out_t) const {
    const size_t samples = 100;
    double best_dist_sq = std::numeric_limits<double>::infinity();
    double best_t = 0.0;
    Point2D best_pt{};

    for (size_t k = 0; k <= samples; ++k) {
        double t = static_cast<double>(k) / static_cast<double>(samples);
        Point2D p = evaluate(t);
        double d_sq = (p.x - query.x) * (p.x - query.x) + (p.y - query.y) * (p.y - query.y);
        if (d_sq < best_dist_sq) {
            best_dist_sq = d_sq;
            best_t = t;
            best_pt = p;
        }
    }
    if (out_t) *out_t = best_t;
    return best_pt;
}

std::unique_ptr<Curve2D> CatmullRomSpline2D::clone() const {
    return std::make_unique<CatmullRomSpline2D>(*this);
}

// =============================================================================
// BSpline2D
// =============================================================================

BSpline2D::BSpline2D(std::vector<Point2D> pts, bool is_closed)
    : control_points(std::move(pts)), closed(is_closed) {}

void BSpline2D::get_segment_points(size_t seg_idx, Point2D& p0, Point2D& p1, Point2D& p2, Point2D& p3) const {
    size_t n = control_points.size();
    if (closed) {
        p0 = control_points[seg_idx % n];
        p1 = control_points[(seg_idx + 1) % n];
        p2 = control_points[(seg_idx + 2) % n];
        p3 = control_points[(seg_idx + 3) % n];
    } else {
        p0 = control_points[seg_idx];
        p1 = control_points[seg_idx + 1];
        p2 = control_points[seg_idx + 2];
        p3 = control_points[seg_idx + 3];
    }
}

Point2D BSpline2D::evaluate(double t) const {
    size_t n = control_points.size();
    if (n < 4) return {};
    size_t m = closed ? n : (n - 3);

    double clamped_t = std::clamp(t, 0.0, 1.0);
    double u = clamped_t * static_cast<double>(m);
    size_t i = std::min(static_cast<size_t>(u), m - 1);
    double s = u - static_cast<double>(i);

    Point2D p0{}, p1{}, p2{}, p3{};
    get_segment_points(i, p0, p1, p2, p3);

    double one_minus_s = 1.0 - s;
    double b0 = one_minus_s * one_minus_s * one_minus_s;
    double b1 = 3.0 * s * s * s - 6.0 * s * s + 4.0;
    double b2 = -3.0 * s * s * s + 3.0 * s * s + 3.0 * s + 1.0;
    double b3 = s * s * s;

    return (p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3) * (1.0 / 6.0);
}

Vec2d BSpline2D::derivative(double t) const {
    size_t n = control_points.size();
    if (n < 4) return {0.0, 0.0};
    size_t m = closed ? n : (n - 3);

    double clamped_t = std::clamp(t, 0.0, 1.0);
    double u = clamped_t * static_cast<double>(m);
    size_t i = std::min(static_cast<size_t>(u), m - 1);
    double s = u - static_cast<double>(i);

    Point2D p0{}, p1{}, p2{}, p3{};
    get_segment_points(i, p0, p1, p2, p3);

    double one_minus_s = 1.0 - s;
    double db0 = -3.0 * one_minus_s * one_minus_s;
    double db1 = 9.0 * s * s - 12.0 * s;
    double db2 = -9.0 * s * s + 6.0 * s + 3.0;
    double db3 = 3.0 * s * s;

    Vec2d ds = (p0 * db0 + p1 * db1 + p2 * db2 + p3 * db3) * (1.0 / 6.0);
    return ds * static_cast<double>(m);
}

Vec2d BSpline2D::second_derivative(double t) const {
    size_t n = control_points.size();
    if (n < 4) return {0.0, 0.0};
    size_t m = closed ? n : (n - 3);

    double clamped_t = std::clamp(t, 0.0, 1.0);
    double u = clamped_t * static_cast<double>(m);
    size_t i = std::min(static_cast<size_t>(u), m - 1);
    double s = u - static_cast<double>(i);

    Point2D p0{}, p1{}, p2{}, p3{};
    get_segment_points(i, p0, p1, p2, p3);

    double d2b0 = 6.0 * (1.0 - s);
    double d2b1 = 18.0 * s - 12.0;
    double d2b2 = -18.0 * s + 6.0;
    double d2b3 = 6.0 * s;

    Vec2d d2s = (p0 * d2b0 + p1 * d2b1 + p2 * d2b2 + p3 * d2b3) * (1.0 / 6.0);
    return d2s * static_cast<double>(m * m);
}

double BSpline2D::total_length() const {
    return arc_length(0.0, 1.0);
}

double BSpline2D::arc_length(double t0, double t1) const {
    return gauss_legendre_arc_length(*this, t0, t1, 16);
}

BBox2D BSpline2D::bounding_box() const {
    BBox2D box;
    const size_t samples = 64;
    for (size_t k = 0; k <= samples; ++k) {
        box.expand(evaluate(static_cast<double>(k) / static_cast<double>(samples)));
    }
    return box;
}

Point2D BSpline2D::closest_point(const Point2D& query, double* out_t) const {
    const size_t samples = 100;
    double best_dist_sq = std::numeric_limits<double>::infinity();
    double best_t = 0.0;
    Point2D best_pt{};

    for (size_t k = 0; k <= samples; ++k) {
        double t = static_cast<double>(k) / static_cast<double>(samples);
        Point2D p = evaluate(t);
        double d_sq = (p.x - query.x) * (p.x - query.x) + (p.y - query.y) * (p.y - query.y);
        if (d_sq < best_dist_sq) {
            best_dist_sq = d_sq;
            best_t = t;
            best_pt = p;
        }
    }
    if (out_t) *out_t = best_t;
    return best_pt;
}

std::unique_ptr<Curve2D> BSpline2D::clone() const {
    return std::make_unique<BSpline2D>(*this);
}

// =============================================================================
// ParabolicArc2D
// =============================================================================

std::unique_ptr<ParabolicArc2D> ParabolicArc2D::from_three_points(
    const Point2D& start,
    const Point2D& apex,
    const Point2D& end
) {
    // Parabolic arc with apex as vertex, symmetry axis through apex perpendicular to chord (start - end)
    Vec2d chord = end - start;
    double chord_len = chord.length();
    if (chord_len < 1e-12) return nullptr;

    Vec2d chord_dir = chord / chord_len;
    Vec2d axis_dir{-chord_dir.y, chord_dir.x};
    double angle = std::atan2(axis_dir.y, axis_dir.x);

    // Compute coordinate of start relative to apex along chord and axis
    Vec2d v_start = start - apex;
    double x_start = v_start.dot(chord_dir);
    double y_start = v_start.dot(axis_dir);

    Vec2d v_end = end - apex;
    double x_end = v_end.dot(chord_dir);

    double a = 0.0;
    if (std::abs(x_start) > 1e-9) {
        a = y_start / (x_start * x_start);
    } else {
        a = 0.1;
    }

    double min_x = std::min(x_start, x_end);
    double max_x = std::max(x_start, x_end);

    return std::make_unique<ParabolicArc2D>(apex, a, angle, min_x, max_x);
}

Point2D ParabolicArc2D::evaluate(double t) const {
    double clamped_t = std::clamp(t, 0.0, 1.0);
    double local_x = t_min + clamped_t * (t_max - t_min);
    double local_y = focal_coeff * local_x * local_x;

    // Local coordinates: X along tangent perpendicular to axis, Y along symmetry axis
    Vec2d axis{std::cos(axis_angle), std::sin(axis_angle)};
    Vec2d perp{-axis.y, axis.x};

    return vertex + perp * local_x + axis * local_y;
}

Vec2d ParabolicArc2D::derivative(double t) const {
    double clamped_t = std::clamp(t, 0.0, 1.0);
    double local_x = t_min + clamped_t * (t_max - t_min);
    double dx_dt = t_max - t_min;

    Vec2d axis{std::cos(axis_angle), std::sin(axis_angle)};
    Vec2d perp{-axis.y, axis.x};

    return (perp + axis * (2.0 * focal_coeff * local_x)) * dx_dt;
}

Vec2d ParabolicArc2D::second_derivative(double t) const {
    (void)t;
    double dx_dt = t_max - t_min;
    Vec2d axis{std::cos(axis_angle), std::sin(axis_angle)};
    return axis * (2.0 * focal_coeff * dx_dt * dx_dt);
}

double ParabolicArc2D::curvature(double t) const noexcept {
    double clamped_t = std::clamp(t, 0.0, 1.0);
    double local_x = t_min + clamped_t * (t_max - t_min);
    double denom = 1.0 + 4.0 * focal_coeff * focal_coeff * local_x * local_x;
    return std::abs(2.0 * focal_coeff) / std::pow(denom, 1.5);
}

double ParabolicArc2D::antiderivative_arc_length(double x) const noexcept {
    double a = std::abs(focal_coeff);
    if (a < 1e-12) return x;

    double u = 2.0 * a * x;
    double sqrt_1_u2 = std::sqrt(1.0 + u * u);
    double integral = 0.5 * u * sqrt_1_u2 + 0.5 * std::asinh(u);
    return integral / (2.0 * a);
}

double ParabolicArc2D::total_length() const {
    return arc_length(0.0, 1.0);
}

double ParabolicArc2D::arc_length(double t0, double t1) const {
    double x0 = t_min + t0 * (t_max - t_min);
    double x1 = t_min + t1 * (t_max - t_min);
    return std::abs(antiderivative_arc_length(x1) - antiderivative_arc_length(x0));
}

BBox2D ParabolicArc2D::bounding_box() const {
    BBox2D box;
    const size_t samples = 48;
    for (size_t k = 0; k <= samples; ++k) {
        box.expand(evaluate(static_cast<double>(k) / static_cast<double>(samples)));
    }
    return box;
}

Point2D ParabolicArc2D::closest_point(const Point2D& query, double* out_t) const {
    const size_t samples = 80;
    double best_dist_sq = std::numeric_limits<double>::infinity();
    double best_t = 0.0;
    Point2D best_pt{};

    for (size_t k = 0; k <= samples; ++k) {
        double t = static_cast<double>(k) / static_cast<double>(samples);
        Point2D p = evaluate(t);
        double d_sq = (p.x - query.x) * (p.x - query.x) + (p.y - query.y) * (p.y - query.y);
        if (d_sq < best_dist_sq) {
            best_dist_sq = d_sq;
            best_t = t;
            best_pt = p;
        }
    }
    if (out_t) *out_t = best_t;
    return best_pt;
}

std::unique_ptr<Curve2D> ParabolicArc2D::clone() const {
    return std::make_unique<ParabolicArc2D>(*this);
}

} // namespace shunya::cad2d
