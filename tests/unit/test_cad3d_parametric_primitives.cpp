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

TEST_CASE("PH4-05.01.003: Custom primitive generators for engineering fasteners: Hex Bolts, Washers, Nuts, Pins") {
    // 1. Hex Bolt (ISO 4014 standard preset M6 & M8)
    auto bolt_m6_params = ParametricPrimitiveGenerator::iso4014_hex_bolt("M6", 40.0);
    REQUIRE(is_close(bolt_m6_params.nominal_diameter, 6.0));
    REQUIRE(is_close(bolt_m6_params.total_length, 40.0));
    REQUIRE(is_close(bolt_m6_params.head_across_flats, 10.0));

    auto bolt_mesh = ParametricPrimitiveGenerator::generate_hex_bolt(bolt_m6_params);
    REQUIRE(!bolt_mesh.vertices.empty());
    REQUIRE(!bolt_mesh.triangles.empty());
    REQUIRE(bolt_mesh.normals.size() == bolt_mesh.vertices.size());
    REQUIRE(bolt_mesh.uvs.size() == bolt_mesh.vertices.size());
    auto bolt_bbox = bolt_mesh.compute_bbox();
    REQUIRE(is_close(bolt_bbox.size().z, 44.0)); // head_height (4.0) + total_length (40.0)

    auto bolt_m8_params = ParametricPrimitiveGenerator::iso4014_hex_bolt("M8", 50.0);
    REQUIRE(is_close(bolt_m8_params.nominal_diameter, 8.0));
    REQUIRE(is_close(bolt_m8_params.head_across_flats, 13.0));

    // 2. Washer (DIN 125 standard preset M6 & M10)
    auto washer_m6_params = ParametricPrimitiveGenerator::din125_washer("M6");
    REQUIRE(is_close(washer_m6_params.inner_diameter, 6.4));
    REQUIRE(is_close(washer_m6_params.outer_diameter, 12.0));
    REQUIRE(is_close(washer_m6_params.thickness, 1.6));

    auto washer_mesh = ParametricPrimitiveGenerator::generate_washer(washer_m6_params);
    REQUIRE(!washer_mesh.vertices.empty());
    REQUIRE(!washer_mesh.triangles.empty());
    REQUIRE(washer_mesh.normals.size() == washer_mesh.vertices.size());
    REQUIRE(washer_mesh.uvs.size() == washer_mesh.vertices.size());
    auto washer_bbox = washer_mesh.compute_bbox();
    REQUIRE(is_close(washer_bbox.size().x, 12.0));
    REQUIRE(is_close(washer_bbox.size().z, 1.6));

    // 3. Nut (DIN 934 standard preset M6 & M8)
    auto nut_m6_params = ParametricPrimitiveGenerator::din934_nut("M6");
    REQUIRE(is_close(nut_m6_params.nominal_diameter, 6.0));
    REQUIRE(is_close(nut_m6_params.across_flats, 10.0));
    REQUIRE(is_close(nut_m6_params.height, 5.0));

    auto nut_mesh = ParametricPrimitiveGenerator::generate_nut(nut_m6_params);
    REQUIRE(!nut_mesh.vertices.empty());
    REQUIRE(!nut_mesh.triangles.empty());
    REQUIRE(nut_mesh.normals.size() == nut_mesh.vertices.size());
    REQUIRE(nut_mesh.uvs.size() == nut_mesh.vertices.size());
    auto nut_bbox = nut_mesh.compute_bbox();
    REQUIRE(is_close(nut_bbox.size().z, 5.0));

    // 4. Dowel Pin (ISO 8734 6x30mm)
    auto pin_params = ParametricPrimitiveGenerator::iso8734_dowel_pin(6.0, 30.0);
    REQUIRE(is_close(pin_params.diameter, 6.0));
    REQUIRE(is_close(pin_params.length, 30.0));

    auto pin_mesh = ParametricPrimitiveGenerator::generate_pin(pin_params);
    REQUIRE(!pin_mesh.vertices.empty());
    REQUIRE(!pin_mesh.triangles.empty());
    REQUIRE(pin_mesh.normals.size() == pin_mesh.vertices.size());
    auto pin_bbox = pin_mesh.compute_bbox();
    REQUIRE(is_close(pin_bbox.size().x, 6.0));
    REQUIRE(is_close(pin_bbox.size().z, 30.0));

    // 5. Fasteners wrapped in ParametricPrimitive with non-destructive editing
    ParametricPrimitive prim_bolt(PrimitiveType::HexBolt, bolt_m6_params);
    REQUIRE(prim_bolt.has_property("nominal_diameter"));
    REQUIRE(prim_bolt.has_property("total_length"));
    REQUIRE(is_close(prim_bolt.get_double_property("total_length"), 40.0));
    prim_bolt.set_double_property("total_length", 60.0);
    const auto& updated_bolt_mesh = prim_bolt.mesh();
    REQUIRE(is_close(updated_bolt_mesh.compute_bbox().size().z, 64.0));
}

