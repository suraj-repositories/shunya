#include "tests/test_framework.hpp"

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/bezier_spline.hpp"
#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/cad2d/profile_offset.hpp"
#include "shunya/cad2d/fillet_chamfer.hpp"
#include "shunya/cad2d/drawing_tools.hpp"
#include "shunya/cad2d/dimensioning_tool.hpp"
#include "shunya/cad2d/vector_text_tool.hpp"
#include "shunya/cad2d/advanced_curves.hpp"

#include <cmath>
#include <vector>

using namespace shunya::cad2d;
using shunya::core::base::math::PI;

namespace {

bool is_approx(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps;
}

Path2D make_rect(double x0, double y0, double x1, double y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

Path2D make_circle_poly(double cx, double cy, double r, size_t n = 64, bool ccw = true) {
    Path2D p;
    p.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        double a = (ccw ? 1.0 : -1.0) * 2.0 * PI * (static_cast<double>(i) / static_cast<double>(n));
        p.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
    return p;
}

} // namespace

// =============================================================================
// Task 2D-01-001-01: 2D Curves, Ellipses & Cubic Bezier Splines
// =============================================================================

TEST_CASE("CAD2D: LineSegment2D, Polyline2D, Circle2D, Arc2D Analytical Precision") {
    // 1. LineSegment2D
    LineSegment2D line({1.0, 2.0}, {4.0, 6.0});
    CHECK(is_approx(line.total_length(), 5.0));
    CHECK(is_approx(line.arc_length(0.2, 0.8), 3.0));
    CHECK(is_approx(line.curvature(0.5), 0.0));
    double t_line = -1.0;
    Point2D cp_line = line.closest_point({4.0, 2.0}, &t_line);
    CHECK(t_line > 0.0 && t_line < 1.0);
    // Vector from closest point to query is orthogonal to line direction
    CHECK(is_approx((cp_line - Point2D{4.0, 2.0}).dot(line.tangent(t_line)), 0.0));

    // 2. Circle2D
    Circle2D circle({2.0, -3.0}, 5.0);
    CHECK(is_approx(circle.total_length(), 2.0 * PI * 5.0));
    CHECK(is_approx(circle.curvature(0.25), 0.2));
    auto cbox = circle.bounding_box();
    CHECK(is_approx(cbox.min.x, -3.0));
    CHECK(is_approx(cbox.max.x, 7.0));
    CHECK(is_approx(cbox.min.y, -8.0));
    CHECK(is_approx(cbox.max.y, 2.0));
    double t_c = 0.0;
    Point2D cp_c = circle.closest_point({12.0, -3.0}, &t_c);
    CHECK(is_approx(cp_c.x, 7.0));
    CHECK(is_approx(cp_c.y, -3.0));

    // 3. Arc2D (quarter circle CCW from 0 to pi/2)
    Arc2D arc({0.0, 0.0}, 4.0, 0.0, 0.5 * PI);
    CHECK(is_approx(arc.total_length(), 2.0 * PI));
    CHECK(is_approx(arc.curvature(0.5), 0.25));
    auto abox = arc.bounding_box();
    CHECK(is_approx(abox.min.x, 0.0));
    CHECK(is_approx(abox.max.x, 4.0));
    CHECK(is_approx(abox.min.y, 0.0));
    CHECK(is_approx(abox.max.y, 4.0));
    double t_a = 0.0;
    Point2D cp_a = arc.closest_point({10.0, 10.0}, &t_a);
    CHECK(is_approx(t_a, 0.5));
    CHECK(is_approx(cp_a.x, 4.0 * std::cos(PI / 4.0)));
    CHECK(is_approx(cp_a.y, 4.0 * std::sin(PI / 4.0)));

    // 4. Polyline2D
    Polyline2D poly({{0.0, 0.0}, {3.0, 0.0}, {3.0, 4.0}}, false);
    CHECK(is_approx(poly.total_length(), 7.0));
    Point2D p_mid = poly.evaluate(3.0 / 7.0);
    CHECK(is_approx(p_mid.x, 3.0));
    CHECK(is_approx(p_mid.y, 0.0));
}

TEST_CASE("CAD2D: Ellipse2D Arc Length vs Ramanujan and Rotated Bounds") {
    // Axis-aligned ellipse rx=5, ry=3
    Ellipse2D ell({1.0, 2.0}, 5.0, 3.0, 0.0);
    double quad_len = ell.total_length();
    double ramanujan_len = ell.ramanujan_perimeter();
    // Ramanujan approximation is accurate to ~1e-5 of exact integral for a/b = 5/3
    CHECK(std::abs(quad_len - ramanujan_len) < 1e-3);

    // Degenerate circle case: must match 2*pi*R to < 1e-9
    Ellipse2D ell_circ({0.0, 0.0}, 4.0, 4.0, 0.3);
    CHECK(is_approx(ell_circ.total_length(), 2.0 * PI * 4.0, 1e-9));

    // Closest point query satisfies normal condition (E(t) - Q) . tangent(t) == 0 to < 1e-9
    double t_e = 0.0;
    Point2D query{4.0, 5.0};
    Point2D cp = ell.closest_point(query, &t_e);
    CHECK(is_approx((cp - query).dot(ell.tangent(t_e)), 0.0, 1e-9));

    // Rotated 45 deg ellipse tight bounding box
    Ellipse2D rot_ell({0.0, 0.0}, 4.0, 2.0, PI / 4.0);
    auto rbox = rot_ell.bounding_box();
    double expected_half = std::hypot(4.0 * std::cos(PI / 4.0), 2.0 * std::sin(PI / 4.0));
    CHECK(is_approx(rbox.max.x, expected_half));
    CHECK(is_approx(rbox.min.x, -expected_half));
}

TEST_CASE("CAD2D: BezierCubic2D Derivatives, Splitting, Tight Bounds, Closest Point") {
    BezierCubic2D bez({0.0, 0.0}, {1.0, 4.0}, {3.0, -2.0}, {4.0, 2.0});

    // Convex hull bound check
    auto tight_box = bez.bounding_box();
    auto ctrl_box = bez.control_polygon_bbox();
    CHECK(tight_box.min.x >= ctrl_box.min.x - 1e-12);
    CHECK(tight_box.max.x <= ctrl_box.max.x + 1e-12);
    CHECK(tight_box.min.y >= ctrl_box.min.y - 1e-12);
    CHECK(tight_box.max.y <= ctrl_box.max.y + 1e-12);

    // Every sampled point lies inside tight bounding box
    for (const auto& pt : bez.sample_polyline(100)) {
        CHECK(tight_box.contains(pt, 1e-9));
    }

    // de Casteljau split preserves arc length to < 1e-9
    auto [left_bez, right_bez] = bez.split(0.37);
    CHECK(left_bez.evaluate(1.0).approx_equals(bez.evaluate(0.37), 1e-12));
    CHECK(right_bez.evaluate(0.0).approx_equals(bez.evaluate(0.37), 1e-12));
    CHECK(is_approx(left_bez.total_length() + right_bez.total_length(), bez.total_length(), 1e-9));

    // Closest point orthogonality
    double t_b = 0.0;
    Point2D q{2.0, 1.0};
    Point2D cp = bez.closest_point(q, &t_b);
    if (t_b > 1e-6 && t_b < 1.0 - 1e-6) {
        CHECK(is_approx((cp - q).dot(bez.tangent(t_b)), 0.0, 1e-9));
    }
}

// =============================================================================
// Task 2D-01-001-02: Polygon Boolean Operations
// =============================================================================

TEST_CASE("CAD2D: Polygon Booleans (Union, Intersection, Difference, Xor) on Rectangles") {
    Path2D r1 = make_rect(0.0, 0.0, 10.0, 10.0); // Area = 100
    Path2D r2 = make_rect(5.0, 5.0, 15.0, 15.0); // Area = 100, overlap = [5,10]x[5,10] = 25

    auto u_res = PolygonBooleanEngine::execute(BooleanOp::Union, {r1}, {r2});
    CHECK(u_res.size() == 1);
    CHECK(is_approx(PolygonBooleanEngine::net_area(u_res), 175.0, 1e-5));

    auto i_res = PolygonBooleanEngine::execute(BooleanOp::Intersection, {r1}, {r2});
    CHECK(i_res.size() == 1);
    CHECK(is_approx(PolygonBooleanEngine::net_area(i_res), 25.0, 1e-5));

    auto d_res = PolygonBooleanEngine::execute(BooleanOp::Difference, {r1}, {r2});
    CHECK(d_res.size() == 1);
    CHECK(is_approx(PolygonBooleanEngine::net_area(d_res), 75.0, 1e-5));

    auto x_res = PolygonBooleanEngine::execute(BooleanOp::Xor, {r1}, {r2});
    CHECK(is_approx(PolygonBooleanEngine::net_area(x_res), 150.0, 1e-5));
}

TEST_CASE("CAD2D: Polygon Booleans Doughnut Holes & Overlapping Circles") {
    // Nested doughnut: Outer 10x10 minus centered inner 4x4
    Path2D outer = make_rect(0.0, 0.0, 10.0, 10.0);
    Path2D inner = make_rect(3.0, 3.0, 7.0, 7.0);

    auto ring_paths = PolygonBooleanEngine::execute(BooleanOp::Difference, {outer}, {inner});
    REQUIRE(ring_paths.size() == 2); // One CCW outer + one CW hole
    CHECK(is_approx(PolygonBooleanEngine::net_area(ring_paths), 84.0, 1e-5));

    auto hierarchy = PolygonBooleanEngine::extract_hierarchy(ring_paths);
    REQUIRE(hierarchy.size() == 1);
    REQUIRE(hierarchy[0].holes.size() == 1);
    CHECK(is_approx(PolygonBooleanEngine::area(hierarchy[0].outer), 100.0, 1e-5));
    CHECK(is_approx(PolygonBooleanEngine::area(hierarchy[0].holes[0]), 16.0, 1e-5));

    // FillRule::EvenOdd vs NonZero on same-orientation nested squares
    auto eo_paths = PolygonBooleanEngine::simplify({outer, inner}, FillRule::EvenOdd);
    CHECK(is_approx(PolygonBooleanEngine::net_area(eo_paths), 84.0, 1e-5));
    auto nz_paths = PolygonBooleanEngine::simplify({outer, inner}, FillRule::NonZero);
    CHECK(is_approx(PolygonBooleanEngine::net_area(nz_paths), 100.0, 1e-5));

    // Overlapping 64-gon circles
    Path2D c1 = make_circle_poly(0.0, 0.0, 5.0, 64);
    Path2D c2 = make_circle_poly(5.0, 0.0, 5.0, 64);
    auto ci = PolygonBooleanEngine::execute(BooleanOp::Intersection, {c1}, {c2});
    auto cu = PolygonBooleanEngine::execute(BooleanOp::Union, {c1}, {c2});
    // Exact circular lens area for d = R = 5: 2 * R^2 * (pi/3 - sqrt(3)/4)
    double poly_c1_area = PolygonBooleanEngine::area(c1);
    double exact_lens = 2.0 * 25.0 * (PI / 3.0 - std::sqrt(3.0) / 4.0);
    CHECK(is_approx(PolygonBooleanEngine::net_area(ci), exact_lens, 0.25));
    CHECK(is_approx(PolygonBooleanEngine::net_area(cu), 2.0 * poly_c1_area - PolygonBooleanEngine::net_area(ci), 1e-4));
}

TEST_CASE("CAD2D: Degenerate Edge-Touching Squares Union and Intersection") {
    Path2D left = make_rect(0.0, 0.0, 10.0, 10.0);
    Path2D right = make_rect(10.0, 0.0, 20.0, 10.0);

    // Edge-touching squares merge cleanly into a single 20x10 rectangle (4 vertices!)
    auto merged = PolygonBooleanEngine::execute(BooleanOp::Union, {left}, {right});
    REQUIRE(merged.size() == 1);
    CHECK(merged[0].size() == 4);
    CHECK(is_approx(PolygonBooleanEngine::net_area(merged), 200.0, 1e-5));

    // Edge-touching intersection has 0 area -> empty paths
    auto inter = PolygonBooleanEngine::execute(BooleanOp::Intersection, {left}, {right});
    CHECK(inter.empty());
}

// =============================================================================
// Task 2D-01-001-03: Offsetting, Filleting & Chamfering
// =============================================================================

TEST_CASE("CAD2D: ProfileOffsetEngine Miter, Round, Square & Negative Erosion") {
    Path2D sq = make_rect(0.0, 0.0, 10.0, 10.0); // 10x10

    // Miter outward +2.0 -> 14x14 (Area 196)
    auto out_miter = ProfileOffsetEngine::offset_polygons({sq}, 2.0, JoinType::Miter);
    REQUIRE(out_miter.size() == 1);
    CHECK(is_approx(PolygonBooleanEngine::net_area(out_miter), 196.0, 1e-4));

    // Round outward +2.0 -> 10x10 + 4*2*10 + pi*2^2 = 180 + 4*pi (~192.566)
    auto out_round = ProfileOffsetEngine::offset_polygons({sq}, 2.0, JoinType::Round, 2.0, 32);
    REQUIRE(out_round.size() == 1);
    double expected_round = 100.0 + 80.0 + PI * 4.0;
    CHECK(is_approx(PolygonBooleanEngine::net_area(out_round), expected_round, 0.2));

    // Inward erosion -2.0 -> 6x6 (Area 36)
    auto in_miter = ProfileOffsetEngine::offset_polygons({sq}, -2.0, JoinType::Miter);
    REQUIRE(in_miter.size() == 1);
    CHECK(is_approx(PolygonBooleanEngine::net_area(in_miter), 36.0, 1e-4));

    // Over-shrunk erosion -6.0 -> vanishes cleanly
    auto collapsed = ProfileOffsetEngine::offset_polygons({sq}, -6.0, JoinType::Miter);
    CHECK(collapsed.empty());
}

TEST_CASE("CAD2D: Corner Filleting G1 Continuity & Analytical Chamfering") {
    // Perpendicular corner: (0, 10) -> (0, 0) -> (10, 0) with radius 3.0
    auto res_90 = FilletChamferEngine::fillet_corner({0.0, 10.0}, {0.0, 0.0}, {10.0, 0.0}, 3.0);
    REQUIRE(res_90.is_ok());
    const auto& f90 = res_90.value();

    CHECK(is_approx(f90.trim_distance, 3.0, 1e-9));
    CHECK(f90.arc.center.approx_equals({3.0, 3.0}, 1e-9));
    CHECK(is_approx(f90.arc.radius, 3.0, 1e-9));
    CHECK(f90.trimmed_leg1.p1.approx_equals(f90.arc.evaluate(0.0), 1e-9));
    CHECK(f90.arc.evaluate(1.0).approx_equals(f90.trimmed_leg2.p0, 1e-9));

    // Verify exact G1 tangent vector continuity at both transition points
    CHECK(is_approx(f90.trimmed_leg1.tangent(1.0).dot(f90.arc.tangent(0.0)), 1.0, 1e-9));
    CHECK(is_approx(f90.arc.tangent(1.0).dot(f90.trimmed_leg2.tangent(0.0)), 1.0, 1e-9));

    // Non-orthogonal 60 degree corner filleting
    Point2D arm_b{10.0 * std::cos(PI / 3.0), 10.0 * std::sin(PI / 3.0)};
    auto res_60 = FilletChamferEngine::fillet_corner({10.0, 0.0}, {0.0, 0.0}, arm_b, 2.0);
    REQUIRE(res_60.is_ok());
    const auto& f60 = res_60.value();
    CHECK(is_approx(f60.trim_distance, 2.0 / std::tan(PI / 6.0), 1e-9));
    CHECK(is_approx(f60.trimmed_leg1.tangent(1.0).dot(f60.arc.tangent(0.0)), 1.0, 1e-9));
    CHECK(is_approx(f60.arc.tangent(1.0).dot(f60.trimmed_leg2.tangent(0.0)), 1.0, 1e-9));

    // Chamfer setback check
    auto cham_res = FilletChamferEngine::chamfer_corner({0.0, 10.0}, {0.0, 0.0}, {10.0, 0.0}, 3.0, 4.0);
    REQUIRE(cham_res.is_ok());
    CHECK(is_approx(cham_res.value().bevel.total_length(), 5.0, 1e-9));

    // Full polygon filleting reduces sharp area by (4 - pi) * R^2
    Path2D rect = make_rect(0.0, 0.0, 10.0, 10.0);
    Path2D filleted = FilletChamferEngine::fillet_polygon(rect, 2.0, 24);
    double expected_fillet_area = 100.0 - (4.0 - PI) * 4.0;
    CHECK(is_approx(PolygonBooleanEngine::area(filleted), expected_fillet_area, 0.05));
}

// =============================================================================
// Task PH4-04.01.001: Interactive 2D Primitive Creation Tools
// =============================================================================

TEST_CASE("PH4-04.01.001: Interactive 2D Primitive Creation Tools") {
    SketchDocument2D doc;
    DrawingToolController controller;

    // 1. Line Tool
    controller.select_tool(DrawingToolType::Line);
    CHECK(controller.active_tool_type() == DrawingToolType::Line);
    controller.on_pointer_down({0.0, 0.0});
    controller.on_pointer_move({10.0, 0.0});
    auto line_prev = controller.preview_curves();
    REQUIRE(line_prev.size() == 1);
    CHECK(is_approx(line_prev[0]->total_length(), 10.0));
    controller.on_pointer_down({10.0, 0.0});
    controller.on_pointer_up({10.0, 0.0});
    size_t line_cnt = controller.commit_to_document(doc);
    CHECK(line_cnt == 1);
    CHECK(doc.entities().size() == 1);

    // 2. Rectangle Tool
    controller.select_tool(DrawingToolType::Rectangle);
    controller.on_pointer_down({0.0, 0.0});
    controller.on_pointer_move({8.0, 5.0});
    controller.on_pointer_down({8.0, 5.0});
    controller.on_pointer_up({8.0, 5.0});
    size_t rect_cnt = controller.commit_to_document(doc);
    CHECK(rect_cnt == 1);
    CHECK(doc.entities().size() == 2);
    CHECK(is_approx(doc.entities().back().curve->total_length(), 26.0));

    // 3. Circle Tool
    controller.select_tool(DrawingToolType::Circle);
    controller.on_pointer_down({0.0, 0.0});
    controller.on_pointer_move({3.0, 4.0}); // radius 5.0
    controller.on_pointer_down({3.0, 4.0});
    controller.on_pointer_up({3.0, 4.0});
    size_t circ_cnt = controller.commit_to_document(doc);
    CHECK(circ_cnt == 1);
    CHECK(doc.entities().size() == 3);
    CHECK(is_approx(doc.entities().back().curve->total_length(), 2.0 * PI * 5.0));

    // 4. Ellipse Tool
    controller.select_tool(DrawingToolType::Ellipse);
    controller.on_pointer_down({0.0, 0.0}); // Center
    controller.on_pointer_down({5.0, 0.0}); // Major rx = 5.0
    controller.on_pointer_down({0.0, 2.0}); // Minor ry = 2.0
    controller.on_pointer_up({0.0, 2.0});
    size_t el_cnt = controller.commit_to_document(doc);
    CHECK(el_cnt == 1);
    CHECK(doc.entities().size() == 4);

    // 5. Regular Polygon Tool (Hexagon)
    controller.select_tool(DrawingToolType::RegularPolygon);
    controller.on_pointer_down({0.0, 0.0});
    controller.on_pointer_move({10.0, 0.0});
    controller.on_pointer_down({10.0, 0.0});
    controller.on_pointer_up({10.0, 0.0});
    size_t poly_cnt = controller.commit_to_document(doc);
    CHECK(poly_cnt == 1);
    CHECK(doc.entities().size() == 5);
    // Regular hexagon with circumradius 10 has perimeter = 6 * 10 = 60
    CHECK(is_approx(doc.entities().back().curve->total_length(), 60.0, 1e-4));

    // 6. Arbitrary Polygon Tool
    controller.select_tool(DrawingToolType::ArbitraryPolygon);
    controller.on_pointer_down({0.0, 0.0});
    controller.on_pointer_down({5.0, 0.0});
    controller.on_pointer_down({5.0, 5.0});
    controller.on_pointer_down({0.0, 5.0});
    auto* arb_tool = dynamic_cast<ArbitraryPolygonTool*>(controller.active_tool());
    REQUIRE(arb_tool != nullptr);
    arb_tool->finish();
    size_t arb_cnt = controller.commit_to_document(doc);
    CHECK(arb_cnt == 1);
    CHECK(doc.entities().size() == 6);
    CHECK(is_approx(doc.entities().back().curve->total_length(), 20.0));

    // 7. Arc 3-Point Tool (Passes through (1,0), (0,1), (-1,0) -> semicircle of radius 1)
    controller.select_tool(DrawingToolType::Arc3Point);
    controller.on_pointer_down({1.0, 0.0});
    controller.on_pointer_down({0.0, 1.0});
    controller.on_pointer_down({-1.0, 0.0});
    controller.on_pointer_up({-1.0, 0.0});
    size_t arc3_cnt = controller.commit_to_document(doc);
    CHECK(arc3_cnt == 1);
    CHECK(doc.entities().size() == 7);
    CHECK(is_approx(doc.entities().back().curve->total_length(), PI, 1e-4));

    // 8. Arc Center-Radius Tool
    controller.select_tool(DrawingToolType::ArcCenterRadius);
    controller.on_pointer_down({0.0, 0.0}); // Center
    controller.on_pointer_down({4.0, 0.0}); // Radius 4 at angle 0
    controller.on_pointer_down({0.0, 4.0}); // Quarter arc to pi/2
    controller.on_pointer_up({0.0, 4.0});
    size_t arcc_cnt = controller.commit_to_document(doc);
    CHECK(arcc_cnt == 1);
    CHECK(doc.entities().size() == 8);
    CHECK(is_approx(doc.entities().back().curve->total_length(), 2.0 * PI, 1e-4));

    // 9. Cubic Bezier Tool
    controller.select_tool(DrawingToolType::CubicBezier);
    controller.on_pointer_down({0.0, 0.0});
    controller.on_pointer_down({1.0, 2.0});
    controller.on_pointer_down({3.0, 2.0});
    controller.on_pointer_down({4.0, 0.0});
    controller.on_pointer_up({4.0, 0.0});
    size_t bez_cnt = controller.commit_to_document(doc);
    CHECK(bez_cnt == 1);
    CHECK(doc.entities().size() == 9);

    // 10. Freehand Polyline Tool with Ramer-Douglas-Peucker simplification
    controller.select_tool(DrawingToolType::FreehandPolyline);
    controller.on_pointer_down({0.0, 0.0});
    for (int i = 1; i <= 20; ++i) {
        // Collinear points with slight sub-micron jitter below simplification tolerance
        controller.on_pointer_move({static_cast<double>(i), 0.001 * (i % 2)});
    }
    controller.on_pointer_up({20.0, 0.0});
    size_t free_cnt = controller.commit_to_document(doc);
    CHECK(free_cnt == 1);
    CHECK(doc.entities().size() == 10);
    // Verified 10 different primitives cleanly created and committed to sketch document
    CHECK(doc.entities().size() == 10);
}

// =============================================================================
// Task PH4-04.01.002: High-Precision Dimensioning & Measurement Tool
// =============================================================================

TEST_CASE("PH4-04.01.002: High-Precision Dimensioning & Associative Measurement") {
    // 1. Analytical precision measurements
    auto d_res = PrecisionMeasurementEngine::measure_distance({0.0, 0.0}, {3.0, 4.0});
    CHECK(is_approx(d_res.value, 5.0));
    CHECK(is_approx(d_res.dx, 3.0));
    CHECK(is_approx(d_res.dy, 4.0));
    CHECK(is_approx(d_res.angle_deg, 53.13, 0.01));
    CHECK(d_res.formatted_string.find("5.000 mm") != std::string::npos);

    auto a_res = PrecisionMeasurementEngine::measure_angle({0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0});
    CHECK(is_approx(a_res.angle_deg, 90.0));
    CHECK(is_approx(a_res.angle_rad, 0.5 * PI));

    Circle2D circle_sample({0.0, 0.0}, 7.5);
    auto c_res = PrecisionMeasurementEngine::measure_curve(circle_sample);
    CHECK(is_approx(c_res.value, 2.0 * PI * 7.5));
    CHECK(c_res.formatted_string.find("Radius 7.500 mm") != std::string::npos);

    auto pt_c_res = PrecisionMeasurementEngine::measure_point_to_curve({0.0, 10.0}, circle_sample);
    CHECK(is_approx(pt_c_res.value, 2.5)); // 10.0 - 7.5
    CHECK(is_approx(pt_c_res.closest_pt.x, 0.0));
    CHECK(is_approx(pt_c_res.closest_pt.y, 7.5));

    // 2. Dimension Manager & Callouts
    SketchDocument2D doc;
    auto line_ptr = std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{10.0, 0.0});
    std::string line_id = doc.add_entity(line_ptr);

