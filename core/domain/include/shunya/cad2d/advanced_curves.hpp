#pragma once

#include "shunya/cad2d/curve_2d.hpp"

#include <vector>
#include <memory>

namespace shunya::cad2d {

/**
 * @brief Interpolating cubic Catmull-Rom spline passing exactly through all control points.
 */
class CatmullRomSpline2D final : public Curve2D {
public:
    std::vector<Point2D> control_points;
    bool closed{false};

    CatmullRomSpline2D() = default;
    explicit CatmullRomSpline2D(std::vector<Point2D> pts, bool is_closed = false);

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::Polyline; }
    [[nodiscard]] bool is_closed() const noexcept override { return closed && control_points.size() >= 3; }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;

private:
    void get_segment_points(size_t seg_idx, Point2D& p0, Point2D& p1, Point2D& p2, Point2D& p3) const;
};

/**
 * @brief Uniform cubic B-spline offering C^2 curvature continuity across control points.
 */
class BSpline2D final : public Curve2D {
public:
    std::vector<Point2D> control_points;
    bool closed{false};

    BSpline2D() = default;
    explicit BSpline2D(std::vector<Point2D> pts, bool is_closed = false);

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::Polyline; }
    [[nodiscard]] bool is_closed() const noexcept override { return closed && control_points.size() >= 4; }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;

private:
    void get_segment_points(size_t seg_idx, Point2D& p0, Point2D& p1, Point2D& p2, Point2D& p3) const;
};

/**
 * @brief Parabolic arc defined by vertex, focal coefficient a, orientation, and parameter range [t_min, t_max].
 * Local equation: y = a * x^2 for x in [t_min, t_max].
 */
class ParabolicArc2D final : public Curve2D {
public:
    Point2D vertex{0.0, 0.0};
    double focal_coeff{0.1};      // a in y = a * x^2
    double axis_angle{0.0};       // Orientation angle in radians of the symmetry axis (local +Y)
    double t_min{-5.0};           // Local x min
    double t_max{5.0};            // Local x max

    ParabolicArc2D() = default;
    ParabolicArc2D(Point2D v, double a, double angle_rad, double min_t, double max_t) noexcept
        : vertex(v), focal_coeff(a), axis_angle(angle_rad), t_min(min_t), t_max(max_t) {}

    [[nodiscard]] static std::unique_ptr<ParabolicArc2D> from_three_points(
        const Point2D& start,
        const Point2D& apex,
        const Point2D& end
    );

    [[nodiscard]] CurveType2D type() const noexcept override { return CurveType2D::Arc; }
    [[nodiscard]] bool is_closed() const noexcept override { return false; }

    [[nodiscard]] Point2D evaluate(double t) const override;
    [[nodiscard]] Vec2d derivative(double t) const override;
    [[nodiscard]] Vec2d second_derivative(double t) const override;
    [[nodiscard]] double curvature(double t) const noexcept override;

    [[nodiscard]] double total_length() const override;
    [[nodiscard]] double arc_length(double t0, double t1) const override;
    [[nodiscard]] BBox2D bounding_box() const override;
    [[nodiscard]] Point2D closest_point(const Point2D& query, double* out_t = nullptr) const override;
    [[nodiscard]] std::unique_ptr<Curve2D> clone() const override;

    [[nodiscard]] double antiderivative_arc_length(double x) const noexcept;
};

} // namespace shunya::cad2d