TEST_CASE("PH4-05.01.004: Ensure clean 2-manifold watertight topology with proper normals and UV coordinates") {
    // 1. Icosphere: inherently closed, watertight 2-manifold with V - E + F = 2
    IcosphereParams ip;
    ip.radius = 1.0;
    ip.subdivisions = 1;
    auto ico = ParametricPrimitiveGenerator::generate_icosphere(ip);

    auto ico_raw_rep = ico.analyze_manifold();
    REQUIRE(ico_raw_rep.is_watertight);
    REQUIRE(ico_raw_rep.is_2_manifold);
    REQUIRE(ico_raw_rep.boundary_edge_count == 0);
    REQUIRE(ico_raw_rep.non_manifold_edge_count == 0);
    REQUIRE(ico_raw_rep.euler_characteristic == 2);

    auto ico_val = ParametricMeshValidator::validate_topology_and_attributes(ico, true);
    REQUIRE(ico_val.is_watertight);
    REQUIRE(ico_val.is_2_manifold);
    REQUIRE(ico_val.normals_valid);
    REQUIRE(ico_val.uvs_valid);
    REQUIRE(ico_val.zero_normals_count == 0);
    REQUIRE(ico_val.invalid_uvs_count == 0);

    // 2. Cube: verify normal alignment and watertight topology
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0;
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);
    auto cube_val = ParametricMeshValidator::validate_topology_and_attributes(cube, true);
    REQUIRE(cube_val.is_watertight);
    REQUIRE(cube_val.is_2_manifold);
    REQUIRE(cube_val.normals_valid);
    REQUIRE(cube_val.uvs_valid);
    REQUIRE(cube_val.euler_characteristic == 2);

    // 3. Cylinder with caps: verify watertight topology
    CylinderParams cyl_p;
    cyl_p.radius = 1.0; cyl_p.height = 3.0; cyl_p.radial_segments = 24;
    cyl_p.cap_fill = CapFillType::TriangleFan;
    auto cyl = ParametricPrimitiveGenerator::generate_cylinder(cyl_p);
    auto cyl_val = ParametricMeshValidator::validate_topology_and_attributes(cyl, true);
    REQUIRE(cyl_val.is_watertight);
    REQUIRE(cyl_val.is_2_manifold);
    REQUIRE(cyl_val.normals_valid);
    REQUIRE(cyl_val.uvs_valid);

    // 4. Torus: closed manifold of Genus 1 (Euler characteristic = 0)
    TorusParams tp;
    tp.major_radius = 2.0; tp.minor_radius = 0.5;
    tp.major_segments = 20; tp.minor_segments = 12;
    auto torus = ParametricPrimitiveGenerator::generate_torus(tp);
    auto torus_val = ParametricMeshValidator::validate_topology_and_attributes(torus, true);
    REQUIRE(torus_val.is_watertight);
    REQUIRE(torus_val.is_2_manifold);
    REQUIRE(torus_val.euler_characteristic == 0); // V - E + F = 0 for torus
    REQUIRE(torus_val.normals_valid);
    REQUIRE(torus_val.uvs_valid);

    // 5. Open Primitives (Plane, Circle, Grid)
    PlaneParams pl_p;
    auto plane = ParametricPrimitiveGenerator::generate_plane(pl_p);
    auto plane_val = ParametricMeshValidator::validate_topology_and_attributes(plane, false);
    REQUIRE(!plane_val.is_watertight); // Open boundary
    REQUIRE(plane_val.is_2_manifold);  // No non-manifold edges
    REQUIRE(plane_val.normals_valid);
    REQUIRE(plane_val.uvs_valid);

    CircleParams circ_p;
    auto circle = ParametricPrimitiveGenerator::generate_circle(circ_p);
    auto circ_val = ParametricMeshValidator::validate_topology_and_attributes(circle, false);
    REQUIRE(!circ_val.is_watertight);
    REQUIRE(circ_val.is_2_manifold);
    REQUIRE(circ_val.normals_valid);
    REQUIRE(circ_val.uvs_valid);
}