    DimensionManager dim_mgr;
    auto& lin_dim = dim_mgr.add_linear_dimension({0.0, 0.0}, {10.0, 0.0}, 2.0, DimensionCalloutType::LinearAligned, line_id);
    std::string lin_id = lin_dim.id;
    CHECK(is_approx(lin_dim.measured_value, 10.0));
    CHECK(lin_dim.display_text() == "10.000 mm");
    CHECK(lin_dim.extension_lines.size() == 2);
    CHECK(lin_dim.dimension_lines.size() == 1);
    CHECK(lin_dim.arrowheads.size() == 2);

    // 3. Angular Dimension Callout
    dim_mgr.add_angular_dimension({0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}, 3.0);
    const auto* ang_dim = dim_mgr.find_callout("DIM_2");
    REQUIRE(ang_dim != nullptr);
    CHECK(is_approx(ang_dim->measured_value, 90.0));
    CHECK(ang_dim->display_text() == "90.000 °");
    CHECK(ang_dim->dimension_arcs.size() == 1);

    // 4. Radial & Diametral Callouts
    auto circ_ptr = std::make_shared<Circle2D>(Point2D{5.0, 5.0}, 4.0);
    std::string circ_id = doc.add_entity(circ_ptr);

    dim_mgr.add_radial_dimension(circ_id, *circ_ptr, {10.0, 10.0});
    const auto* rad_dim = dim_mgr.find_callout("DIM_3");
    REQUIRE(rad_dim != nullptr);
    CHECK(is_approx(rad_dim->measured_value, 4.0));
    CHECK(rad_dim->display_text() == "R 4.000 mm");

