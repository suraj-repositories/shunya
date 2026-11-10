#include "tests/test_framework.hpp"

#include "shunya/cad3d/csg_boolean.hpp"
#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/cad3d/solid_chamfer.hpp"
#include "shunya/cad3d/solid_fillet.hpp"
#include "shunya/cad3d/solid_shell.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <vector>

using namespace shunya::cad3d;
using shunya::cad2d::Path2D;
using shunya::core::base::math::PI;

namespace {

bool is_close(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) <= tol;
}

SolidMesh3D make_box(double cx, double cy, double z0, double sx, double sy, double sz) {
    const double hx = sx * 0.5;
    const double hy = sy * 0.5;
    const Path2D rect{{cx - hx, cy - hy}, {cx + hx, cy - hy}, {cx + hx, cy + hy}, {cx - hx, cy + hy}};
    ExtrusionParams p;
    p.depth = sz;
    p.end_condition = EndCondition::Blind;
    p.plane = SketchPlane3D::xy(z0);
    return ExtrusionEngine::extrude(rect, {}, p);
}

SolidMesh3D make_cylinder(double cx, double cy, double z0, double r, double h, size_t segs = 24) {
    Path2D pts;
    pts.reserve(segs);
    for (size_t i = 0; i < segs; ++i) {
        const double th = 2.0 * PI * (static_cast<double>(i) / static_cast<double>(segs));
        pts.push_back({cx + r * std::cos(th), cy + r * std::sin(th)});
    }
    ExtrusionParams p;
    p.depth = h;
    p.end_condition = EndCondition::Blind;
    p.plane = SketchPlane3D::xy(z0);
    return ExtrusionEngine::extrude(pts, {}, p);
}

SolidMesh3D make_hex_prism(double r, double h, size_t sides = 6) {
    Path2D pts;
    pts.reserve(sides);
    for (size_t i = 0; i < sides; ++i) {
        const double th = 2.0 * PI * (static_cast<double>(i) / static_cast<double>(sides));
        pts.push_back({r * std::cos(th), r * std::sin(th)});
    }
    ExtrusionParams p;
    p.depth = h;
    p.end_condition = EndCondition::Blind;
    return ExtrusionEngine::extrude(pts, {}, p);
}

} // namespace

TEST_CASE("CsgBooleanEngine: volume conservation and subtracting a cylinder from a cube") {
    // 1. Overlapping boxes: Vol(A) + Vol(B) == Vol(A U B) + Vol(A & B)
    const SolidMesh3D box_a = make_box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0); // 1000
    const SolidMesh3D box_b = make_box(4.0, 3.0, 2.0, 8.0, 8.0, 8.0);    // 512

    const SolidMesh3D u_mesh = CsgBooleanEngine::execute(CsgOp::Union, box_a, box_b);
    const SolidMesh3D i_mesh = CsgBooleanEngine::execute(CsgOp::Intersect, box_a, box_b);
    const SolidMesh3D s_mesh = CsgBooleanEngine::execute(CsgOp::Subtract, box_a, box_b);

    REQUIRE(u_mesh.analyze_manifold().is_2_manifold);
    REQUIRE(i_mesh.analyze_manifold().is_2_manifold);
    REQUIRE(s_mesh.analyze_manifold().is_2_manifold);

    const double va = box_a.compute_volume();
    const double vb = box_b.compute_volume();
    const double vu = u_mesh.compute_volume();
    const double vi = i_mesh.compute_volume();
    const double vs = s_mesh.compute_volume();

    REQUIRE(is_close(va + vb, vu + vi, 1e-4));
    REQUIRE(is_close(va - vi, vs, 1e-4));
    REQUIRE(is_close(vi, 5.0 * 6.0 * 8.0, 1e-4)); // x:[0..5]=5, y:[-1..5]=6, z:[2..10]=8

    // 2. Subtract a cylinder through a cube -> clean through-hole (genus 1, Euler = 0)
    const SolidMesh3D cyl = make_cylinder(0.0, 0.0, -2.0, 2.0, 14.0, 20);
    const SolidMesh3D drilled = CsgBooleanEngine::execute(CsgOp::Subtract, box_a, cyl);
    const ManifoldReport dr_rep = drilled.analyze_manifold();
    REQUIRE(dr_rep.is_watertight);
    REQUIRE(dr_rep.is_2_manifold);
    REQUIRE(dr_rep.euler_characteristic == 0);

    // Cylinder cross-section is 20-gon inscribed in r=2 -> exact drilled volume check against polygon prism of height 10
    const SolidMesh3D cyl_inside = make_cylinder(0.0, 0.0, 0.0, 2.0, 10.0, 20);
    REQUIRE(is_close(drilled.compute_volume(), va - cyl_inside.compute_volume(), 1e-3));
}

