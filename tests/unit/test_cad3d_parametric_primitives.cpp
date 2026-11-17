#include "tests/test_framework.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <vector>

using namespace shunya::cad3d;

namespace {

bool is_close(double a, double b, double tol = 1e-4) {
    return std::abs(a - b) <= tol;
}

} // namespace

TEST_CASE("PH4-05.01.001: Parametric primitive generation for all 11 core 3D shapes") {
    // 1. Cube
    CubeParams cp;
    cp.width = 2.0; cp.height = 3.0; cp.depth = 4.0;
    cp.subdivisions_x = 2; cp.subdivisions_y = 2; cp.subdivisions_z = 2;
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);
    REQUIRE(!cube.vertices.empty());
    REQUIRE(!cube.triangles.empty());
    REQUIRE(cube.normals.size() == cube.vertices.size());
    REQUIRE(cube.uvs.size() == cube.vertices.size());
    auto cube_bbox = cube.compute_bbox();
    REQUIRE(is_close(cube_bbox.size().x, 2.0));
    REQUIRE(is_close(cube_bbox.size().y, 3.0));
    REQUIRE(is_close(cube_bbox.size().z, 4.0));

    // 2. Sphere & UV Sphere
    UvSphereParams sp;
    sp.radius = 1.5;
    sp.segments = 24;
    sp.rings = 12;
    auto sphere = ParametricPrimitiveGenerator::generate_uv_sphere(sp);
    REQUIRE(!sphere.vertices.empty());
    REQUIRE(!sphere.triangles.empty());
    REQUIRE(sphere.normals.size() == sphere.vertices.size());
    REQUIRE(sphere.uvs.size() == sphere.vertices.size());
    auto sphere_bbox = sphere.compute_bbox();
    REQUIRE(is_close(sphere_bbox.size().x, 3.0));
    REQUIRE(is_close(sphere_bbox.size().y, 3.0));
    REQUIRE(is_close(sphere_bbox.size().z, 3.0));

    // 3. Icosphere
    IcosphereParams ip;
    ip.radius = 2.0;
    ip.subdivisions = 2;
    auto ico = ParametricPrimitiveGenerator::generate_icosphere(ip);
    REQUIRE(ico.vertices.size() > 12);
    REQUIRE(ico.triangles.size() == 20 * 16); // 20 * 4^2 = 320 triangles
    REQUIRE(ico.normals.size() == ico.vertices.size());
    REQUIRE(ico.uvs.size() == ico.vertices.size());
    auto ico_bbox = ico.compute_bbox();
    REQUIRE(is_close(ico_bbox.size().x, 4.0, 0.1));

    // 4. Cylinder
    CylinderParams cyl_p;
    cyl_p.radius = 1.0;
    cyl_p.height = 5.0;
    cyl_p.radial_segments = 20;
    cyl_p.cap_fill = CapFillType::TriangleFan;
    auto cyl = ParametricPrimitiveGenerator::generate_cylinder(cyl_p);
    REQUIRE(!cyl.vertices.empty());
    REQUIRE(!cyl.triangles.empty());
    REQUIRE(cyl.normals.size() == cyl.vertices.size());
    REQUIRE(cyl.uvs.size() == cyl.vertices.size());
    auto cyl_bbox = cyl.compute_bbox();
    REQUIRE(is_close(cyl_bbox.size().x, 2.0));
    REQUIRE(is_close(cyl_bbox.size().z, 5.0));

    // 5. Cone
    ConeParams cone_p;
    cone_p.radius = 1.5;
    cone_p.height = 4.0;
    cone_p.radial_segments = 16;
    cone_p.cap_fill = CapFillType::TriangleFan;
    auto cone = ParametricPrimitiveGenerator::generate_cone(cone_p);
    REQUIRE(!cone.vertices.empty());
    REQUIRE(!cone.triangles.empty());
    auto cone_bbox = cone.compute_bbox();
    REQUIRE(is_close(cone_bbox.size().x, 3.0));
    REQUIRE(is_close(cone_bbox.size().z, 4.0));

    // 6. Torus
    TorusParams tp;
    tp.major_radius = 3.0;
    tp.minor_radius = 0.5;
    tp.major_segments = 24;
    tp.minor_segments = 12;
    auto torus = ParametricPrimitiveGenerator::generate_torus(tp);
    REQUIRE(!torus.vertices.empty());
    REQUIRE(!torus.triangles.empty());
    REQUIRE(torus.normals.size() == torus.vertices.size());
    auto torus_bbox = torus.compute_bbox();
    REQUIRE(is_close(torus_bbox.size().x, 7.0)); // 2*(3+0.5)
    REQUIRE(is_close(torus_bbox.size().z, 1.0)); // 2*0.5

    // 7. Plane
    PlaneParams pp;
    pp.width = 4.0;
    pp.height = 6.0;
    pp.segments_x = 2;
    pp.segments_y = 3;
    auto plane = ParametricPrimitiveGenerator::generate_plane(pp);
    REQUIRE(plane.vertices.size() == 3 * 4); // (2+1)*(3+1) = 12
    REQUIRE(plane.triangles.size() == 2 * 3 * 2); // 12
    auto plane_bbox = plane.compute_bbox();
    REQUIRE(is_close(plane_bbox.size().x, 4.0));
    REQUIRE(is_close(plane_bbox.size().y, 6.0));
    REQUIRE(is_close(plane_bbox.size().z, 0.0));

    // 8. Circle
    CircleParams cp_circ;
    cp_circ.radius = 2.5;
    cp_circ.segments = 24;
    cp_circ.fill = CircleFillType::TriangleFan;
    auto circle = ParametricPrimitiveGenerator::generate_circle(cp_circ);
    REQUIRE(circle.vertices.size() == 25); // 1 center + 24 rim
    REQUIRE(circle.triangles.size() == 24);
    auto circle_bbox = circle.compute_bbox();
    REQUIRE(is_close(circle_bbox.size().x, 5.0));

    // 9. Grid
    GridParams gp;
    gp.width = 10.0;
    gp.height = 10.0;
    gp.subdivisions_x = 5;
    gp.subdivisions_y = 5;
    auto grid = ParametricPrimitiveGenerator::generate_grid(gp);
    REQUIRE(grid.vertices.size() == 36); // 6*6
    REQUIRE(grid.triangles.size() == 50); // 25*2

    // 10. Capsule
    CapsuleParams cap_p;
    cap_p.radius = 1.0;
    cap_p.cylinder_height = 4.0;
    cap_p.radial_segments = 20;
    cap_p.rings = 6;
    auto capsule = ParametricPrimitiveGenerator::generate_capsule(cap_p);
    REQUIRE(!capsule.vertices.empty());
    REQUIRE(!capsule.triangles.empty());
    REQUIRE(capsule.normals.size() == capsule.vertices.size());
    auto cap_bbox = capsule.compute_bbox();
    REQUIRE(is_close(cap_bbox.size().x, 2.0));
    REQUIRE(is_close(cap_bbox.size().z, 6.0)); // 4.0 + 2*1.0
}

