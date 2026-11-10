#include "shunya/cad2d/constraint_graph.hpp"

#include <algorithm>

namespace shunya::cad2d {

ParamPoint2D ConstraintSystem2D::add_point(double x, double y, bool fixed) {
    size_t ix = params_.size();
    params_.push_back(x);
    fixed_.push_back(fixed);
    size_t iy = params_.size();
    params_.push_back(y);
    fixed_.push_back(fixed);
    return {ix, iy};
}

ParamLine2D ConstraintSystem2D::add_line(ParamPoint2D p0, ParamPoint2D p1) {
    return {p0, p1};
}

ParamLine2D ConstraintSystem2D::add_line_points(double x0, double y0, double x1, double y1) {
    return {add_point(x0, y0, false), add_point(x1, y1, false)};
}

ParamCircle2D ConstraintSystem2D::add_circle(double cx, double cy, double radius, bool center_fixed, bool radius_fixed) {
    auto c = add_point(cx, cy, center_fixed);
    size_t ir = params_.size();
    params_.push_back(radius);
    fixed_.push_back(radius_fixed);
    return {c, ir};
}

void ConstraintSystem2D::fix_param(size_t param_idx, bool fixed) {
    if (param_idx < fixed_.size()) fixed_[param_idx] = fixed;
}

void ConstraintSystem2D::fix_point(ParamPoint2D pt, bool fixed) {
    fix_param(pt.ix, fixed);
    fix_param(pt.iy, fixed);
}

void ConstraintSystem2D::set_point(ParamPoint2D pt, const Point2D& val) {
    if (pt.ix < params_.size()) params_[pt.ix] = val.x;
    if (pt.iy < params_.size()) params_[pt.iy] = val.y;
}

Point2D ConstraintSystem2D::get_point(ParamPoint2D pt) const {
    double x = (pt.ix < params_.size()) ? params_[pt.ix] : 0.0;
    double y = (pt.iy < params_.size()) ? params_[pt.iy] : 0.0;
    return {x, y};
}

double ConstraintSystem2D::get_param(size_t idx) const {
    return (idx < params_.size()) ? params_[idx] : 0.0;
}

void ConstraintSystem2D::set_param(size_t idx, double val) {
    if (idx < params_.size()) params_[idx] = val;
}

bool ConstraintSystem2D::is_fixed(size_t idx) const {
    return (idx < fixed_.size()) ? fixed_[idx] : true;
}

size_t ConstraintSystem2D::add_constraint(std::shared_ptr<Constraint2D> constraint) {
    if (!constraint) return 0;
    constraint->id = next_constraint_id_++;
    constraints_.push_back(std::move(constraint));
    return constraints_.back()->id;
}

bool ConstraintSystem2D::remove_constraint(size_t constraint_id) {
    auto it = std::find_if(constraints_.begin(), constraints_.end(), [&](const auto& c) {
        return c && c->id == constraint_id;
    });
    if (it == constraints_.end()) return false;
    constraints_.erase(it);
    return true;
}

void ConstraintSystem2D::clear() {
    params_.clear();
    fixed_.clear();
    constraints_.clear();
    next_constraint_id_ = 1;
}

size_t ConstraintSystem2D::free_param_count() const noexcept {
    size_t count = 0;
    for (bool f : fixed_) {
        if (!f) ++count;
    }
    return count;
}

size_t ConstraintSystem2D::total_residual_rows() const noexcept {
    size_t rows = 0;
    for (const auto& c : constraints_) {
        if (c && c->enabled) rows += c->residual_count();
    }
    return rows;
}

} // namespace shunya::cad2d
