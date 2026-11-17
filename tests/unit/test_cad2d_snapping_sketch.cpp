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

// =============================================================================
// Task PH4-04.02.001: Intelligent Magnetic Snap Engine (7 Object Snaps)
// =============================================================================

TEST_CASE("PH4-04.02.001: Intelligent Magnetic Snap Engine (7 Snaps)") {
    SketchViewport2D vp;
    vp.viewport_width = 1000.0;
    vp.viewport_height = 1000.0;
    vp.pixels_per_unit = 100.0; // 100 px = 1.0 unit (10 px tolerance = 0.10 units)

    SketchDocument2D doc;
    // Line 1: (0, 0) to (10, 0)
    std::string l1_id = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{10.0, 0.0}));
    // Line 2: (5, -5) to (5, 5) -> intersects Line 1 at (5, 0)
    doc.add_entity(std::make_shared<LineSegment2D>(Point2D{5.0, -5.0}, Point2D{5.0, 5.0}));
    // Circle at (20, 20) with radius 5
    doc.add_entity(std::make_shared<Circle2D>(Point2D{20.0, 20.0}, 5.0));

    SnapTargetManager snap_mgr(1.0);
    snap_mgr.set_grid_snap_spacing(1.0);
    snap_mgr.rebuild_index(doc);

    // 1. Grid Snap (Snapping to (2.0, 3.0) from cursor (2.02, 3.01))
    auto s_grid = snap_mgr.query_snap(doc, vp, {2.02, 3.01}, 10.0, static_cast<uint32_t>(SnapType::Grid));
    REQUIRE(s_grid.has_value());
    CHECK(s_grid->type == SnapType::Grid);
    CHECK(is_approx(s_grid->world_pos.x, 2.0));
    CHECK(is_approx(s_grid->world_pos.y, 3.0));

    // 2. Endpoint Snap (Line 1 end at (10, 0))
    auto s_end = snap_mgr.query_snap(doc, vp, {10.03, 0.02}, 10.0, static_cast<uint32_t>(SnapType::Endpoint));
    REQUIRE(s_end.has_value());
    CHECK(s_end->type == SnapType::Endpoint);
    CHECK(is_approx(s_end->world_pos.x, 10.0));
    CHECK(is_approx(s_end->world_pos.y, 0.0));

    // 3. Midpoint Snap (Line 1 midpoint at (5, 0) with Midpoint mask only)
    auto s_mid = snap_mgr.query_snap(doc, vp, {5.02, 0.03}, 10.0, static_cast<uint32_t>(SnapType::Midpoint));
    REQUIRE(s_mid.has_value());
    CHECK(s_mid->type == SnapType::Midpoint);
    CHECK(is_approx(s_mid->world_pos.x, 5.0));
    CHECK(is_approx(s_mid->world_pos.y, 0.0));

    // 4. Center Snap (Circle center at (20, 20))
    auto s_ctr = snap_mgr.query_snap(doc, vp, {20.03, 19.98}, 10.0, static_cast<uint32_t>(SnapType::Center));
    REQUIRE(s_ctr.has_value());
    CHECK(s_ctr->type == SnapType::Center);
    CHECK(is_approx(s_ctr->world_pos.x, 20.0));
    CHECK(is_approx(s_ctr->world_pos.y, 20.0));

    // 5. Intersection Snap (Lines intersect at (5, 0))
    auto s_inter = snap_mgr.query_snap(doc, vp, {5.01, 0.01}, 10.0, static_cast<uint32_t>(SnapType::Intersection));
    REQUIRE(s_inter.has_value());
    CHECK(s_inter->type == SnapType::Intersection);
    CHECK(is_approx(s_inter->world_pos.x, 5.0));
    CHECK(is_approx(s_inter->world_pos.y, 0.0));

    // 6. Perpendicular Snap: Anchor at (8, 6), line is horizontal along y=0 -> perpendicular foot at (8, 0)
    Point2D anchor_perp{8.0, 6.0};
    auto s_perp = snap_mgr.query_snap(doc, vp, {8.02, 0.04}, 10.0, static_cast<uint32_t>(SnapType::Perpendicular), anchor_perp);
    REQUIRE(s_perp.has_value());
    CHECK(s_perp->type == SnapType::Perpendicular);
    CHECK(is_approx(s_perp->world_pos.x, 8.0));
    CHECK(is_approx(s_perp->world_pos.y, 0.0));

    // 7. Tangent Snap: Anchor at (20, 30), circle center at (20, 20), radius 5
    // Tangent points are at angles 90 +/- 60 deg -> (15.67, 22.5) and (24.33, 22.5)
    Point2D anchor_tan{20.0, 30.0};
    auto s_tan = snap_mgr.query_snap(doc, vp, {15.65, 22.48}, 15.0, static_cast<uint32_t>(SnapType::Tangent), anchor_tan);
    REQUIRE(s_tan.has_value());
    CHECK(s_tan->type == SnapType::Tangent);
    // Tangency check: dot product of (snap_pos - center) and (anchor - snap_pos) == 0
    Vec2d radius_vec = s_tan->world_pos - Point2D{20.0, 20.0};
    Vec2d tangent_line = anchor_tan - s_tan->world_pos;
    CHECK(is_approx(radius_vec.dot(tangent_line), 0.0, 1e-4));
}