TEST_CASE("SolidChamferEngine & SolidFilletEngine: 90-deg boxes, obtuse hex prisms, and variable-radius fillets") {
    const SolidMesh3D box = make_box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);

    // 1. Planar chamfer (1.0 setback on 10x10x10 cube)
    const SolidMesh3D chamfered = SolidChamferEngine::chamfer(box, {1.0, 0.15, {}});
    const ManifoldReport ch_rep = chamfered.analyze_manifold();
    REQUIRE(ch_rep.is_watertight);
    REQUIRE(ch_rep.is_2_manifold);
    REQUIRE(ch_rep.euler_characteristic == 2);
    REQUIRE(chamfered.compute_volume() < 1000.0);
    REQUIRE(chamfered.compute_volume() > 800.0);

    // 2. Rolling-ball constant fillet (R=1.0, 8 arc segments)
    const SolidMesh3D filleted = SolidFilletEngine::fillet(box, {1.0, -1.0, 8, 0.15, {}, {}});
    const ManifoldReport fi_rep = filleted.analyze_manifold();
    REQUIRE(fi_rep.is_watertight);
    REQUIRE(fi_rep.is_2_manifold);
    REQUIRE(fi_rep.euler_characteristic == 2);
    // Rounded fillet retains more volume than flat chamfer of same setback (1.0), and less than sharp box (1000)
    REQUIRE(filleted.compute_volume() > chamfered.compute_volume());
    REQUIRE(filleted.compute_volume() < 1000.0);

    // 3. Variable radius fillet (0.5 -> 1.5 across edges)
    const SolidMesh3D var_fillet = SolidFilletEngine::fillet(box, {0.5, 1.5, 6, 0.15, {}, {}});
    REQUIRE(var_fillet.analyze_manifold().is_2_manifold);
    REQUIRE(var_fillet.analyze_manifold().euler_characteristic == 2);

    // 4. Obtuse angle prism (hexagonal prism has 60-deg dihedral angle between side walls + 90-deg cap edges)
    const SolidMesh3D hex = make_hex_prism(5.0, 8.0, 6);
    const SolidMesh3D hex_fillet = SolidFilletEngine::fillet(hex, {0.6, -1.0, 6, 0.15, {}, {}});
    const ManifoldReport hf_rep = hex_fillet.analyze_manifold();
    REQUIRE(hf_rep.is_watertight);
    REQUIRE(hf_rep.is_2_manifold);
    REQUIRE(hf_rep.euler_characteristic == 2);
}

TEST_CASE("ShellEngine: thin-wall open-top box shell and uniform solid offsetting") {
    const SolidMesh3D box = make_box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);

    // Open top face (+Z normal), wall thickness t = 0.75
    const SolidMesh3D shelled = ShellEngine::shell_open_by_normal(box, 0.75, {0.0, 0.0, 1.0}, 0.9);
    const ManifoldReport sh_rep = shelled.analyze_manifold();
    REQUIRE(sh_rep.is_watertight);
    REQUIRE(sh_rep.is_2_manifold);
    REQUIRE(sh_rep.euler_characteristic == 2);

    // Exact cavity = (10 - 1.5) x (10 - 1.5) x (10 - 0.75) = 8.5 * 8.5 * 9.25 = 668.3125
    const double expected_shell_vol = 1000.0 - (8.5 * 8.5 * 9.25);
    REQUIRE(is_close(shelled.compute_volume(), expected_shell_vol, 1e-6));

    // Verify inner wall plane distance from outer wall is exactly 0.75 within 1e-6
    bool found_inner_bottom = false;
    for (const auto& v : shelled.vertices) {
        if (is_close(v.z, 0.75, 1e-6) && is_close(std::abs(v.x), 4.25, 1e-6) && is_close(std::abs(v.y), 4.25, 1e-6)) {
            found_inner_bottom = true;
            break;
        }
    }
    REQUIRE(found_inner_bottom);

    // Uniform outward offset +1.0 -> 12x12x12 cube = 1728.0
    const SolidMesh3D expanded = ShellEngine::offset_solid(box, 1.0);
    REQUIRE(expanded.analyze_manifold().is_2_manifold);
    REQUIRE(is_close(expanded.compute_volume(), 12.0 * 12.0 * 12.0, 1e-6));
}
