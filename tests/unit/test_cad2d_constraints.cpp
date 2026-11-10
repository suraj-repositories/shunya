#include "tests/test_framework.hpp"

#include "shunya/cad2d/constraint_graph.hpp"
#include "shunya/cad2d/geometric_constraints.hpp"
#include "shunya/cad2d/dimensional_constraints.hpp"
#include "shunya/cad2d/dof_analyzer.hpp"
#include "shunya/cad2d/sketch_solver_2d.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace shunya::cad2d;
using shunya::core::base::math::PI;

namespace {

bool is_approx(double a, double b, double eps = 1e-8) {
    return std::abs(a - b) <= eps;
}

void verify_jacobian_finite_diff(
    const ConstraintSystem2D& sys,
    const Constraint2D& constraint,
    double fd_step = 1e-6,
    double tol = 1e-7
) {
    size_t m = constraint.residual_count();
    size_t n = sys.param_count();

    std::vector<double> r_plus(m, 0.0);
    std::vector<double> r_minus(m, 0.0);
    std::vector<JacobianTriplet> triplets;
    constraint.accumulate_jacobian(sys.params(), 0, triplets);

    std::vector<double> J_analytic(m * n, 0.0);
    for (const auto& t : triplets) {
        REQUIRE(t.row < m);
        REQUIRE(t.col < n);
        J_analytic[t.row * n + t.col] += t.value;
    }

    std::vector<double> p = sys.params();
    for (size_t col = 0; col < n; ++col) {
        double orig = p[col];
        p[col] = orig + fd_step;
        constraint.evaluate(p, r_plus.data());
        p[col] = orig - fd_step;
        constraint.evaluate(p, r_minus.data());
        p[col] = orig;

        for (size_t row = 0; row < m; ++row) {
            double fd = (r_plus[row] - r_minus[row]) / (2.0 * fd_step);
            double an = J_analytic[row * n + col];
            CHECK(std::abs(an - fd) < tol);
        }
    }
}

} // namespace

// =============================================================================
// Task 2D-02-001-01: Analytical Jacobian Verification vs Central Differences
// =============================================================================

TEST_CASE("CAD2D Solver: All Geometric Constraint Jacobians Match Finite Differences (< 1e-7)") {
    ConstraintSystem2D sys;
    auto p1 = sys.add_point(1.3, -2.7);
    auto p2 = sys.add_point(4.1, 3.2);
    auto p3 = sys.add_point(-1.5, 0.8);
    auto p4 = sys.add_point(2.9, 5.4);
    auto p5 = sys.add_point(0.6, 1.9);

    auto l1 = sys.add_line(p1, p2);
    auto l2 = sys.add_line(p3, p4);
    auto c1 = sys.add_circle(2.1, -1.1, 3.4);
    auto c2 = sys.add_circle(6.8, 2.3, 1.9);

    verify_jacobian_finite_diff(sys, CoincidentConstraint(p1, p2));
    verify_jacobian_finite_diff(sys, PointOnLineConstraint(p5, l1));
    verify_jacobian_finite_diff(sys, CollinearConstraint(l1, l2));
    verify_jacobian_finite_diff(sys, ParallelConstraint(l1, l2));
    verify_jacobian_finite_diff(sys, PerpendicularConstraint(l1, l2));
    verify_jacobian_finite_diff(sys, ConcentricConstraint(c1.center, p5));
    verify_jacobian_finite_diff(sys, PointOnCircleConstraint(p5, c1));
    verify_jacobian_finite_diff(sys, TangentLineCircleConstraint(l1, c1));
    verify_jacobian_finite_diff(sys, TangentCircleCircleConstraint(c1, c2, false));
    verify_jacobian_finite_diff(sys, TangentCircleCircleConstraint(c1, c2, true));
}

TEST_CASE("CAD2D Solver: All Dimensional Constraint Jacobians Match Finite Differences (< 1e-7)") {
    ConstraintSystem2D sys;
    auto p1 = sys.add_point(-0.7, 1.4);
    auto p2 = sys.add_point(3.5, 4.1);
    auto p3 = sys.add_point(2.2, -2.3);
    auto p4 = sys.add_point(-3.1, 0.9);

    auto l1 = sys.add_line(p1, p2);
    auto l2 = sys.add_line(p3, p4);
    auto c1 = sys.add_circle(1.2, 2.4, 2.75);
    auto c2 = sys.add_circle(-2.5, 3.1, 1.35);

    verify_jacobian_finite_diff(sys, HorizontalConstraint(p1, p2));
    verify_jacobian_finite_diff(sys, VerticalConstraint(p1, p2));
    verify_jacobian_finite_diff(sys, PointDistanceConstraint(p1, p2, 5.0));
    verify_jacobian_finite_diff(sys, LineLengthConstraint(l1, 7.5));
    verify_jacobian_finite_diff(sys, AngleConstraint(l1, l2, PI / 3.0));
    verify_jacobian_finite_diff(sys, RadiusConstraint(c1, 3.0));
    verify_jacobian_finite_diff(sys, DiameterConstraint(c1, 6.0));
    verify_jacobian_finite_diff(sys, EqualLengthConstraint(l1, l2));
    verify_jacobian_finite_diff(sys, EqualRadiusConstraint(c1, c2));
}

