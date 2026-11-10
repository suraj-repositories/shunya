#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

namespace shunya::cad2d {

using Point2D = core::base::math::Vector2<double>;
using Vec2d = core::base::math::Vector2<double>;

[[nodiscard]] constexpr double cross2d(const Vec2d& a, const Vec2d& b) noexcept {
    return a.x * b.y - a.y * b.x;
}

[[nodiscard]] constexpr Vec2d perp_ccw(const Vec2d& v) noexcept {
    return {-v.y, v.x};
}

[[nodiscard]] constexpr Vec2d perp_cw(const Vec2d& v) noexcept {
    return {v.y, -v.x};
}

struct BBox2D {
    Point2D min{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    Point2D max{-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};

    [[nodiscard]] bool is_valid() const noexcept {
        return std::isfinite(min.x) && std::isfinite(min.y) &&
               std::isfinite(max.x) && std::isfinite(max.y) &&
               min.x <= max.x && min.y <= max.y;
    }

    void expand(const Point2D& pt) noexcept {
        min.x = std::min(min.x, pt.x);
        min.y = std::min(min.y, pt.y);
        max.x = std::max(max.x, pt.x);
        max.y = std::max(max.y, pt.y);
    }

    void expand(const BBox2D& other) noexcept {
        if (!other.is_valid()) return;
        expand(other.min);
        expand(other.max);
    }

    [[nodiscard]] bool contains(const Point2D& pt, double tol = 0.0) const noexcept {
        return is_valid() &&
               pt.x >= min.x - tol && pt.x <= max.x + tol &&
               pt.y >= min.y - tol && pt.y <= max.y + tol;
    }

    [[nodiscard]] bool intersects(const BBox2D& other, double tol = 0.0) const noexcept {
        if (!is_valid() || !other.is_valid()) return false;
        return !(max.x < other.min.x - tol || min.x > other.max.x + tol ||
                 max.y < other.min.y - tol || min.y > other.max.y + tol);
    }

    [[nodiscard]] double width() const noexcept { return is_valid() ? (max.x - min.x) : 0.0; }
    [[nodiscard]] double height() const noexcept { return is_valid() ? (max.y - min.y) : 0.0; }
    [[nodiscard]] Point2D center() const noexcept { return {(min.x + max.x) * 0.5, (min.y + max.y) * 0.5}; }
};

enum class CurveType2D {
    LineSegment,
    Polyline,
    Arc,
    Circle,
    Ellipse,
    BezierCubic
};

/**
 * @brief Base class for parametric 2D geometric curves parameterized over t in [0, 1].
 */
class Curve2D {
public:
    virtual ~Curve2D() = default;

    [[nodiscard]] virtual CurveType2D type() const noexcept = 0;
    [[nodiscard]] virtual bool is_closed() const noexcept = 0;

    [[nodiscard]] virtual Point2D evaluate(double t) const = 0;
    [[nodiscard]] virtual Vec2d derivative(double t) const = 0;
    [[nodiscard]] virtual Vec2d second_derivative(double t) const = 0;

    [[nodiscard]] virtual Vec2d tangent(double t) const;
    [[nodiscard]] virtual double curvature(double t) const;

    [[nodiscard]] virtual double total_length() const = 0;
    [[nodiscard]] virtual double arc_length(double t0, double t1) const = 0;
    [[nodiscard]] virtual BBox2D bounding_box() const = 0;
    [[nodiscard]] virtual Point2D closest_point(const Point2D& query, double* out_t = nullptr) const = 0;

    [[nodiscard]] virtual std::vector<Point2D> sample_polyline(size_t segments = 64) const;
    [[nodiscard]] virtual std::unique_ptr<Curve2D> clone() const = 0;
};

/**
 * @brief Gauss-Legendre numerical integration helper for curve arc length (16-point composite quadrature).
 */
[[nodiscard]] double gauss_legendre_arc_length(const Curve2D& curve, double t0, double t1, size_t sub_intervals = 8);

class LineSegment2D final : public Curve2D {
public:
    Point2D p0{};
    Point2D p1{};

    LineSegment2D() = default;
    LineSegment2D(Point2D start, Point2D end) noexcept : p0(start), p1(end) {}

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::LineSegment; }
    [[nodiscard]] bool is_closed() const noexcept override { return false; }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;
    [[nodiscard]] double curvature(double t) const noexcept override { (void)t; return 0.0; }

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::vector<Point2D> sample_polyline(size_t segments = 64) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;
};

class Polyline2D final : public Curve2D {
public:
    std::vector<Point2D> points;
    bool closed{false};

    Polyline2D() = default;
    explicit Polyline2D(std::vector<Point2D> pts, bool is_closed_loop = false)
        : points(std::move(pts)), closed(is_closed_loop) {}

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::Polyline; }
    [[nodiscard]] bool is_closed() const noexcept override { return closed && points.size() >= 2; }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const noexcept override { (void)t; return {0.0, 0.0}; }

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::vector<Point2D> sample_polyline(size_t segments = 64) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;
};

class Arc2D final : public Curve2D {
public:
    Point2D center{0.0, 0.0};
    double radius{1.0};
    double start_angle{0.0}; // radians
    double sweep_angle{core::base::math::PI}; // positive = CCW, negative = CW

    Arc2D() = default;
    Arc2D(Point2D c, double r, double start_rad, double sweep_rad) noexcept
        : center(c), radius(std::abs(r)), start_angle(start_rad), sweep_angle(sweep_rad) {}

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::Arc; }
    [[nodiscard]] bool is_closed() const noexcept override {
        return std::abs(std::abs(sweep_angle) - 2.0 * core::base::math::PI) <= 1e-12;
    }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;
    [[nodiscard]] double curvature(double t) const noexcept override;

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;
};

class Circle2D final : public Curve2D {
public:
    Point2D center{0.0, 0.0};
    double radius{1.0};

    Circle2D() = default;
    Circle2D(Point2D c, double r) noexcept : center(c), radius(std::abs(r)) {}

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::Circle; }
    [[nodiscard]] bool is_closed() const noexcept override { return true; }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;
    [[nodiscard]] double curvature(double t) const noexcept override;

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;
};

class Ellipse2D final : public Curve2D {
public:
    Point2D center{0.0, 0.0};
    double rx{2.0};
    double ry{1.0};
    double rotation{0.0}; // radians

    Ellipse2D() = default;
    Ellipse2D(Point2D c, double radius_x, double radius_y, double rotation_rad = 0.0) noexcept
        : center(c), rx(std::abs(radius_x)), ry(std::abs(radius_y)), rotation(rotation_rad) {}

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::Ellipse; }
    [[nodiscard]] bool is_closed() const noexcept override { return true; }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] double ramanujan_perimeter() const noexcept;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;
};

} // namespace shunya::cad2d
