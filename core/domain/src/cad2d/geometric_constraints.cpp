#include "shunya/cad2d/geometric_constraints.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

// =============================================================================
// CoincidentConstraint
// =============================================================================

void CoincidentConstraint::evaluate(const std::vector<double>& p, double* out) const {
    out[0] = p[a.ix] - p[b.ix];
    out[1] = p[a.iy] - p[b.iy];
}

void CoincidentConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    (void)p;
    triplets.push_back({row + 0, a.ix, 1.0});
    triplets.push_back({row + 0, b.ix, -1.0});
    triplets.push_back({row + 1, a.iy, 1.0});
    triplets.push_back({row + 1, b.iy, -1.0});
}

// =============================================================================
// PointOnLineConstraint
// F = (x1 - x0)*(yp - y0) - (y1 - y0)*(xp - x0) = 0
// =============================================================================

void PointOnLineConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double x0 = p[line.p0.ix], y0 = p[line.p0.iy];
    double x1 = p[line.p1.ix], y1 = p[line.p1.iy];
    double xp = p[pt.ix],      yp = p[pt.iy];
    out[0] = (x1 - x0) * (yp - y0) - (y1 - y0) * (xp - x0);
}

void PointOnLineConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double x0 = p[line.p0.ix], y0 = p[line.p0.iy];
    double x1 = p[line.p1.ix], y1 = p[line.p1.iy];
    double xp = p[pt.ix],      yp = p[pt.iy];

    triplets.push_back({row, pt.ix,      -(y1 - y0)});
    triplets.push_back({row, pt.iy,       (x1 - x0)});
    triplets.push_back({row, line.p0.ix,  (y1 - yp)});
    triplets.push_back({row, line.p0.iy,  (xp - x1)});
    triplets.push_back({row, line.p1.ix,  (yp - y0)});
    triplets.push_back({row, line.p1.iy, -(xp - x0)});
}

// =============================================================================
// CollinearConstraint
// =============================================================================

void CollinearConstraint::evaluate(const std::vector<double>& p, double* out) const {
    PointOnLineConstraint c1(l2.p0, l1);
    PointOnLineConstraint c2(l2.p1, l1);
    c1.evaluate(p, &out[0]);
    c2.evaluate(p, &out[1]);
}

void CollinearConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    PointOnLineConstraint c1(l2.p0, l1);
    PointOnLineConstraint c2(l2.p1, l1);
    c1.accumulate_jacobian(p, row + 0, triplets);
    c2.accumulate_jacobian(p, row + 1, triplets);
}

// =============================================================================
// ParallelConstraint
// F = (x1b - x1a)*(y2b - y2a) - (y1b - y1a)*(x2b - x2a) = 0
// =============================================================================

void ParallelConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double dx1 = p[l1.p1.ix] - p[l1.p0.ix];
    double dy1 = p[l1.p1.iy] - p[l1.p0.iy];
    double dx2 = p[l2.p1.ix] - p[l2.p0.ix];
    double dy2 = p[l2.p1.iy] - p[l2.p0.iy];
    out[0] = dx1 * dy2 - dy1 * dx2;
}

void ParallelConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double dx1 = p[l1.p1.ix] - p[l1.p0.ix];
    double dy1 = p[l1.p1.iy] - p[l1.p0.iy];
    double dx2 = p[l2.p1.ix] - p[l2.p0.ix];
    double dy2 = p[l2.p1.iy] - p[l2.p0.iy];

    triplets.push_back({row, l1.p0.ix, -dy2});
    triplets.push_back({row, l1.p0.iy,  dx2});
    triplets.push_back({row, l1.p1.ix,  dy2});
    triplets.push_back({row, l1.p1.iy, -dx2});
    triplets.push_back({row, l2.p0.ix,  dy1});
    triplets.push_back({row, l2.p0.iy, -dx1});
    triplets.push_back({row, l2.p1.ix, -dy1});
    triplets.push_back({row, l2.p1.iy,  dx1});
}

// =============================================================================
// PerpendicularConstraint
// F = dx1 * dx2 + dy1 * dy2 = 0
// =============================================================================

void PerpendicularConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double dx1 = p[l1.p1.ix] - p[l1.p0.ix];
    double dy1 = p[l1.p1.iy] - p[l1.p0.iy];
    double dx2 = p[l2.p1.ix] - p[l2.p0.ix];
    double dy2 = p[l2.p1.iy] - p[l2.p0.iy];
    out[0] = dx1 * dx2 + dy1 * dy2;
}

void PerpendicularConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double dx1 = p[l1.p1.ix] - p[l1.p0.ix];
    double dy1 = p[l1.p1.iy] - p[l1.p0.iy];
    double dx2 = p[l2.p1.ix] - p[l2.p0.ix];
    double dy2 = p[l2.p1.iy] - p[l2.p0.iy];

    triplets.push_back({row, l1.p0.ix, -dx2});
    triplets.push_back({row, l1.p0.iy, -dy2});
    triplets.push_back({row, l1.p1.ix,  dx2});
    triplets.push_back({row, l1.p1.iy,  dy2});
    triplets.push_back({row, l2.p0.ix, -dx1});
    triplets.push_back({row, l2.p0.iy, -dy1});
    triplets.push_back({row, l2.p1.ix,  dx1});
    triplets.push_back({row, l2.p1.iy,  dy1});
}

