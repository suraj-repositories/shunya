#include "tests/test_framework.hpp"

#include "shunya/cad2d/sketch_viewport_2d.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/snap_manager.hpp"
#include "shunya/cad2d/polar_guide.hpp"

#include <chrono>
#include <cmath>
#include <memory>

using namespace shunya::cad2d;
using shunya::core::base::math::PI;

namespace {
bool is_approx(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps;
}
} // namespace

// =============================================================================
// Task 2D-01-002-01: Interactive Sketch Viewport Multi-Scale Transform & Grid
// =============================================================================

TEST_CASE("CAD2D Sketch: Multi-Scale Transform Inverses (Sub-Micron to Kilometer) & Zoom Anchor") {
    SketchViewport2D vp;
    vp.viewport_width = 1280.0;
    vp.viewport_height = 720.0;

    // 1. Sub-micron scale (1e8 px/m -> 100 px = 1 micrometer)
    vp.pixels_per_unit = 1e8;
    vp.pan_world = {1.5e-6, -2.25e-6};
    Point2D micron_pt{1.85e-6, -2.0e-6};
    Point2D s_mu = vp.world_to_screen(micron_pt);
    Point2D w_mu = vp.screen_to_world(s_mu);
    CHECK(w_mu.approx_equals(micron_pt, 1e-12));

    auto grid_mu = vp.compute_adaptive_grid(14.0);
    CHECK(grid_mu.minor_step > 0.0 && grid_mu.minor_step < 1e-5);
    CHECK(!grid_mu.vertical_lines.empty());

    // 2. Kilometer scale (0.05 px/m -> 50 px = 1 km)
    vp.pixels_per_unit = 0.05;
    vp.pan_world = {12500.0, -8400.0};
    Point2D km_pt{14200.0, -6100.0};
    Point2D s_km = vp.world_to_screen(km_pt);
    Point2D w_km = vp.screen_to_world(s_km);
    CHECK(w_km.approx_equals(km_pt, 1e-9));

    auto grid_km = vp.compute_adaptive_grid(14.0);
    CHECK(grid_km.minor_step >= 100.0);

    // 3. Zoom anchor invariance
    vp.pixels_per_unit = 200.0;
    vp.pan_world = {1.0, 2.0};
    Point2D cursor_px{420.0, 290.0};
    Point2D before_world = vp.screen_to_world(cursor_px);
    vp.zoom_at_screen(cursor_px, 2.5);
    Point2D after_world = vp.screen_to_world(cursor_px);
    CHECK(before_world.approx_equals(after_world, 1e-11));

    // 4. Stress test 10,000 entities pan/zoom transform speed
    SketchDocument2D doc;
    for (int i = 0; i < 10000; ++i) {
        double x = static_cast<double>(i % 100);
        double y = static_cast<double>(i / 100);
        doc.add_entity(std::make_shared<LineSegment2D>(Point2D{x, y}, Point2D{x + 0.8, y + 0.8}));
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    SnapTargetManager snap_mgr(2.0);
    snap_mgr.rebuild_index(doc);
    for (int k = 0; k < 10000; ++k) {
        (void)vp.world_to_screen(Point2D{static_cast<double>(k), 0.0});
    }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
    CHECK(ms < 250.0);
}

// =============================================================================
// Task 2D-01-002-02: Analytical Geometric Object Snapping
// =============================================================================

TEST_CASE("CAD2D Sketch: Analytical Intersections (Line-Line, Line-Circle, Circle-Circle, Arc)") {
    // 1. Line x Line
    LineSegment2D l1({0.0, 0.0}, {10.0, 10.0});
    LineSegment2D l2({0.0, 10.0}, {10.0, 0.0});
    auto ll = SnapTargetManager::intersect_curves(l1, l2);
    REQUIRE(ll.size() == 1);
    CHECK(ll[0].approx_equals({5.0, 5.0}, 1e-9));

    // 2. Line x Circle (y = 3 crossing circle x^2 + y^2 = 25 -> x = +/- 4)
    Circle2D c1({0.0, 0.0}, 5.0);
    LineSegment2D chord({-10.0, 3.0}, {10.0, 3.0});
    auto lc = SnapTargetManager::intersect_curves(chord, c1);
    REQUIRE(lc.size() == 2);
    CHECK(is_approx(lc[0].y, 3.0));
    CHECK(is_approx(std::abs(lc[0].x), 4.0));
    CHECK(is_approx(std::abs(lc[1].x), 4.0));

    // 3. Line x Arc (upper-right quadrant arc [0, pi/2] only catches x=+4, ignores x=-4)
    Arc2D arc_q1({0.0, 0.0}, 5.0, 0.0, 0.5 * PI);
    auto la = SnapTargetManager::intersect_curves(chord, arc_q1);
    REQUIRE(la.size() == 1);
    CHECK(la[0].approx_equals({4.0, 3.0}, 1e-9));

    // 4. Circle x Circle (centers (0,0) and (6,0), radius 5 -> x=3, y=+/-4)
    Circle2D c2({6.0, 0.0}, 5.0);
    auto cc = SnapTargetManager::intersect_curves(c1, c2);
    REQUIRE(cc.size() == 2);
    CHECK(is_approx(cc[0].x, 3.0));
    CHECK(is_approx(std::abs(cc[0].y), 4.0));

    // 5. Tangent points from external anchor (10, 0) onto circle (0, 0), R=5 -> x = R^2 / d = 25/10 = 2.5
    auto tpts = SnapTargetManager::find_tangent_points({10.0, 0.0}, c1);
    REQUIRE(tpts.size() == 2);
    CHECK(is_approx(tpts[0].x, 2.5, 1e-9));
    // Verify tangent orthogonality: (T - Center) . (Anchor - T) == 0
    CHECK(is_approx((tpts[0] - c1.center).dot(Point2D{10.0, 0.0} - tpts[0]), 0.0, 1e-9));
}

TEST_CASE("CAD2D Sketch: Spatial SnapTargetManager Endpoint, Midpoint, Center, Quadrant & Pixel Radius") {
    SketchDocument2D doc;
    doc.add_entity(std::make_shared<LineSegment2D>(Point2D{-10.0, 3.0}, Point2D{10.0, 3.0}));
    doc.add_entity(std::make_shared<Circle2D>(Point2D{0.0, 0.0}, 5.0));

    SketchViewport2D vp;
    vp.viewport_width = 1000.0;
    vp.viewport_height = 1000.0;
    vp.pixels_per_unit = 100.0; // 1 px = 0.01 world units -> 10 px tolerance = 0.10 world units
    vp.pan_world = {0.0, 0.0};

    SnapTargetManager mgr(2.0);
    mgr.rebuild_index(doc);

    // Within 5px of (4, 3) Intersection
    auto s_inter = mgr.query_snap(doc, vp, {4.03, 3.04}, 10.0);
    REQUIRE(s_inter.has_value());
    CHECK(s_inter->type == SnapType::Intersection);
    CHECK(s_inter->world_pos.approx_equals({4.0, 3.0}, 1e-9));

    // Within 5px of (0, 5) Circle Quadrant
    auto s_quad = mgr.query_snap(doc, vp, {0.02, 4.96}, 10.0);
    REQUIRE(s_quad.has_value());
    CHECK(s_quad->type == SnapType::Quadrant);
    CHECK(s_quad->world_pos.approx_equals({0.0, 5.0}, 1e-9));

    // Within 5px of (0, 0) Center
    auto s_ctr = mgr.query_snap(doc, vp, {-0.04, 0.03}, 10.0);
    REQUIRE(s_ctr.has_value());
    CHECK(s_ctr->type == SnapType::Center);
    CHECK(s_ctr->world_pos.approx_equals({0.0, 0.0}, 1e-9));

    // Outside 10px radius -> no snap
    auto s_none = mgr.query_snap(doc, vp, {2.5, 2.5}, 10.0);
    CHECK(!s_none.has_value());
}

// =============================================================================
// Task 2D-01-002-03: Dynamic Polar Guides & Construction Layer Exclusion
// =============================================================================

TEST_CASE("CAD2D Sketch: PolarTrackingEngine & Construction Geometry Profile Exclusion") {
    PolarTrackingEngine polar;
    polar.enabled = true;
    polar.snap_threshold_deg = 1.0;

    // 45.3 deg from (1, 1) -> snaps tightly to exact 45 deg
    double r = 10.0;
    double ang_test = 45.3 * (PI / 180.0);
    auto match45 = polar.evaluate({1.0, 1.0}, {1.0 + r * std::cos(ang_test), 1.0 + r * std::sin(ang_test)});
    REQUIRE(match45.active);
    CHECK(is_approx(match45.snapped_angle_deg, 45.0, 1e-9));
    // Verify snapped vector lies along (cos 45, sin 45)
    Vec2d delta = match45.snapped_point - Point2D{1.0, 1.0};
    CHECK(is_approx(delta.x, delta.y, 1e-9));

    // 18.0 deg -> far from 0/30 deg guides -> does not snap
    auto nomatch = polar.evaluate({0.0, 0.0}, {10.0 * std::cos(18.0 * PI / 180.0), 10.0 * std::sin(18.0 * PI / 180.0)});
    CHECK(!nomatch.active);

    // Construction Layer exclusion from solid extrusion profiles
    SketchDocument2D doc;
    // Closed solid triangle formed by 3 connected LineSegments
    doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{4.0, 0.0}), false);
    doc.add_entity(std::make_shared<LineSegment2D>(Point2D{4.0, 0.0}, Point2D{0.0, 3.0}), false);
    doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 3.0}, Point2D{0.0, 0.0}), false);

    // Construction circle + construction square: MUST BE EXCLUDED from solid profile loops!
    doc.add_entity(std::make_shared<Circle2D>(Point2D{2.0, 2.0}, 10.0), true);
    doc.add_entity(std::make_shared<Polyline2D>(
        std::vector<Point2D>{{10, 10}, {20, 10}, {20, 20}, {10, 20}}, true
    ), true);

    CHECK(doc.solid_entities().size() == 3);
    CHECK(doc.construction_entities().size() == 2);

    auto loops = doc.extract_solid_profile_loops();
    REQUIRE(loops.size() == 1); // Only the 1 stitched solid triangle!
    CHECK(loops[0].size() == 3);
    CHECK(is_approx(PolygonBooleanEngine::area(loops[0]), 6.0, 1e-6));
}