TEST_CASE("PH4-05.01.002: Non-destructive parameter editing for 3D primitives during and after creation") {
    // 1. Create a Cylinder primitive
    ParametricPrimitive prim(PrimitiveType::Cylinder);
    REQUIRE(prim.type() == PrimitiveType::Cylinder);
    REQUIRE(prim.is_dirty());

    // Generate initial mesh
    const auto& m1 = prim.mesh();
    REQUIRE(!prim.is_dirty());
    REQUIRE(is_close(prim.get_double_property("radius"), 1.0));
    REQUIRE(is_close(prim.get_double_property("height"), 2.0));
    REQUIRE(prim.get_uint_property("radial_segments") == 32);
    auto bbox1 = m1.compute_bbox();
    REQUIRE(is_close(bbox1.size().x, 2.0));
    REQUIRE(is_close(bbox1.size().z, 2.0));

    // 2. Non-destructive parameter modification: Change radius and height
    prim.set_double_property("radius", 3.0);
    prim.set_double_property("height", 8.0);
    REQUIRE(prim.is_dirty());

    // Rebuild mesh and verify non-destructive update
    const auto& m2 = prim.mesh();
    REQUIRE(!prim.is_dirty());
    auto bbox2 = m2.compute_bbox();
    REQUIRE(is_close(bbox2.size().x, 6.0));
    REQUIRE(is_close(bbox2.size().z, 8.0));

    // 3. Edit radial subdivisions non-destructively
    size_t tri_count_before = m2.triangles.size();
    prim.set_uint_property("radial_segments", 64);
    REQUIRE(prim.is_dirty());
    const auto& m3 = prim.mesh();
    REQUIRE(!prim.is_dirty());
    REQUIRE(m3.triangles.size() > tri_count_before);

    // 4. Test Cube parameter editing
    ParametricPrimitive cube_prim(PrimitiveType::Cube);
    REQUIRE(cube_prim.has_property("width"));
    REQUIRE(cube_prim.has_property("height"));
    REQUIRE(cube_prim.has_property("depth"));
    cube_prim.set_double_property("width", 5.0);
    cube_prim.set_double_property("height", 10.0);
    cube_prim.set_double_property("depth", 15.0);
    const auto& cube_m = cube_prim.mesh();
    auto cube_bbox = cube_m.compute_bbox();
    REQUIRE(is_close(cube_bbox.size().x, 5.0));
    REQUIRE(is_close(cube_bbox.size().y, 10.0));
    REQUIRE(is_close(cube_bbox.size().z, 15.0));

    // 5. Test Torus parameter editing
    ParametricPrimitive torus_prim(PrimitiveType::Torus);
    torus_prim.set_double_property("major_radius", 5.0);
    torus_prim.set_double_property("minor_radius", 1.0);
    const auto& torus_m = torus_prim.mesh();
    auto torus_bbox = torus_m.compute_bbox();
    REQUIRE(is_close(torus_bbox.size().x, 12.0)); // 2 * (5.0 + 1.0)
    REQUIRE(is_close(torus_bbox.size().z, 2.0));  // 2 * 1.0

    // 6. Test whole parameter variant assignment via set_params()
    CylinderParams new_cyl_p;
    new_cyl_p.radius = 0.5;
    new_cyl_p.height = 1.0;
    new_cyl_p.radial_segments = 16;
    prim.set_params(new_cyl_p);
    REQUIRE(prim.is_dirty());
    const auto& m4 = prim.mesh();
    REQUIRE(is_close(m4.compute_bbox().size().x, 1.0));
    REQUIRE(is_close(m4.compute_bbox().size().z, 1.0));
}