// =============================================================================
// ConcentricConstraint
// =============================================================================

void ConcentricConstraint::evaluate(const std::vector<double>& p, double* out) const {
    out[0] = p[c1.ix] - p[c2.ix];
    out[1] = p[c1.iy] - p[c2.iy];
}

void ConcentricConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    (void)p;
    triplets.push_back({row + 0, c1.ix,  1.0});
    triplets.push_back({row + 0, c2.ix, -1.0});
    triplets.push_back({row + 1, c1.iy,  1.0});
    triplets.push_back({row + 1, c2.iy, -1.0});
}

// =============================================================================
// PointOnCircleConstraint
// F = (xp - xc)^2 + (yp - yc)^2 - R^2 = 0
// =============================================================================

void PointOnCircleConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double dx = p[pt.ix] - p[circle.center.ix];
    double dy = p[pt.iy] - p[circle.center.iy];
    double r = p[circle.ir];
    out[0] = dx * dx + dy * dy - r * r;
}

void PointOnCircleConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double dx = p[pt.ix] - p[circle.center.ix];
    double dy = p[pt.iy] - p[circle.center.iy];
    double r = p[circle.ir];

    triplets.push_back({row, pt.ix,             2.0 * dx});
    triplets.push_back({row, pt.iy,             2.0 * dy});
    triplets.push_back({row, circle.center.ix, -2.0 * dx});
    triplets.push_back({row, circle.center.iy, -2.0 * dy});
    triplets.push_back({row, circle.ir,        -2.0 * r});
}

// =============================================================================
// TangentLineCircleConstraint
// F = N^2 / L^2 - R^2 = 0 where N = ux*(yc - y0) - uy*(xc - x0), L^2 = ux^2 + uy^2
// =============================================================================

void TangentLineCircleConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double x0 = p[line.p0.ix], y0 = p[line.p0.iy];
    double x1 = p[line.p1.ix], y1 = p[line.p1.iy];
    double xc = p[circle.center.ix], yc = p[circle.center.iy];
    double r = p[circle.ir];

    double ux = x1 - x0;
    double uy = y1 - y0;
    double l_sq = std::max(1e-24, ux * ux + uy * uy);
    double n_val = ux * (yc - y0) - uy * (xc - x0);
    out[0] = (n_val * n_val) / l_sq - r * r;
}

void TangentLineCircleConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double x0 = p[line.p0.ix], y0 = p[line.p0.iy];
    double x1 = p[line.p1.ix], y1 = p[line.p1.iy];
    double xc = p[circle.center.ix], yc = p[circle.center.iy];
    double r = p[circle.ir];

    double ux = x1 - x0;
    double uy = y1 - y0;
    double l_sq = std::max(1e-24, ux * ux + uy * uy);
    double n_val = ux * (yc - y0) - uy * (xc - x0);

    double c_n = 2.0 * n_val / l_sq;
    double c_l = (n_val * n_val) / (l_sq * l_sq);

    triplets.push_back({row, line.p0.ix, c_n * (y1 - yc) - c_l * (-2.0 * ux)});
    triplets.push_back({row, line.p0.iy, c_n * (xc - x1) - c_l * (-2.0 * uy)});
    triplets.push_back({row, line.p1.ix, c_n * (yc - y0) - c_l * (2.0 * ux)});
    triplets.push_back({row, line.p1.iy, c_n * (x0 - xc) - c_l * (2.0 * uy)});
    triplets.push_back({row, circle.center.ix, c_n * (-uy)});
    triplets.push_back({row, circle.center.iy, c_n * (ux)});
    triplets.push_back({row, circle.ir, -2.0 * r});
}

// =============================================================================
// TangentCircleCircleConstraint
// F = (xc2 - xc1)^2 + (yc2 - yc1)^2 - (r1 +/- r2)^2 = 0
// =============================================================================

void TangentCircleCircleConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double dx = p[c2.center.ix] - p[c1.center.ix];
    double dy = p[c2.center.iy] - p[c1.center.iy];
    double r1 = p[c1.ir];
    double r2 = p[c2.ir];
    double target_r = internal ? (r1 - r2) : (r1 + r2);
    out[0] = dx * dx + dy * dy - target_r * target_r;
}

void TangentCircleCircleConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double dx = p[c2.center.ix] - p[c1.center.ix];
    double dy = p[c2.center.iy] - p[c1.center.iy];
    double r1 = p[c1.ir];
    double r2 = p[c2.ir];
    double target_r = internal ? (r1 - r2) : (r1 + r2);

    triplets.push_back({row, c1.center.ix, -2.0 * dx});
    triplets.push_back({row, c1.center.iy, -2.0 * dy});
    triplets.push_back({row, c2.center.ix,  2.0 * dx});
    triplets.push_back({row, c2.center.iy,  2.0 * dy});
    triplets.push_back({row, c1.ir, -2.0 * target_r});
    triplets.push_back({row, c2.ir, internal ? (2.0 * target_r) : (-2.0 * target_r)});
}

} // namespace shunya::cad2d
