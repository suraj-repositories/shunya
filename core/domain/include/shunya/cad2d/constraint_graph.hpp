#pragma once

#include "shunya/cad2d/curve_2d.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::cad2d {

struct ParamPoint2D {
    size_t ix{0};
    size_t iy{0};
};

struct ParamLine2D {
    ParamPoint2D p0;
    ParamPoint2D p1;
};

struct ParamCircle2D {
    ParamPoint2D center;
    size_t ir{0};
};

struct JacobianTriplet {
    size_t row{0};
    size_t col{0}; // Parameter index in ConstraintSystem2D::params()
    double value{0.0};
};

/**
 * @brief Base class for algebraic 2D geometric and dimensional constraint equations F(p) = 0.
 */
class Constraint2D {
public:
    size_t id{0};
    std::string label;
    bool enabled{true};

    virtual ~Constraint2D() = default;

    [[nodiscard]] virtual std::string_view type_name() const noexcept = 0;
    [[nodiscard]] virtual size_t residual_count() const noexcept = 0;
    virtual void evaluate(const std::vector<double>& params, double* out_residuals) const = 0;
    virtual void accumulate_jacobian(
        const std::vector<double>& params,
        size_t row_offset,
        std::vector<JacobianTriplet>& out_triplets
    ) const = 0;
};

/**
 * @brief Holds parametric variables, fixed/free flags, primitives, and constraints for a 2D sketch.
 */
class ConstraintSystem2D {
public:
    ParamPoint2D add_point(double x, double y, bool fixed = false);
    ParamLine2D add_line(ParamPoint2D p0, ParamPoint2D p1);
    ParamLine2D add_line_points(double x0, double y0, double x1, double y1);
    ParamCircle2D add_circle(double cx, double cy, double radius, bool center_fixed = false, bool radius_fixed = false);

    void fix_param(size_t param_idx, bool fixed = true);
    void fix_point(ParamPoint2D pt, bool fixed = true);
    void set_point(ParamPoint2D pt, const Point2D& val);

    [[nodiscard]] Point2D get_point(ParamPoint2D pt) const;
    [[nodiscard]] double get_param(size_t idx) const;
    void set_param(size_t idx, double val);
    [[nodiscard]] bool is_fixed(size_t idx) const;

    size_t add_constraint(std::shared_ptr<Constraint2D> constraint);
    bool remove_constraint(size_t constraint_id);
    void clear();

    [[nodiscard]] std::vector<double>& params() noexcept { return params_; }
    [[nodiscard]] const std::vector<double>& params() const noexcept { return params_; }
    [[nodiscard]] const std::vector<bool>& fixed_flags() const noexcept { return fixed_; }
    [[nodiscard]] const std::vector<std::shared_ptr<Constraint2D>>& constraints() const noexcept { return constraints_; }

    [[nodiscard]] size_t param_count() const noexcept { return params_.size(); }
    [[nodiscard]] size_t free_param_count() const noexcept;
    [[nodiscard]] size_t total_residual_rows() const noexcept;

private:
    std::vector<double> params_;
    std::vector<bool> fixed_;
    std::vector<std::shared_ptr<Constraint2D>> constraints_;
    size_t next_constraint_id_{1};
};

} // namespace shunya::cad2d
