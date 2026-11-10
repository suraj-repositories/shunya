#pragma once

#include "shunya/cad2d/curve_2d.hpp"

#include <utility>
#include <vector>

namespace shunya::cad2d {

/**
 * @brief Cubic Bezier curve defined by four control points P0, P1, P2, P3 for t in [0, 1].
 */
class BezierCubic2D final : public Curve2D {
public:
    Point2D p0{};
    Point2D p1{};
    Point2D p2{};
    Point2D p3{};

    BezierCubic2D() = default;
    BezierCubic2D(Point2D cp0, Point2D cp1, Point2D cp2, Point2D cp3) noexcept
        : p0(cp0), p1(cp1), p2(cp2), p3(cp3) {}

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::BezierCubic; }
    [[nodiscard]] bool is_closed() const noexcept override { return p0.approx_equals(p3, 1e-12); }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] BBox2D control_polygon_bbox() const noexcept;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;

    [[nodiscard]] std::pair<BezierCubic2D, BezierCubic2D> split(double t) const noexcept;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;
};

/**
 * @brief Piecewise composite cubic Bezier spline chain.
 */
class BezierSpline2D {
public:
    std::vector<BezierCubic2D> segments;

    void add_segment(const BezierCubic2D& seg) { segments.push_back(seg); }
    [[nodiscard]] size_t segment_count() const noexcept { return segments.size(); }
    [[nodiscard]] Point2D evaluate(double t) const;
    [[nodiscard]] double total_length() const;
    [[nodiscard]] BBox2D bounding_box() const;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const;
    [[nodiscard]] std::vector<Point2D> sample_polyline(size_t segments_per_curve = 32) const;
};

} // namespace shunya::cad2d
