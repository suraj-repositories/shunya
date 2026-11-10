#pragma once

#include "shunya/cad2d/constraint_graph.hpp"

namespace shunya::cad2d {

class CoincidentConstraint final : public Constraint2D {
public:
    ParamPoint2D a;
    ParamPoint2D b;

    CoincidentConstraint(ParamPoint2D pt_a, ParamPoint2D pt_b) noexcept : a(pt_a), b(pt_b) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Coincident"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 2; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class PointOnLineConstraint final : public Constraint2D {
public:
    ParamPoint2D pt;
    ParamLine2D line;

    PointOnLineConstraint(ParamPoint2D point, ParamLine2D ln) noexcept : pt(point), line(ln) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "PointOnLine"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class CollinearConstraint final : public Constraint2D {
public:
    ParamLine2D l1;
    ParamLine2D l2;

    CollinearConstraint(ParamLine2D line1, ParamLine2D line2) noexcept : l1(line1), l2(line2) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Collinear"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 2; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class ParallelConstraint final : public Constraint2D {
public:
    ParamLine2D l1;
    ParamLine2D l2;

    ParallelConstraint(ParamLine2D line1, ParamLine2D line2) noexcept : l1(line1), l2(line2) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Parallel"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class PerpendicularConstraint final : public Constraint2D {
public:
    ParamLine2D l1;
    ParamLine2D l2;

    PerpendicularConstraint(ParamLine2D line1, ParamLine2D line2) noexcept : l1(line1), l2(line2) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Perpendicular"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class ConcentricConstraint final : public Constraint2D {
public:
    ParamPoint2D c1;
    ParamPoint2D c2;

    ConcentricConstraint(ParamPoint2D center1, ParamPoint2D center2) noexcept : c1(center1), c2(center2) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "Concentric"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 2; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class PointOnCircleConstraint final : public Constraint2D {
public:
    ParamPoint2D pt;
    ParamCircle2D circle;

    PointOnCircleConstraint(ParamPoint2D point, ParamCircle2D circ) noexcept : pt(point), circle(circ) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "PointOnCircle"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class TangentLineCircleConstraint final : public Constraint2D {
public:
    ParamLine2D line;
    ParamCircle2D circle;

    TangentLineCircleConstraint(ParamLine2D ln, ParamCircle2D circ) noexcept : line(ln), circle(circ) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "TangentLineCircle"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

class TangentCircleCircleConstraint final : public Constraint2D {
public:
    ParamCircle2D c1;
    ParamCircle2D c2;
    bool internal{false};

    TangentCircleCircleConstraint(ParamCircle2D circ1, ParamCircle2D circ2, bool is_internal = false) noexcept
        : c1(circ1), c2(circ2), internal(is_internal) {}

    [[nodiscard]] std::string_view type_name() const noexcept override { return "TangentCircleCircle"; }
    [[nodiscard]] size_t residual_count() const noexcept override { return 1; }
    void evaluate(const std::vector<double>& p, double* out) const override;
    void accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const override;
};

} // namespace shunya::cad2d