// =============================================================================
// Task 2D-02-001-02: Dimensional Solving (50mm Distance, Triangles, Tangents)
// =============================================================================

TEST_CASE("CAD2D Solver: Exact 50mm Point Separation and Right Triangle Construction") {
    // 1. Force 50.000000mm separation between two initially arbitrary points
    {
        ConstraintSystem2D sys;
        auto a = sys.add_point(0.0, 0.0, true); // Anchor fixed
        auto b = sys.add_point(12.0, 7.5, false);
        sys.add_constraint(std::make_shared<HorizontalConstraint>(a, b));
        sys.add_constraint(std::make_shared<PointDistanceConstraint>(a, b, 50.0));

        auto report = SketchSolver2D::solve(sys);
        REQUIRE(report.converged);
        CHECK(report.residual_norm < 1e-9);
        CHECK(report.dof.remaining_dof == 0);
        CHECK(report.dof.status == DofStatus::FullyConstrained);

        Point2D pb = sys.get_point(b);
        CHECK(is_approx(pb.x, 50.0, 1e-7));
        CHECK(is_approx(pb.y, 0.0, 1e-7));
    }

    // 2. 3-4-5 Right Triangle with Angle and Tangent Circle
    {
        ConstraintSystem2D sys;
        auto p0 = sys.add_point(0.0, 0.0, true);
        auto p1 = sys.add_point(3.5, 0.2, false);
        auto p2 = sys.add_point(0.1, 2.8, false);

        auto base = sys.add_line(p0, p1);
        auto side = sys.add_line(p0, p2);
        auto hyp = sys.add_line(p1, p2);

        sys.add_constraint(std::make_shared<HorizontalConstraint>(base));
        sys.add_constraint(std::make_shared<VerticalConstraint>(side));
        sys.add_constraint(std::make_shared<LineLengthConstraint>(base, 4.0));
        sys.add_constraint(std::make_shared<LineLengthConstraint>(side, 3.0));

        // Circle tangent to hypotenuse and concentric with (1.0, 1.0)
        auto circ = sys.add_circle(1.0, 1.0, 0.5, true, false);
        sys.add_constraint(std::make_shared<TangentLineCircleConstraint>(hyp, circ));

        auto report = SketchSolver2D::solve(sys);
        REQUIRE(report.converged);
        CHECK(report.residual_norm < 1e-9);
        CHECK(report.dof.remaining_dof == 0);

        double hyp_len = (sys.get_point(p2) - sys.get_point(p1)).length();
        CHECK(is_approx(hyp_len, 5.0, 1e-7));
    }

    // 3. Angle Constraint (60 deg equilateral triangle leg alignment)
    {
        ConstraintSystem2D sys;
        auto p0 = sys.add_point(0.0, 0.0, true);
        auto p1 = sys.add_point(10.0, 0.0, true);
        auto p2 = sys.add_point(4.0, 6.0, false);

        auto l1 = sys.add_line(p0, p1);
        auto l2 = sys.add_line(p0, p2);
        sys.add_constraint(std::make_shared<AngleConstraint>(l1, l2, PI / 3.0));
        sys.add_constraint(std::make_shared<LineLengthConstraint>(l2, 10.0));

        auto report = SketchSolver2D::solve(sys);
        REQUIRE(report.converged);
        CHECK(report.dof.remaining_dof == 0);
        CHECK(is_approx(sys.get_point(p2).x, 5.0, 1e-7));
        CHECK(is_approx(sys.get_point(p2).y, 10.0 * std::sin(PI / 3.0), 1e-7));
    }
}

// =============================================================================
// Task 2D-02-001-03: DOF Tracking, Conflicts & 20-Constraint Benchmark
// =============================================================================