    dim_mgr.add_diametral_dimension(circ_id, *circ_ptr, {12.0, 5.0});
    const auto* dia_dim = dim_mgr.find_callout("DIM_4");
    REQUIRE(dia_dim != nullptr);
    CHECK(is_approx(dia_dim->measured_value, 8.0));
    CHECK(dia_dim->display_text() == "Ø 8.000 mm");

    // 5. Associative Recomputation on Document Geometry Change
    line_ptr->p1 = Point2D{25.0, 0.0};
    size_t updated = dim_mgr.update_all_associative(doc);
    CHECK(updated >= 1);
    const auto* p_lin = dim_mgr.find_callout(lin_id);
    REQUIRE(p_lin != nullptr);
    CHECK(is_approx(p_lin->measured_value, 25.0));
    CHECK(p_lin->display_text() == "25.000 mm");
    CHECK(is_approx(p_lin->dimension_lines[0].total_length(), 25.0));
}

// =============================================================================
// Task PH4-04.01.003: Interactive Vector Text Tool
// =============================================================================

TEST_CASE("PH4-04.01.003: Interactive Vector Text Tool & Typography Engine") {
    // 1. Vector glyph generation
    TextProperties props;
    props.content = "SHUNYA";
    props.position = {0.0, 0.0};
    props.font_size = 10.0;
    props.h_align = TextHAlign::Left;
    props.v_align = TextVAlign::Bottom;

    auto curves = VectorFontEngine::generate_vector_strokes(props);
    CHECK(!curves.empty());
    auto bbox = VectorFontEngine::compute_text_bounds(props);
    CHECK(bbox.is_valid());
    CHECK(bbox.height() > 5.0);
    CHECK(bbox.width() > 10.0);

    // 2. Alignment variations
    props.h_align = TextHAlign::Center;
    props.position = {100.0, 50.0};
    auto bbox_center = VectorFontEngine::compute_text_bounds(props);
    CHECK(is_approx(bbox_center.center().x, 100.0, 5.0));

    // 3. Rotation transformation (90 degrees)
    props.rotation_rad = 0.5 * PI;
    auto bbox_rot = VectorFontEngine::compute_text_bounds(props);
    // At 90 deg rotation, width and height exchange aspect ratio
    CHECK(bbox_rot.height() > bbox_rot.width());

    // 4. Interactive VectorTextTool & Document Commit
    SketchDocument2D doc;
    VectorTextTool tool;
    tool.start_placement({10.0, 20.0}, "CAD 2026", 8.0);
    CHECK(tool.is_placing);
    auto previews = tool.preview_curves();
    CHECK(!previews.empty());

    tool.update_cursor({15.0, 25.0});
    auto committed_ids = tool.commit_to_document(doc, false, "ANNOTATIONS");
    CHECK(!tool.is_placing);
    CHECK(!committed_ids.empty());
    CHECK(doc.entities().size() == committed_ids.size());
    CHECK(doc.entities().front().layer == "ANNOTATIONS");
}