TEST_CASE("PH4-05.01.005: Analytical volume, surface area, and bounding box calculation algorithms") {
    // 1. Cube analytical vs discrete: exact agreement
    CubeParams cp;
    cp.width = 3.0; cp.height = 4.0; cp.depth = 5.0;
    auto cube_analytical = ParametricPrimitiveGenerator::compute_analytical(cp);
    REQUIRE(is_close(cube_analytical.volume, 60.0));
    REQUIRE(is_close(cube_analytical.surface_area, 94.0));
    REQUIRE(is_close(cube_analytical.bbox.size().x, 3.0));
    REQUIRE(is_close(cube_analytical.bbox.size().y, 4.0));
    REQUIRE(is_close(cube_analytical.bbox.size().z, 5.0));

    auto cube_mesh = ParametricPrimitiveGenerator::generate_cube(cp);
    auto cube_comp = ParametricPrimitiveGenerator::compare_analytical_vs_discrete(cube_mesh, cube_analytical);
    REQUIRE(is_close(cube_comp.discrete_volume, 60.0, 1e-3));
    REQUIRE(is_close(cube_comp.discrete_surface_area, 94.0, 1e-3));
    REQUIRE(cube_comp.volume_error_percent < 0.01);
    REQUIRE(cube_comp.surface_area_error_percent < 0.01);

    // 2. Sphere analytical calculations
    SphereParams sp;
    sp.radius = 2.0;
    auto sphere_analytical = ParametricPrimitiveGenerator::compute_analytical(sp);
    double expected_sphere_vol = (4.0 / 3.0) * std::numbers::pi * 8.0;
    double expected_sphere_area = 4.0 * std::numbers::pi * 4.0;
    REQUIRE(is_close(sphere_analytical.volume, expected_sphere_vol));
    REQUIRE(is_close(sphere_analytical.surface_area, expected_sphere_area));
    REQUIRE(is_close(sphere_analytical.bbox.size().x, 4.0));

    // Verify discrete mesh convergence
    UvSphereParams uv_p;
    uv_p.radius = 2.0; uv_p.segments = 64; uv_p.rings = 32;
    auto sphere_mesh = ParametricPrimitiveGenerator::generate_uv_sphere(uv_p);
    auto sphere_comp = ParametricPrimitiveGenerator::compare_analytical_vs_discrete(sphere_mesh, sphere_analytical);
    REQUIRE(sphere_comp.volume_error_percent < 1.5); // < 1.5% discretization error

    // 3. Cylinder analytical calculations
    CylinderParams cyl_p;
    cyl_p.radius = 1.0; cyl_p.height = 4.0; cyl_p.radial_segments = 48;
    auto cyl_analytical = ParametricPrimitiveGenerator::compute_analytical(cyl_p);
    double expected_cyl_vol = std::numbers::pi * 1.0 * 4.0;
    double expected_cyl_area = 2.0 * std::numbers::pi * 1.0 * 4.0 + 2.0 * std::numbers::pi * 1.0;
    REQUIRE(is_close(cyl_analytical.volume, expected_cyl_vol));
    REQUIRE(is_close(cyl_analytical.surface_area, expected_cyl_area));

    auto cyl_mesh = ParametricPrimitiveGenerator::generate_cylinder(cyl_p);
    auto cyl_comp = ParametricPrimitiveGenerator::compare_analytical_vs_discrete(cyl_mesh, cyl_analytical);
    REQUIRE(cyl_comp.volume_error_percent < 1.0);

    // 4. Cone analytical calculations
    ConeParams cone_p;
    cone_p.radius = 3.0; cone_p.height = 4.0;
    auto cone_analytical = ParametricPrimitiveGenerator::compute_analytical(cone_p);
    double expected_cone_vol = (1.0 / 3.0) * std::numbers::pi * 9.0 * 4.0; // 12 * pi
    double expected_cone_area = std::numbers::pi * 3.0 * (3.0 + 5.0);      // 24 * pi
    REQUIRE(is_close(cone_analytical.volume, expected_cone_vol));
    REQUIRE(is_close(cone_analytical.surface_area, expected_cone_area));

    // 5. Torus analytical calculations
    TorusParams tp;
    tp.major_radius = 4.0; tp.minor_radius = 1.0;
    auto torus_analytical = ParametricPrimitiveGenerator::compute_analytical(tp);
    double expected_torus_vol = 2.0 * std::pow(std::numbers::pi, 2) * 4.0 * 1.0; // 8 * pi^2
    double expected_torus_area = 4.0 * std::pow(std::numbers::pi, 2) * 4.0 * 1.0; // 16 * pi^2
    REQUIRE(is_close(torus_analytical.volume, expected_torus_vol));
    REQUIRE(is_close(torus_analytical.surface_area, expected_torus_area));
    REQUIRE(is_close(torus_analytical.bbox.size().x, 10.0));
    REQUIRE(is_close(torus_analytical.bbox.size().z, 2.0));

    // 6. Fastener analytical calculations (Washer & Nut)
    WasherParams wp;
    wp.inner_diameter = 6.0; wp.outer_diameter = 12.0; wp.thickness = 2.0;
    auto washer_analytical = ParametricPrimitiveGenerator::compute_analytical(wp);
    double expected_washer_vol = std::numbers::pi * (36.0 - 9.0) * 2.0; // 54 * pi
    REQUIRE(is_close(washer_analytical.volume, expected_washer_vol));
    REQUIRE(washer_analytical.surface_area > 0.0);

    // 7. ParametricPrimitive analytical_properties() query
    ParametricPrimitive prim_torus(PrimitiveType::Torus, tp);
    auto prim_props = prim_torus.analytical_properties();
    REQUIRE(is_close(prim_props.volume, expected_torus_vol));
    REQUIRE(is_close(prim_props.surface_area, expected_torus_area));
}