TEST_CASE("CAD2D Solver: Under-Constrained DOF, Redundant Detection & Over-Constrained Conflicts") {
    // 1. Under-constrained free segment
    {
        ConstraintSystem2D sys;
        auto p0 = sys.add_point(0.0, 0.0, true);
        auto p1 = sys.add_point(3.0, 4.0, false); // 2 free params
        auto report = SketchSolver2D::solve(sys);
        CHECK(report.dof.remaining_dof == 2);
        CHECK(report.dof.status == DofStatus::UnderConstrained);

        sys.add_constraint(std::make_shared<HorizontalConstraint>(p0, p1));
        report = SketchSolver2D::solve(sys);
        CHECK(report.dof.remaining_dof == 1);
    }

    // 2. Redundant consistent constraints
    {
        ConstraintSystem2D sys;
        auto p0 = sys.add_point(0.0, 0.0, true);
        auto p1 = sys.add_point(5.0, 0.5, false);
        auto l = sys.add_line(p0, p1);
        sys.add_constraint(std::make_shared<HorizontalConstraint>(l));
        sys.add_constraint(std::make_shared<LineLengthConstraint>(l, 5.0));
        sys.add_constraint(std::make_shared<PointDistanceConstraint>(p0, p1, 5.0)); // Redundant consistent

        auto report = SketchSolver2D::solve(sys);
        REQUIRE(report.converged);
        CHECK(report.dof.remaining_dof == 0);
        CHECK(report.dof.redundant_rows == 1);
        CHECK(report.dof.status == DofStatus::RedundantConsistent);
        CHECK(report.dof.conflicting_constraint_ids.empty());
    }

    // 3. Conflicting over-constrained dimensions flagged in conflicting_constraint_ids
    {
        ConstraintSystem2D sys;
        auto p0 = sys.add_point(0.0, 0.0, true);
        auto p1 = sys.add_point(8.0, 0.0, false);
        auto l = sys.add_line(p0, p1);
        sys.add_constraint(std::make_shared<HorizontalConstraint>(l));
        size_t id1 = sys.add_constraint(std::make_shared<LineLengthConstraint>(l, 10.0));
        size_t id2 = sys.add_constraint(std::make_shared<LineLengthConstraint>(l, 14.0));

        auto report = SketchSolver2D::solve(sys);
        CHECK(!report.converged);
        CHECK(report.dof.status == DofStatus::OverConstrainedConflict);
        REQUIRE(!report.dof.conflicting_constraint_ids.empty());
        // Check that at least one of the conflicting length constraint IDs is flagged
        bool has_conflict = false;
        for (size_t cid : report.dof.conflicting_constraint_ids) {
            if (cid == id1 || cid == id2) has_conflict = true;
        }
        CHECK(has_conflict);
    }
}

TEST_CASE("CAD2D Solver: 20-Constraint Parametric Closed Polygon Benchmark (< 5ms)") {
    // 10-sided constrained polygon chain: 20 parameters (with P0 fixed -> 18 free DOFs), 18+ coupled constraints
    ConstraintSystem2D sys;
    const int N = 10;
    std::vector<ParamPoint2D> pts;
    double R_init = 12.0;
    for (int i = 0; i < N; ++i) {
        double a = 2.0 * PI * (static_cast<double>(i) / static_cast<double>(N));
        // Perturb initial positions intentionally
        double perturb = (i == 0) ? 0.0 : ((i % 2 == 0) ? 1.2 : -0.9);
        pts.push_back(sys.add_point((R_init + perturb) * std::cos(a), (R_init + perturb) * std::sin(a), i == 0));
    }

    auto center = sys.add_point(0.0, 0.0, true);
    auto circumcircle = sys.add_circle(0.0, 0.0, 10.0, true, true); // Fixed radius R = 10.0

    // 10 PointOnCircle constraints + 9 EqualLength constraints + 1 LineLength/Closing = 20 constraints!
    for (int i = 0; i < N; ++i) {
        sys.add_constraint(std::make_shared<PointOnCircleConstraint>(pts[i], circumcircle));
    }
    std::vector<ParamLine2D> edges;
    for (int i = 0; i < N; ++i) {
        edges.push_back(sys.add_line(pts[i], pts[(i + 1) % N]));
    }
    for (int i = 0; i + 1 < N; ++i) {
        sys.add_constraint(std::make_shared<EqualLengthConstraint>(edges[i], edges[i + 1]));
    }
    // Pin exact first vertex on x-axis already fixed (0,0)? Wait: p0 is at (12, 0), fix onto R=10 -> keep p0 free horizontally? Let's release p0.x and keep p0.y=0 fixed!
    sys.fix_param(pts[0].ix, false); // 19 free DOFs total: let's add edge length to get exact decagon or leave DOF=0!
    // P0.y is fixed (0), P0 on circle R=10 => P0=(10,0), plus 9 equal edges around circle of R=10 => 10+9 = 19 constraints on 19 free DOFs!
    // Add 1 Parallel edge constraint (top & bottom horizontal for 10-gon symmetry) = 20 constraints total!
    sys.add_constraint(std::make_shared<ParallelConstraint>(edges[0], edges[5]));
    (void)center;

    auto report = SketchSolver2D::solve(sys);
    REQUIRE(report.converged);
    CHECK(report.residual_norm < 1e-8);
    CHECK(report.solve_time_ms < 5.0);

    // Every vertex should lie precisely on R = 10.0
    for (int i = 0; i < N; ++i) {
        CHECK(is_approx(sys.get_point(pts[i]).length(), 10.0, 1e-7));
    }
    // Every edge of regular decagon on R=10 has exact length 2 * 10 * sin(pi/10)
    double expected_side = 2.0 * 10.0 * std::sin(PI / 10.0);
    for (int i = 0; i < N; ++i) {
        double edge_len = (sys.get_point(pts[(i + 1) % N]) - sys.get_point(pts[i])).length();
        CHECK(is_approx(edge_len, expected_side, 1e-6));
    }
}
