#include "tests/test_framework.hpp"

#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/cad3d/loft_feature.hpp"
#include "shunya/cad3d/polygon_tessellator.hpp"
#include "shunya/cad3d/revolve_feature.hpp"
#include "shunya/cad3d/sweep_feature.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <vector>

using namespace shunya::cad3d;
using shunya::cad2d::Path2D;
using shunya::cad2d::Paths2D;
using shunya::cad2d::Point2D;
using shunya::core::base::math::PI;

namespace {

bool rel_close(double measured, double expected, double rel_tol = 0.005) {
    return std::abs(measured - expected) <= rel_tol * std::max(1.0, std::abs(expected));
}

double tess_area(const TessellatedPolygon2D& tess) {
    double a2 = 0.0;
    for (const auto& t : tess.triangles) {
        const auto& p0 = tess.vertices[t[0]];
        const auto& p1 = tess.vertices[t[1]];
        const auto& p2 = tess.vertices[t[2]];
        a2 += (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    }
    return std::abs(a2) * 0.5;
}

Path2D make_circle_loop(double cx, double cy, double r, size_t segs = 64) {
    Path2D pts;
    pts.reserve(segs);
    for (size_t i = 0; i < segs; ++i) {
        const double th = 2.0 * PI * (static_cast<double>(i) / static_cast<double>(segs));
        pts.push_back({cx + r * std::cos(th), cy + r * std::sin(th)});
    }
    return pts;
}

Path2D make_gear_loop(double r_inner, double r_outer, size_t teeth = 12) {
    Path2D pts;
    const size_t steps = teeth * 4;
    pts.reserve(steps);
    for (size_t i = 0; i < steps; ++i) {
        const double th = 2.0 * PI * (static_cast<double>(i) / static_cast<double>(steps));
        const double r = ((i % 4) < 2) ? r_outer : r_inner;
        pts.push_back({r * std::cos(th), r * std::sin(th)});
    }
    return pts;
}

} // namespace

TEST_CASE("ExtrusionEngine: concave polygon with multiple holes produces watertight 2-manifold with Euler check") {
    // Concave L-shape outer boundary (area = 20*10 + 10*10 = 300)
    const Path2D l_outer{{0, 0}, {20, 0}, {20, 10}, {10, 10}, {10, 20}, {0, 20}};
    const Path2D hole1{{2, 2}, {6, 2}, {6, 6}, {2, 6}};       // area = 16
    const Path2D hole2{{12, 2}, {18, 2}, {18, 8}, {12, 8}};   // area = 36

    const auto tess = PolygonTessellator::triangulate(l_outer, {hole1, hole2});
    REQUIRE(rel_close(tess_area(tess), 300.0 - 16.0 - 36.0, 1e-9));

    ExtrusionParams params;
    params.depth = 10.0;
    params.end_condition = EndCondition::Blind;
    const SolidMesh3D mesh = ExtrusionEngine::extrude(l_outer, {hole1, hole2}, params);

    const ManifoldReport rep = mesh.analyze_manifold();
    REQUIRE(rep.is_watertight);
    REQUIRE(rep.is_2_manifold);
    // Genus g = 2 holes -> Euler characteristic V - E + F = 2*(1 - 2) = -2
    REQUIRE(rep.euler_characteristic == -2);
    REQUIRE(rel_close(mesh.compute_volume(), (300.0 - 52.0) * 10.0, 1e-6));

    // Extruded complex gear with bore hole (genus = 1 -> Euler V - E + F == 0)
    const Path2D gear = make_gear_loop(8.0, 10.0, 10);
    const Path2D bore = make_circle_loop(0.0, 0.0, 3.0, 32);
    const SolidMesh3D gear_mesh = ExtrusionEngine::extrude(gear, {bore}, params);
    const ManifoldReport gear_rep = gear_mesh.analyze_manifold();
    REQUIRE(gear_rep.is_2_manifold);
    REQUIRE(gear_rep.euler_characteristic == 0);
}

TEST_CASE("ExtrusionEngine: Symmetric limits and drafted frustum volume check") {
    // 10x10 square extruded with 15 degree outward draft over height 5
    const Path2D sq{{-5, -5}, {5, -5}, {5, 5}, {-5, 5}};
    ExtrusionParams params;
    params.depth = 5.0;
    params.end_condition = EndCondition::Symmetric;
    params.draft_angle_rad = 15.0 * PI / 180.0;

    const SolidMesh3D frustum = ExtrusionEngine::extrude(sq, {}, params);
    REQUIRE(frustum.analyze_manifold().is_2_manifold);
    REQUIRE(frustum.analyze_manifold().euler_characteristic == 2);

    const BBox3D box = frustum.compute_bbox();
    REQUIRE(rel_close(box.min.z, -2.5, 1e-9));
    REQUIRE(rel_close(box.max.z, 2.5, 1e-9));

    const double off = 5.0 * std::tan(params.draft_angle_rad);
    const double s1 = 10.0;
    const double s2 = 10.0 + 2.0 * off;
    const double a1 = s1 * s1;
    const double a2 = s2 * s2;
    const double analytical_vol = (5.0 / 3.0) * (a1 + a2 + std::sqrt(a1 * a2));
    REQUIRE(rel_close(frustum.compute_volume(), analytical_vol, 1e-6));
}

TEST_CASE("RevolveEngine: Cylinder, Cone, Torus analytical volume/area & partial caps") {
    // 1. Cylinder (rectangle touching Y axis: r=4, h=5)
    const Path2D cyl_rect{{0.0, 0.0}, {4.0, 0.0}, {4.0, 5.0}, {0.0, 5.0}};
    RevolveParams rev;
    rev.axis = {{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    rev.angle_rad = 2.0 * PI;
    rev.angular_segments = 128;

    const SolidMesh3D cyl = RevolveEngine::revolve(cyl_rect, {}, rev);
    REQUIRE(cyl.analyze_manifold().is_2_manifold);
    REQUIRE(cyl.analyze_manifold().euler_characteristic == 2);
    REQUIRE(rel_close(cyl.compute_volume(), PI * 16.0 * 5.0, 0.002));

    // 2. Cone (right triangle touching Y axis: r=3, h=6)
    const Path2D cone_tri{{0.0, 0.0}, {3.0, 0.0}, {0.0, 6.0}};
    const SolidMesh3D cone = RevolveEngine::revolve(cone_tri, {}, rev);
    REQUIRE(cone.analyze_manifold().is_2_manifold);
    REQUIRE(rel_close(cone.compute_volume(), (1.0 / 3.0) * PI * 9.0 * 6.0, 0.002));

    // 3. Torus (circle R=10, r=2)
    const Path2D torus_prof = make_circle_loop(10.0, 0.0, 2.0, 72);
    const SolidMesh3D torus = RevolveEngine::revolve(torus_prof, {}, rev);
    const ManifoldReport tor_rep = torus.analyze_manifold();
    REQUIRE(tor_rep.is_2_manifold);
    REQUIRE(tor_rep.euler_characteristic == 0);
    const double tor_vol_exact = 2.0 * PI * PI * 10.0 * 4.0;
    const double tor_area_exact = 4.0 * PI * PI * 10.0 * 2.0;
    REQUIRE(rel_close(torus.compute_volume(), tor_vol_exact, 0.004));
    REQUIRE(rel_close(torus.compute_surface_area(), tor_area_exact, 0.004));

    // 4. Partial 90-degree revolve of offset rectangle (1/4 volume + closed caps)
    const Path2D ring_rect{{3.0, 0.0}, {5.0, 0.0}, {5.0, 4.0}, {3.0, 4.0}};
    rev.angle_rad = PI * 0.5;
    rev.angular_segments = 48;
    const SolidMesh3D quarter = RevolveEngine::revolve(ring_rect, {}, rev);
    REQUIRE(quarter.analyze_manifold().is_2_manifold);
    REQUIRE(quarter.analyze_manifold().euler_characteristic == 2);
    REQUIRE(rel_close(quarter.compute_volume(), 0.25 * PI * (25.0 - 9.0) * 4.0, 0.002));
}

TEST_CASE("SweepEngine & LoftEngine: RMF orthogonality, helical pipe sweep, and Circle-to-Square loft") {
    // 1. Helical 3D trajectory RMF frame verification
    const auto helix = TrajectoryCurve3D::sample_helix(6.0, 4.0, 1.5, 96);
    const auto frames = RotationMinimizingFrame::compute_double_reflection(helix, false, {0.0, 0.0, 1.0});
    REQUIRE(frames.size() == helix.size());
    for (const auto& fr : frames) {
        REQUIRE(std::abs(fr.tangent.dot(fr.normal)) < 1e-12);
        REQUIRE(std::abs(fr.tangent.dot(fr.binormal)) < 1e-12);
        REQUIRE(std::abs(fr.normal.dot(fr.binormal)) < 1e-12);
        REQUIRE((fr.normal.cross(fr.binormal) - fr.tangent).length() < 1e-12);
    }

    // 2. Swept tube along helical 3D curve (circle with bore hole = genus 1 manifold)
    const Path2D outer_c = make_circle_loop(0.0, 0.0, 1.0, 24);
    const Path2D inner_c = make_circle_loop(0.0, 0.0, 0.5, 24);
    const SolidMesh3D swept_helix = SweepEngine::sweep(outer_c, {inner_c}, helix);
    const ManifoldReport sw_rep = swept_helix.analyze_manifold();
    REQUIRE(sw_rep.is_2_manifold);
    REQUIRE(sw_rep.euler_characteristic == 0);

    // 3. Loft Circle (r=4, z=0) -> Square (8x8, z=10) -> Scaled Circle (r=3, z=20)
    const Path2D sq{{-4, -4}, {4, -4}, {4, 4}, {-4, 4}};
    std::vector<LoftSection> sections{
        {make_circle_loop(0, 0, 4.0, 48), SketchPlane3D::xy(0.0)},
        {sq, SketchPlane3D::xy(10.0)},
        {make_circle_loop(0, 0, 3.0, 48), SketchPlane3D::xy(20.0)}
    };
    const SolidMesh3D lofted = LoftEngine::loft(sections, {48, true, false});
    const ManifoldReport lf_rep = lofted.analyze_manifold();
    REQUIRE(lf_rep.is_2_manifold);
    REQUIRE(lf_rep.euler_characteristic == 2);
    REQUIRE(lofted.compute_volume() > 500.0);
}