// =============================================================================
// Task PH4-04.02.002: Polar Tracking Guides with Customizable Increments
// =============================================================================

TEST_CASE("PH4-04.02.002: Polar Tracking Guides with Customizable Increments (15, 30, 45, 90 deg)") {
    PolarTrackingEngine polar;

    // 1. Preset 15 degrees: snaps to 75 deg
    polar.set_preset(PolarIncrementPreset::Deg15);
    double r = 10.0;
    double rad_75 = 75.2 * (PI / 180.0);
    auto m15 = polar.evaluate({0.0, 0.0}, {r * std::cos(rad_75), r * std::sin(rad_75)});
    REQUIRE(m15.active);
    CHECK(is_approx(m15.snapped_angle_deg, 75.0, 1e-4));
    CHECK(m15.hud_tooltip.find("75.0°") != std::string::npos);

    // 2. Preset 45 degrees: 75 deg will NOT snap, but 45 deg and 135 deg will
    polar.set_preset(PolarIncrementPreset::Deg45);
    auto m45_reject = polar.evaluate({0.0, 0.0}, {r * std::cos(rad_75), r * std::sin(rad_75)});
    CHECK(!m45_reject.active);

    double rad_135 = 134.8 * (PI / 180.0);
    auto m45_accept = polar.evaluate({0.0, 0.0}, {r * std::cos(rad_135), r * std::sin(rad_135)});
    REQUIRE(m45_accept.active);
    CHECK(is_approx(m45_accept.snapped_angle_deg, 135.0, 1e-4));

    // 3. Preset 90 degrees: strictly orthogonal
    polar.set_preset(PolarIncrementPreset::Deg90);
    double rad_89 = 89.2 * (PI / 180.0);
    auto m90 = polar.evaluate({0.0, 0.0}, {r * std::cos(rad_89), r * std::sin(rad_89)});
    REQUIRE(m90.active);
    CHECK(is_approx(m90.snapped_angle_deg, 90.0, 1e-4));

    // 4. Custom auxiliary angle: add 22.5 deg
    polar.add_custom_angle(22.5);
    double rad_22 = 22.3 * (PI / 180.0);
    auto m_custom = polar.evaluate({0.0, 0.0}, {r * std::cos(rad_22), r * std::sin(rad_22)});
    REQUIRE(m_custom.active);
    CHECK(is_approx(m_custom.snapped_angle_deg, 22.5, 1e-4));

    // 5. Relative polar tracking
    polar.track_relative_to_segment = true;
    polar.set_reference_orientation(30.0); // Preceding segment oriented at 30 deg
    // An angle of 45 deg relative to segment yields 75 deg in world space
    double rad_rel = (30.0 + 44.9) * (PI / 180.0);
    polar.set_preset(PolarIncrementPreset::Deg45);
    auto m_rel = polar.evaluate({0.0, 0.0}, {r * std::cos(rad_rel), r * std::sin(rad_rel)});
    REQUIRE(m_rel.active);
    CHECK(is_approx(m_rel.snapped_angle_deg, 75.0, 1e-4));

    // 6. Visual guide ray generation
    auto ray = PolarTrackingEngine::generate_visual_guide_ray(m_rel, 500.0);
    CHECK(is_approx(ray.total_length(), 500.0, 1e-4));
}

