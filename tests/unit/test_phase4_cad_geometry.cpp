#include "tests/test_framework.hpp"

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/cad2d/snap_manager.hpp"
#include "shunya/cad3d/csg_boolean.hpp"
#include "shunya/cad3d/extrusion_feature.hpp"

#include <cmath>
#include <numbers>
#include <vector>

using namespace shunya::cad2d;
using namespace shunya::cad3d;

namespace {

SolidMesh3D make_simple_box(double cx, double cy, double z0, double sx, double sy, double sz) {
    const double hx = sx * 0.5;
    const double hy = sy * 0.5;
    const Path2D rect{{cx - hx, cy - hy}, {cx + hx, cy - hy}, {cx + hx, cy + hy}, {cx - hx, cy + hy}};
    ExtrusionParams p;
    p.depth = sz;
    p.end_condition = EndCondition::Blind;
    p.plane = SketchPlane3D::xy(z0);
    return ExtrusionEngine::extrude(rect, {}, p);
}

} // namespace

// =============================================================================
// PH4-13.01.006: CAD 2D & 3D Geometry Algorithms Verification
// =============================================================================

TEST_CASE("PH4-13.01.006: 2D Parametric Curves, Arcs & Polygon Booleans") {
    // 1. LineSegment2D and Circle2D
    LineSegment2D seg({0.0, 0.0}, {3.0, 4.0});
    CHECK(std::abs(seg.total_length() - 5.0) < 1e-9);

    Circle2D circle({0.0, 0.0}, 2.5);
    CHECK(std::abs(circle.total_length() - (5.0 * std::numbers::pi)) < 1e-6);

    // 2. 2D Polygon Boolean Union & Intersection via PolygonBooleanEngine
    Path2D rect1{{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}; // Area = 16
    Path2D rect2{{2.0, 0.0}, {6.0, 0.0}, {6.0, 4.0}, {2.0, 4.0}}; // Overlap Area = 8

    auto union_paths = PolygonBooleanEngine::execute(BooleanOp::Union, {rect1}, {rect2});
    REQUIRE(!union_paths.empty());

    auto diff_paths = PolygonBooleanEngine::execute(BooleanOp::Difference, {rect1}, {rect2});
    REQUIRE(!diff_paths.empty());
}

TEST_CASE("PH4-13.01.006: 2D Interactive Snapping & Analytical Intersection Engine") {
    // 1. Snap Types and masks
    const uint32_t mask = static_cast<uint32_t>(SnapType::Endpoint) | static_cast<uint32_t>(SnapType::Midpoint);
    CHECK((mask & static_cast<uint32_t>(SnapType::Endpoint)) != 0);
    CHECK((mask & static_cast<uint32_t>(SnapType::Center)) == 0);

    // 2. Analytical Curve Intersections
    LineSegment2D h_line({0.0, 5.0}, {10.0, 5.0});
    LineSegment2D v_line({5.0, 0.0}, {5.0, 10.0});

    auto inters = SnapTargetManager::intersect_curves(h_line, v_line);
    REQUIRE(inters.size() == 1);
    CHECK(std::abs(inters[0].x - 5.0) < 1e-9);
    CHECK(std::abs(inters[0].y - 5.0) < 1e-9);

    // 3. Circle-Line Analytical Intersections
    Circle2D c({0.0, 0.0}, 5.0);
    LineSegment2D diam({-10.0, 0.0}, {10.0, 0.0});
    auto circle_inters = SnapTargetManager::intersect_curves(c, diam);
    REQUIRE(circle_inters.size() == 2);
    CHECK(std::abs(std::abs(circle_inters[0].x) - 5.0) < 1e-9);
    CHECK(std::abs(circle_inters[0].y) < 1e-9);
}

TEST_CASE("PH4-13.01.006: 3D B-Rep & CSG Boolean Algorithms (Union, Difference)") {
    // Box A: 10 x 10 x 10 centered at origin -> Vol = 1000
    SolidMesh3D box_a = make_simple_box(0.0, 0.0, -5.0, 10.0, 10.0, 10.0);
    const auto man_a = box_a.analyze_manifold();
    CHECK(man_a.is_2_manifold);
    CHECK(std::abs(box_a.compute_volume() - 1000.0) < 1e-3);

    // Box B: 8 x 8 x 8 centered at (4, 3, 2)
    SolidMesh3D box_b = make_simple_box(4.0, 3.0, -2.0, 8.0, 8.0, 8.0);
    const auto man_b = box_b.analyze_manifold();
    CHECK(man_b.is_2_manifold);

    // CSG Union
    SolidMesh3D u_mesh = CsgBooleanEngine::execute(CsgOp::Union, box_a, box_b);
    CHECK(u_mesh.analyze_manifold().is_2_manifold);
    CHECK(u_mesh.compute_volume() > box_a.compute_volume());

    // CSG Subtract
    SolidMesh3D s_mesh = CsgBooleanEngine::execute(CsgOp::Subtract, box_a, box_b);
    CHECK(s_mesh.analyze_manifold().is_2_manifold);
    CHECK(s_mesh.compute_volume() < box_a.compute_volume());
}
