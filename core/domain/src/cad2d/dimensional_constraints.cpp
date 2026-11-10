#include "shunya/cad2d/dimensional_constraints.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

// =============================================================================
// HorizontalConstraint & VerticalConstraint
// =============================================================================

void HorizontalConstraint::evaluate(const std::vector<double>& p, double* out) const {
    out[0] = p[p1.iy] - p[p0.iy];
}

void HorizontalConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    (void)p;
    triplets.push_back({row, p0.iy, -1.0});
    triplets.push_back({row, p1.iy,  1.0});
}

void VerticalConstraint::evaluate(const std::vector<double>& p, double* out) const {
    out[0] = p[p1.ix] - p[p0.ix];
}

void VerticalConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    (void)p;
    triplets.push_back({row, p0.ix, -1.0});
    triplets.push_back({row, p1.ix,  1.0});
}

// =============================================================================
// PointDistanceConstraint & LineLengthConstraint
// F = sqrt((xb - xa)^2 + (yb - ya)^2) - target_distance = 0
// =============================================================================

void PointDistanceConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double dx = p[b.ix] - p[a.ix];
    double dy = p[b.iy] - p[a.iy];
    out[0] = std::hypot(dx, dy) - target_distance;
}

void PointDistanceConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double dx = p[b.ix] - p[a.ix];
    double dy = p[b.iy] - p[a.iy];
    double dist = std::hypot(dx, dy);
    if (dist < 1e-12) return;
    double ux = dx / dist;
    double uy = dy / dist;

    triplets.push_back({row, a.ix, -ux});
    triplets.push_back({row, a.iy, -uy});
    triplets.push_back({row, b.ix,  ux});
    triplets.push_back({row, b.iy,  uy});
}

void LineLengthConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double dx = p[line.p1.ix] - p[line.p0.ix];
    double dy = p[line.p1.iy] - p[line.p0.iy];
    out[0] = std::hypot(dx, dy) - target_length;
}

void LineLengthConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    PointDistanceConstraint pd(line.p0, line.p1, target_length);
    pd.accumulate_jacobian(p, row, triplets);
}

// =============================================================================
// AngleConstraint
// F = cross(u_hat, v_hat) * cos(alpha) - dot(u_hat, v_hat) * sin(alpha) = 0
// =============================================================================

void AngleConstraint::evaluate(const std::vector<double>& p, double* out) const {
    Vec2d u{p[l1.p1.ix] - p[l1.p0.ix], p[l1.p1.iy] - p[l1.p0.iy]};
    Vec2d v{p[l2.p1.ix] - p[l2.p0.ix], p[l2.p1.iy] - p[l2.p0.iy]};
    Vec2d u_hat = u.normalized();
    Vec2d v_hat = v.normalized();
    double c = std::cos(target_angle_rad);
    double s = std::sin(target_angle_rad);
    out[0] = cross2d(u_hat, v_hat) * c - u_hat.dot(v_hat) * s;
}

void AngleConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    Vec2d u{p[l1.p1.ix] - p[l1.p0.ix], p[l1.p1.iy] - p[l1.p0.iy]};
    Vec2d v{p[l2.p1.ix] - p[l2.p0.ix], p[l2.p1.iy] - p[l2.p0.iy]};
    double lu = std::max(1e-12, u.length());
    double lv = std::max(1e-12, v.length());
    Vec2d u_hat = u / lu;
    Vec2d v_hat = v / lv;

    double c = std::cos(target_angle_rad);
    double s = std::sin(target_angle_rad);

    // dF / du_hat and dF / dv_hat
    Vec2d df_du_hat{v_hat.y * c - v_hat.x * s, -v_hat.x * c - v_hat.y * s};
    Vec2d df_dv_hat{-u_hat.y * c - u_hat.x * s, u_hat.x * c - u_hat.y * s};

    // Projection (I - u_hat * u_hat^T) / lu
    Vec2d gu = (df_du_hat - u_hat * df_du_hat.dot(u_hat)) / lu;
    Vec2d gv = (df_dv_hat - v_hat * df_dv_hat.dot(v_hat)) / lv;

    triplets.push_back({row, l1.p0.ix, -gu.x});
    triplets.push_back({row, l1.p0.iy, -gu.y});
    triplets.push_back({row, l1.p1.ix,  gu.x});
    triplets.push_back({row, l1.p1.iy,  gu.y});
    triplets.push_back({row, l2.p0.ix, -gv.x});
    triplets.push_back({row, l2.p0.iy, -gv.y});
    triplets.push_back({row, l2.p1.ix,  gv.x});
    triplets.push_back({row, l2.p1.iy,  gv.y});
}

// =============================================================================
// RadiusConstraint, DiameterConstraint, EqualLength, EqualRadius
// =============================================================================

void RadiusConstraint::evaluate(const std::vector<double>& p, double* out) const {
    out[0] = p[circle.ir] - target_radius;
}

void RadiusConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    (void)p;
    triplets.push_back({row, circle.ir, 1.0});
}

void DiameterConstraint::evaluate(const std::vector<double>& p, double* out) const {
    out[0] = 2.0 * p[circle.ir] - target_diameter;
}

void DiameterConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    (void)p;
    triplets.push_back({row, circle.ir, 2.0});
}

void EqualLengthConstraint::evaluate(const std::vector<double>& p, double* out) const {
    double len1 = std::hypot(p[l1.p1.ix] - p[l1.p0.ix], p[l1.p1.iy] - p[l1.p0.iy]);
    double len2 = std::hypot(p[l2.p1.ix] - p[l2.p0.ix], p[l2.p1.iy] - p[l2.p0.iy]);
    out[0] = len1 - len2;
}

void EqualLengthConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    double dx1 = p[l1.p1.ix] - p[l1.p0.ix], dy1 = p[l1.p1.iy] - p[l1.p0.iy];
    double dx2 = p[l2.p1.ix] - p[l2.p0.ix], dy2 = p[l2.p1.iy] - p[l2.p0.iy];
    double len1 = std::max(1e-12, std::hypot(dx1, dy1));
    double len2 = std::max(1e-12, std::hypot(dx2, dy2));

    double ux1 = dx1 / len1, uy1 = dy1 / len1;
    double ux2 = dx2 / len2, uy2 = dy2 / len2;

    triplets.push_back({row, l1.p0.ix, -ux1});
    triplets.push_back({row, l1.p0.iy, -uy1});
    triplets.push_back({row, l1.p1.ix,  ux1});
    triplets.push_back({row, l1.p1.iy,  uy1});
    triplets.push_back({row, l2.p0.ix,  ux2});
    triplets.push_back({row, l2.p0.iy,  uy2});
    triplets.push_back({row, l2.p1.ix, -ux2});
    triplets.push_back({row, l2.p1.iy, -uy2});
}

void EqualRadiusConstraint::evaluate(const std::vector<double>& p, double* out) const {
    out[0] = p[c1.ir] - p[c2.ir];
}

void EqualRadiusConstraint::accumulate_jacobian(const std::vector<double>& p, size_t row, std::vector<JacobianTriplet>& triplets) const {
    (void)p;
    triplets.push_back({row, c1.ir,  1.0});
    triplets.push_back({row, c2.ir, -1.0});
}

} // namespace shunya::cad2d