// =============================================================================
// Task PH4-04.02.003: Cartesian and Isometric Dynamic Visual Grids
// =============================================================================

TEST_CASE("PH4-04.02.003: Cartesian and Isometric Dynamic Visual Grids with Adaptive Zoom") {
    SketchViewport2D vp;
    vp.viewport_width = 1200.0;
    vp.viewport_height = 800.0;
    vp.pixels_per_unit = 50.0; // 50 px = 1 unit

    // 1. Cartesian Grid
    vp.grid_mode = GridMode::Cartesian;
    auto cart_grid = vp.compute_dynamic_grid(14.0);
    CHECK(cart_grid.mode == GridMode::Cartesian);
    CHECK(cart_grid.minor_step > 0.0);
    CHECK(is_approx(cart_grid.major_step, cart_grid.minor_step * 5.0));
    CHECK(!cart_grid.vertical_lines.empty());
    CHECK(!cart_grid.horizontal_lines.empty());
    CHECK(cart_grid.fade_alpha >= 0.2 && cart_grid.fade_alpha <= 1.0);

    // Zooming in subdivides the grid adaptively
    vp.zoom_at_screen({600.0, 400.0}, 10.0); // 500 px/unit
    auto cart_zoom = vp.compute_dynamic_grid(14.0);
    CHECK(cart_zoom.minor_step < cart_grid.minor_step);

    // Cartesian snap
    Point2D c_snap = vp.snap_to_active_grid({1.03, 2.04}, 1.0);
    CHECK(is_approx(c_snap.x, 1.0));
    CHECK(is_approx(c_snap.y, 2.0));

    // 2. Isometric Grid
    vp.grid_mode = GridMode::Isometric;
    vp.isometric_angle_deg = 30.0;
    auto iso_grid = vp.compute_dynamic_grid(14.0);
    CHECK(iso_grid.mode == GridMode::Isometric);
    CHECK(!iso_grid.isometric_lines.empty());

    // Isometric snapping
    // At 30 deg, basis is (cos 30, sin 30) = (sqrt(3)/2, 0.5)
    // An exact isometric grid point with c1=2.0, c2=1.0 has:
    // x = (2.0 - 1.0) * cos(30) = 0.866025
    // y = (2.0 + 1.0) * sin(30) = 1.500000
    double cos30 = std::cos(30.0 * PI / 180.0);
    double sin30 = std::sin(30.0 * PI / 180.0);
    Point2D exact_iso_pt{1.0 * cos30, 3.0 * sin30};

    // Perturb point slightly
    Point2D query_pt = exact_iso_pt + Point2D{0.02, -0.01};
    Point2D iso_snapped = vp.snap_to_active_grid(query_pt, 1.0);

    CHECK(is_approx(iso_snapped.x, exact_iso_pt.x, 1e-4));
    CHECK(is_approx(iso_snapped.y, exact_iso_pt.y, 1e-4));

    // Static helper
    Point2D iso_s2 = SketchViewport2D::snap_to_isometric_grid(query_pt, 1.0, 30.0);
    CHECK(is_approx(iso_s2.x, exact_iso_pt.x, 1e-4));
    CHECK(is_approx(iso_s2.y, exact_iso_pt.y, 1e-4));
}