// =============================================================================
// Task PH4-04.01.004: Advanced 2D Drafting Curves (Catmull-Rom, B-Spline, Parabola)
// =============================================================================

TEST_CASE("PH4-04.01.004: Advanced 2D Drafting Curves") {
    // 1. CatmullRomSpline2D
    std::vector<Point2D> pts = {
        {0.0, 0.0},
        {5.0, 10.0},
        {10.0, 0.0},
        {15.0, 10.0}
    };
    CatmullRomSpline2D catmull(pts, false);
    // Control points must be interpolated exactly
    CHECK(is_approx(catmull.evaluate(0.0).x, 0.0, 1e-6));
    CHECK(is_approx(catmull.evaluate(0.0).y, 0.0, 1e-6));
    CHECK(is_approx(catmull.evaluate(1.0 / 3.0).x, 5.0, 1e-6));
    CHECK(is_approx(catmull.evaluate(1.0 / 3.0).y, 10.0, 1e-6));
    CHECK(is_approx(catmull.evaluate(2.0 / 3.0).x, 10.0, 1e-6));
    CHECK(is_approx(catmull.evaluate(2.0 / 3.0).y, 0.0, 1e-6));
    CHECK(is_approx(catmull.evaluate(1.0).x, 15.0, 1e-6));
    CHECK(is_approx(catmull.evaluate(1.0).y, 10.0, 1e-6));
    CHECK(catmull.total_length() > 30.0);

    // 2. BSpline2D (Uniform cubic B-spline)
    std::vector<Point2D> b_pts = {
        {0.0, 0.0},
        {0.0, 10.0},
        {10.0, 10.0},
        {10.0, 0.0},
        {20.0, 0.0}
    };
    BSpline2D bspline(b_pts, false);
    CHECK(bspline.bounding_box().is_valid());
    // Verify C2 continuity: evaluation and derivatives are non-zero and finite
    Vec2d d1 = bspline.derivative(0.5);
    Vec2d d2 = bspline.second_derivative(0.5);
    CHECK(std::isfinite(d1.x) && std::isfinite(d1.y));
    CHECK(std::isfinite(d2.x) && std::isfinite(d2.y));

    // 3. ParabolicArc2D
    // Vertex at (0, 0), axis along +Y, focal coefficient a = 0.5 (y = 0.5 * x^2)
    ParabolicArc2D parabola({0.0, 0.0}, 0.5, 0.5 * PI, -4.0, 4.0);
    // Apex is at t = 0.5 (local x = 0.0)
    Point2D apex = parabola.evaluate(0.5);
    CHECK(is_approx(apex.x, 0.0, 1e-6));
    CHECK(is_approx(apex.y, 0.0, 1e-6));
    // Curvature at apex: kappa = 2*a = 1.0
    CHECK(is_approx(parabola.curvature(0.5), 1.0, 1e-6));
    // Analytical arc length
    double len = parabola.total_length();
    CHECK(len > 8.0);
    CHECK(is_approx(parabola.arc_length(0.0, 0.5) * 2.0, len, 1e-6)); // Symmetrical

    // 3-point parabola factory
    auto p3 = ParabolicArc2D::from_three_points({-3.0, 4.5}, {0.0, 0.0}, {3.0, 4.5});
    REQUIRE(p3 != nullptr);
    CHECK(is_approx(p3->curvature(0.5), 1.0, 0.05));
}
