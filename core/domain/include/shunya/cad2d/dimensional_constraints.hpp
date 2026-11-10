#pragma once

#include "shunya/cad2d/constraint_graph.hpp"

namespace shunya::cad2d {

class HorizontalConstraint final : public Constraint2D {
public:
    ParamPoint2D p0;
    ParamPoint2D p1;

    HorizontalConstraint(ParamPoint2D a, ParamPoint2D b) noexcept : p0(a), p1(b) {}
    explicit HorizontalConstraint(ParamLine2D line) noexcept : p0(line.p0), p1(line.p1) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Horizontal"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class VerticalConstraint final : public Constraint2D {
public:
    ParamPoint2D p0;
    ParamPoint2D p1;

    VerticalConstraint(ParamPoint2D a, ParamPoint2D b) noexcept : p0(a), p1(b) {}
    explicit VerticalConstraint(ParamLine2D line) noexcept : p0(line.p0), p1(line.p1) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Vertical"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class PointDistanceConstraint final : public Constraint2D {
public:
    ParamPoint2D a;
    ParamPoint2D b;
    double target_distance{1.0};

    PointDistanceConstraint(ParamPoint2D pt_a, ParamPoint2D pt_b, double dist) noexcept
        : a(pt_a), b(pt_b), target_distance(dist) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "PointDistance"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class LineLengthConstraint final : public Constraint2D {
public:
    ParamLine2D line;
    double target_length{1.0};

    LineLengthConstraint(ParamLine2D ln, double len) noexcept : line(ln), target_length(len) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "LineLength"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class AngleConstraint final : public Constraint2D {
public:
    ParamLine2D l1;
    ParamLine2D l2;
    double target_angle_rad{0.0};

    AngleConstraint(ParamLine2D line1, ParamLine2D line2, double angle_rad) noexcept
        : l1(line1), l2(line2), target_angle_rad(angle_rad) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Angle"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class RadiusConstraint final : public Constraint2D {
public:
    ParamCircle2D circle;
    double target_radius{1.0};

    RadiusConstraint(ParamCircle2D circ, double r) noexcept : circle(circ), target_radius(r) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Radius"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class DiameterConstraint final : public Constraint2D {
public:
    ParamCircle2D circle;
    double target_diameter{2.0};

    DiameterConstraint(ParamCircle2D circ, double d) noexcept : circle(circ), target_diameter(d) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Diameter"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class EqualLengthConstraint final : public Constraint2D {
public:
    ParamLine2D l1;
    ParamLine2D l2;

    EqualLengthConstraint(ParamLine2D line1, ParamLine2D line2) noexcept : l1(line1), l2(line2) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "EqualLength"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class EqualRadiusConstraint final : public Constraint2D {
public:
    ParamCircle2D c1;
    ParamCircle2D c2;

    EqualRadiusConstraint(ParamCircle2D circ1, ParamCircle2D circ2) noexcept : c1(circ1), c2(circ2) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "EqualRadius"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

} // namespace shunya::cad2d