TEST_CASE("PH4-05.01.006: Primitive alignment options: Align to World, Align to View, Align to Surface Normal") {
    // 1. Align to World: translate cylinder to target world origin (10, 20, 30)
    CylinderParams cyl_p;
    cyl_p.radius = 1.0; cyl_p.height = 4.0;
    auto cyl = ParametricPrimitiveGenerator::generate_cylinder(cyl_p);

    auto world_cyl = PrimitiveAligner::align_to_world(cyl, Point3D{10.0, 20.0, 30.0});
    auto bbox_world = world_cyl.compute_bbox();
    REQUIRE(is_close((bbox_world.min.x + bbox_world.max.x) * 0.5, 10.0));
    REQUIRE(is_close((bbox_world.min.y + bbox_world.max.y) * 0.5, 20.0));
    REQUIRE(is_close((bbox_world.min.z + bbox_world.max.z) * 0.5, 30.0));

    // 2. Align to View: align plane to face view direction (0, 0, -1) with up (0, 1, 0)
    PlaneParams pl_p;
    pl_p.width = 2.0; pl_p.height = 2.0;
    auto plane = ParametricPrimitiveGenerator::generate_plane(pl_p);

    // Camera looking along -Z -> plane normal faces +Z
    auto view_plane = PrimitiveAligner::align_to_view(plane, Point3D{0.0, 0.0, 5.0}, Vec3d{0.0, 0.0, -1.0}, Vec3d{0.0, 1.0, 0.0});
    REQUIRE(is_close(view_plane.normals[0].z, 1.0));
    auto view_bbox = view_plane.compute_bbox();
    REQUIRE(is_close((view_bbox.min.z + view_bbox.max.z) * 0.5, 5.0));

    // 3. Align to Surface Normal: align cylinder to surface point (5, 5, 5) with normal pointing along +X (1, 0, 0)
    auto aligned_norm_x = PrimitiveAligner::align_to_surface_normal(cyl, Point3D{5.0, 5.0, 5.0}, Vec3d{1.0, 0.0, 0.0});
    auto bbox_norm_x = aligned_norm_x.compute_bbox();
    // Cylinder length (4.0) should now align along X-axis
    REQUIRE(is_close(bbox_norm_x.size().x, 4.0));
    REQUIRE(is_close(bbox_norm_x.size().z, 2.0)); // Diameter is 2*radius = 2.0
    REQUIRE(is_close((bbox_norm_x.min.x + bbox_norm_x.max.x) * 0.5, 5.0));
    REQUIRE(is_close((bbox_norm_x.min.y + bbox_norm_x.max.y) * 0.5, 5.0));
    REQUIRE(is_close((bbox_norm_x.min.z + bbox_norm_x.max.z) * 0.5, 5.0));

    // 4. Align to inverted normal -Z (0, 0, -1)
    auto aligned_norm_neg_z = PrimitiveAligner::align_to_surface_normal(cyl, Point3D{0.0, 0.0, 0.0}, Vec3d{0.0, 0.0, -1.0});
    auto bbox_neg_z = aligned_norm_neg_z.compute_bbox();
    REQUIRE(is_close(bbox_neg_z.size().z, 4.0));
}
