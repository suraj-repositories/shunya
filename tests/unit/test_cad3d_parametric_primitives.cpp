#include "tests/test_framework.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/cad3d/mesh_selection_engine.hpp"
#include "shunya/cad3d/mesh_topology_ops.hpp"
#include "shunya/cad3d/mesh_transform_ops.hpp"
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

TEST_CASE("PH4-05.02.001: Discrete mesh selection modes (Vertex, Edge, Face, Object) with raycast picking") {
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0; // [-1, 1]^3
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);

    // 1. Object selection picking
    Ray3D ray_center{Point3D{0.0, 0.0, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    auto pick_obj = MeshSelectionEngine::pick(cube, ray_center, MeshSelectionMode::Object);
    REQUIRE(pick_obj.hit);
    REQUIRE(pick_obj.mode == MeshSelectionMode::Object);
    REQUIRE(is_close(pick_obj.distance, 4.0)); // From z=5 to z=1

    MeshSelection sel;
    sel.set_mode(MeshSelectionMode::Object);
    bool selected = MeshSelectionEngine::pick_and_select(cube, ray_center, sel);
    REQUIRE(selected);
    REQUIRE(sel.object_selected);

    // 2. Face selection picking
    sel.set_mode(MeshSelectionMode::Face);
    auto pick_face = MeshSelectionEngine::pick(cube, ray_center, MeshSelectionMode::Face);
    REQUIRE(pick_face.hit);
    REQUIRE(pick_face.mode == MeshSelectionMode::Face);
    REQUIRE(pick_face.element_index < cube.triangles.size());

    MeshSelectionEngine::pick_and_select(cube, ray_center, sel);
    REQUIRE(sel.selected_faces.size() == 1);
    REQUIRE(sel.is_face_selected(pick_face.element_index));

    // 3. Vertex selection picking near corner (0.9, 0.9, 1.0)
    Ray3D ray_corner{Point3D{0.9, 0.9, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    sel.set_mode(MeshSelectionMode::Vertex);
    auto pick_vert = MeshSelectionEngine::pick(cube, ray_corner, MeshSelectionMode::Vertex);
    REQUIRE(pick_vert.hit);
    REQUIRE(pick_vert.mode == MeshSelectionMode::Vertex);
    // Closest vertex should be corner (1, 1, 1)
    const auto& picked_pos = cube.vertices[pick_vert.element_index];
    REQUIRE(is_close(picked_pos.x, 1.0));
    REQUIRE(is_close(picked_pos.y, 1.0));
    REQUIRE(is_close(picked_pos.z, 1.0));

    MeshSelectionEngine::pick_and_select(cube, ray_corner, sel);
    REQUIRE(sel.selected_vertices.size() == 1);
    REQUIRE(sel.is_vertex_selected(pick_vert.element_index));

    // 4. Edge selection picking near top-middle (0.0, 0.95, 1.0)
    Ray3D ray_edge{Point3D{0.0, 0.95, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    sel.set_mode(MeshSelectionMode::Edge);
    auto pick_edge = MeshSelectionEngine::pick(cube, ray_edge, MeshSelectionMode::Edge);
    REQUIRE(pick_edge.hit);
    REQUIRE(pick_edge.mode == MeshSelectionMode::Edge);

    MeshSelectionEngine::pick_and_select(cube, ray_edge, sel);
    REQUIRE(sel.selected_edges.size() == 1);
    REQUIRE(sel.is_edge_selected(pick_edge.edge.first, pick_edge.edge.second));

    // 5. Box / Frustum selection in positive octant [0, 2]^3
    BBox3D pos_box;
    pos_box.min = Point3D{0.0, 0.0, 0.0};
    pos_box.max = Point3D{2.0, 2.0, 2.0};

    sel.set_mode(MeshSelectionMode::Vertex);
    MeshSelectionEngine::select_in_box(cube, pos_box, sel);
    REQUIRE(!sel.selected_vertices.empty());
    for (uint32_t v_idx : sel.selected_vertices) {
        REQUIRE(cube.vertices[v_idx].x >= 0.0);
        REQUIRE(cube.vertices[v_idx].y >= 0.0);
        REQUIRE(cube.vertices[v_idx].z >= 0.0);
    }

    // 6. Select All / Deselect All
    sel.select_all(cube);
    REQUIRE(sel.selected_vertices.size() == cube.vertices.size());
    sel.deselect_all();
    REQUIRE(sel.empty());
}

TEST_CASE("PH4-05.02.002: Blender-style mesh transform operations: Grab (G), Rotate (R), Scale (S) with axis-locking") {
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0; // [-1, 1]^3
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);

    MeshSelection sel;
    sel.set_mode(MeshSelectionMode::Object);
    sel.select_object(true);

    // 1. Grab/Translate (G) with X-axis locking
    MeshTransformOps::translate(cube, sel, Vec3d{5.0, 10.0, 15.0}, TransformAxisLock::X);
    auto bbox_g_x = cube.compute_bbox();
    // Only X should have moved by +5.0; Y and Z center remain 0.0
    REQUIRE(is_close((bbox_g_x.min.x + bbox_g_x.max.x) * 0.5, 5.0));
    REQUIRE(is_close((bbox_g_x.min.y + bbox_g_x.max.y) * 0.5, 0.0));
    REQUIRE(is_close((bbox_g_x.min.z + bbox_g_x.max.z) * 0.5, 0.0));

    // 2. Grab with PlaneXY locking (Shift+Z): only X and Y change, Z unchanged
    MeshTransformOps::translate(cube, sel, Vec3d{2.0, 3.0, 99.0}, TransformAxisLock::PlaneXY);
    auto bbox_g_xy = cube.compute_bbox();
    REQUIRE(is_close((bbox_g_xy.min.x + bbox_g_xy.max.x) * 0.5, 7.0));
    REQUIRE(is_close((bbox_g_xy.min.y + bbox_g_xy.max.y) * 0.5, 3.0));
    REQUIRE(is_close((bbox_g_xy.min.z + bbox_g_xy.max.z) * 0.5, 0.0));

    // 3. Rotate (R) around Z-axis with pivot at WorldOrigin (0, 0, 0)
    // First reset position to origin
    auto cube_rot = ParametricPrimitiveGenerator::generate_cube(cp);
    MeshSelection sel_rot;
    sel_rot.set_mode(MeshSelectionMode::Object);
    sel_rot.select_object(true);

    MeshTransformOps::rotate(cube_rot, sel_rot, 0.5 * std::numbers::pi, TransformAxisLock::Z, {0,0,1}, TransformPivot::WorldOrigin);
    // After 90-degree Z rotation, bbox of a cube [-1, 1]^3 remains [-1, 1]^3
    auto bbox_rot = cube_rot.compute_bbox();
    REQUIRE(is_close(bbox_rot.size().x, 2.0));
    REQUIRE(is_close(bbox_rot.size().y, 2.0));
    REQUIRE(is_close(bbox_rot.size().z, 2.0));

    // 4. Scale (S) with axis locking (X only)
    auto cube_scale = ParametricPrimitiveGenerator::generate_cube(cp);
    MeshSelection sel_scale;
    sel_scale.set_mode(MeshSelectionMode::Object);
    sel_scale.select_object(true);

    MeshTransformOps::scale(cube_scale, sel_scale, Vec3d{3.0, 10.0, 10.0}, TransformAxisLock::X, TransformPivot::WorldOrigin);
    auto bbox_scale_x = cube_scale.compute_bbox();
    // X width scaled by 3.0 (from 2.0 to 6.0), Y and Z remain 2.0
    REQUIRE(is_close(bbox_scale_x.size().x, 6.0));
    REQUIRE(is_close(bbox_scale_x.size().y, 2.0));
    REQUIRE(is_close(bbox_scale_x.size().z, 2.0));

    // 5. Scale with PlaneXZ locking (Shift+Y): X and Z scaled by 2.0, Y unchanged
    MeshTransformOps::scale(cube_scale, sel_scale, Vec3d{2.0, 99.0, 2.0}, TransformAxisLock::PlaneXZ, TransformPivot::WorldOrigin);
    auto bbox_scale_xz = cube_scale.compute_bbox();
    REQUIRE(is_close(bbox_scale_xz.size().x, 12.0)); // 6.0 * 2.0
    REQUIRE(is_close(bbox_scale_xz.size().y, 2.0));  // Unchanged
    REQUIRE(is_close(bbox_scale_xz.size().z, 4.0));  // 2.0 * 2.0
}

TEST_CASE("PH4-05.02.003: Face Extrude along vertex/face normals with interactive depth handles") {
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0; // [-1, 1]^3
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);
    size_t tri_count_before = cube.triangles.size();
    size_t vert_count_before = cube.vertices.size();

    // Pick top face (+Z)
    Ray3D ray_top{Point3D{0.0, 0.0, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    auto pick_res = MeshSelectionEngine::pick(cube, ray_top, MeshSelectionMode::Face);
    REQUIRE(pick_res.hit);
    uint32_t top_face = pick_res.element_index;

    // Extrude face along its normal by distance 3.0
    ExtrudeOptions opt;
    opt.distance = 3.0;
    bool ok = MeshTopologyOps::extrude_faces(cube, {top_face}, opt);
    REQUIRE(ok);

    // Verify vertex count increased by 3 and triangle count increased by 6 (3 side quads)
    REQUIRE(cube.vertices.size() == vert_count_before + 3);
    REQUIRE(cube.triangles.size() == tri_count_before + 6);

    // Verify new bbox: Z max is now 1.0 + 3.0 = 4.0
    auto bbox_extruded = cube.compute_bbox();
    REQUIRE(is_close(bbox_extruded.max.z, 4.0));
    REQUIRE(is_close(bbox_extruded.size().z, 5.0)); // From -1.0 to 4.0

    // Verify watertight 2-manifold topology is preserved
    auto val = ParametricMeshValidator::validate_topology_and_attributes(cube, true);
    REQUIRE(val.is_watertight);
    REQUIRE(val.is_2_manifold);
}

TEST_CASE("PH4-05.02.004: Implement Inset Faces with thickness and depth parameters") {
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0; // [-1, 1]^3
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);
    size_t tri_count_before = cube.triangles.size();
    size_t vert_count_before = cube.vertices.size();

    // Pick top face (+Z)
    Ray3D ray_top{Point3D{0.0, 0.0, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    auto pick_res = MeshSelectionEngine::pick(cube, ray_top, MeshSelectionMode::Face);
    REQUIRE(pick_res.hit);
    uint32_t top_face = pick_res.element_index;

    // Inset face: thickness = 0.4 (shrink towards centroid), depth = -0.5 (recessed pocket)
    InsetOptions in_opt;
    in_opt.thickness = 0.4;
    in_opt.depth = -0.5;
    bool ok = MeshTopologyOps::inset_faces(cube, {top_face}, in_opt);
    REQUIRE(ok);

    REQUIRE(cube.vertices.size() == vert_count_before + 3);
    REQUIRE(cube.triangles.size() == tri_count_before + 6);

    // Verify the newly created inset vertices are recessed at Z = 1.0 - 0.5 = 0.5
    for (size_t i = vert_count_before; i < cube.vertices.size(); ++i) {
        REQUIRE(is_close(cube.vertices[i].z, 0.5));
    }

    // Verify watertight 2-manifold topology is preserved
    auto val = ParametricMeshValidator::validate_topology_and_attributes(cube, true);
    REQUIRE(val.is_watertight);
    REQUIRE(val.is_2_manifold);
}

TEST_CASE("PH4-05.02.005: Edge and Vertex Beveling with multi-segment profile rounding") {
    // 1. Vertex Beveling
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0;
    auto cube_v = ParametricPrimitiveGenerator::generate_cube(cp);

    // Pick top-corner vertex (1, 1, 1)
    Ray3D ray_corner{Point3D{0.9, 0.9, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    auto pick_v = MeshSelectionEngine::pick(cube_v, ray_corner, MeshSelectionMode::Vertex);
    REQUIRE(pick_v.hit);
    uint32_t corner_idx = pick_v.element_index;
    Point3D pos_before = cube_v.vertices[corner_idx];

    BevelOptions v_opt;
    v_opt.vertex_only = true;
    v_opt.offset = 0.3;
    bool ok_v = MeshTopologyOps::bevel(cube_v, {}, {corner_idx}, v_opt);
    REQUIRE(ok_v);

    Point3D pos_after = cube_v.vertices[corner_idx];
    // Position should have truncated inwards towards center
    REQUIRE(pos_after.length() < pos_before.length());

    // 2. Edge Beveling
    auto cube_e = ParametricPrimitiveGenerator::generate_cube(cp);
    size_t vert_count_before = cube_e.vertices.size();

    Ray3D ray_edge{Point3D{0.0, 0.95, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    auto pick_e = MeshSelectionEngine::pick(cube_e, ray_edge, MeshSelectionMode::Edge);
    REQUIRE(pick_e.hit);

    BevelOptions e_opt;
    e_opt.vertex_only = false;
    e_opt.offset = 0.15;
    e_opt.segments = 2;
    bool ok_e = MeshTopologyOps::bevel(cube_e, {pick_e.edge}, {}, e_opt);
    REQUIRE(ok_e);
    REQUIRE(cube_e.vertices.size() > vert_count_before);
}

TEST_CASE("PH4-05.02.006: Implement Loop Cut and Slide with preview rings and segment counts") {
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0;
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);
    size_t vert_count_before = cube.vertices.size();
    size_t tri_count_before = cube.triangles.size();

    // Pick top edge
    Ray3D ray_edge{Point3D{0.0, 0.95, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    auto pick_edge = MeshSelectionEngine::pick(cube, ray_edge, MeshSelectionMode::Edge);
    REQUIRE(pick_edge.hit);
    uint32_t eu = pick_edge.edge.first;
    uint32_t ev = pick_edge.edge.second;

    // Apply loop cut with slide = 0.0 (exact midpoint)
    LoopCutOptions opt;
    opt.cuts = 1;
    opt.slide = 0.0;
    bool ok = MeshTopologyOps::loop_cut(cube, eu, ev, opt);
    REQUIRE(ok);

    // Verify 1 new vertex and 2 triangles added
    REQUIRE(cube.vertices.size() == vert_count_before + 1);
    REQUIRE(cube.triangles.size() >= tri_count_before + 1);

    // Midpoint check
    Point3D pu = cube.vertices[eu];
    Point3D pv = cube.vertices[ev];
    Point3D expected_mid = (pu + pv) * 0.5;
    Point3D actual_cut = cube.vertices.back();
    REQUIRE(is_close(actual_cut.x, expected_mid.x));
    REQUIRE(is_close(actual_cut.y, expected_mid.y));
    REQUIRE(is_close(actual_cut.z, expected_mid.z));

    // Test with slide factor = 0.6
    LoopCutOptions opt_slide;
    opt_slide.slide = 0.6; // t = 0.8
    bool ok_slide = MeshTopologyOps::loop_cut(cube, eu, cube.triangles.back()[0], opt_slide);
    REQUIRE(ok_slide);
}

TEST_CASE("PH4-05.03.001: Subdivide Face operation with linear and Catmull-Clark smoothing modes") {
    // 1. Linear Face Subdivision on a Plane
    PlaneParams pl_p;
    pl_p.width = 4.0; pl_p.height = 4.0; pl_p.segments_x = 1; pl_p.segments_y = 1; // 2 triangles
    auto plane_linear = ParametricPrimitiveGenerator::generate_plane(pl_p);
    REQUIRE(plane_linear.triangles.size() == 2);

    // Subdivide triangle 0 with 1 cut (1 -> 4 triangles)
    bool ok1 = MeshTopologyOps::subdivide_faces(plane_linear, {0}, 1, SubdivideMode::Linear);
    REQUIRE(ok1);
    // Triangle 0 was replaced, and 3 added -> net 2 - 1 + 4 = 5 triangles
    REQUIRE(plane_linear.triangles.size() == 5);
    // In Linear mode, all vertices remain strictly at Z = 0
    for (const auto& v : plane_linear.vertices) {
        REQUIRE(is_close(v.z, 0.0));
    }

    // 2. Catmull-Clark Smoothing on a Cube
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0;
    auto cube_cc = ParametricPrimitiveGenerator::generate_cube(cp);
    size_t tri_count_before = cube_cc.triangles.size();

    // Subdivide top face
    Ray3D ray_top{Point3D{0.0, 0.0, 5.0}, Vec3d{0.0, 0.0, -1.0}};
    auto pick_res = MeshSelectionEngine::pick(cube_cc, ray_top, MeshSelectionMode::Face);
    REQUIRE(pick_res.hit);

    bool ok_cc = MeshTopologyOps::subdivide_faces(cube_cc, {pick_res.element_index}, 1, SubdivideMode::CatmullClark);
    REQUIRE(ok_cc);
    REQUIRE(cube_cc.triangles.size() == tri_count_before + 3);

    // 3. Multi-cut subdivision (cuts = 2)
    auto plane_multi = ParametricPrimitiveGenerator::generate_plane(pl_p);
    bool ok_multi = MeshTopologyOps::subdivide_faces(plane_multi, {0}, 2, SubdivideMode::Linear);
    REQUIRE(ok_multi);
    // Level 1: 1 -> 4 tris; Level 2: 4 -> 16 tris. (Original had 2, so 2 - 1 + 16 = 17)
    REQUIRE(plane_multi.triangles.size() == 17);
}

TEST_CASE("PH4-05.03.002: Bridge Edge Loops to connect separate face loops and punch holes") {
    SolidMesh3D mesh;
    const size_t N = 8;
    std::vector<uint32_t> loop_a(N);
    std::vector<uint32_t> loop_b(N);

    // Loop A: circle at Z = 0, radius = 1.0
    for (size_t i = 0; i < N; ++i) {
        double th = (static_cast<double>(i) / N) * 2.0 * std::numbers::pi;
        loop_a[i] = mesh.add_vertex(Point3D{std::cos(th), std::sin(th), 0.0});
    }

    // Loop B: circle at Z = 5.0, radius = 2.0
    for (size_t i = 0; i < N; ++i) {
        double th = (static_cast<double>(i) / N) * 2.0 * std::numbers::pi;
        loop_b[i] = mesh.add_vertex(Point3D{2.0 * std::cos(th), 2.0 * std::sin(th), 5.0});
    }

    // 1. Single segment bridge
    bool ok = MeshTopologyOps::bridge_edge_loops(mesh, loop_a, loop_b, 1, 0.0);
    REQUIRE(ok);
    // N quads = N * 2 = 16 triangles
    REQUIRE(mesh.triangles.size() == 16);

    auto bbox = mesh.compute_bbox();
    REQUIRE(is_close(bbox.min.z, 0.0));
    REQUIRE(is_close(bbox.max.z, 5.0));
    REQUIRE(is_close(bbox.size().x, 4.0)); // Loop B diameter is 2*2 = 4

    // 2. Multi-segment bridge (segments = 3)
    SolidMesh3D mesh_multi;
    std::vector<uint32_t> la_m(N);
    std::vector<uint32_t> lb_m(N);
    for (size_t i = 0; i < N; ++i) {
        double th = (static_cast<double>(i) / N) * 2.0 * std::numbers::pi;
        la_m[i] = mesh_multi.add_vertex(Point3D{std::cos(th), std::sin(th), 0.0});
        lb_m[i] = mesh_multi.add_vertex(Point3D{std::cos(th), std::sin(th), 6.0});
    }

    bool ok_multi = MeshTopologyOps::bridge_edge_loops(mesh_multi, la_m, lb_m, 3, 0.0);
    REQUIRE(ok_multi);
    // 3 segments * 16 triangles = 48 triangles
    REQUIRE(mesh_multi.triangles.size() == 48);
    // 2 intermediate rings of 8 = 16 new vertices added + 16 original = 32 vertices
    REQUIRE(mesh_multi.vertices.size() == 32);

    // 3. Twist support
    SolidMesh3D mesh_twist;
    std::vector<uint32_t> la_t(N);
    std::vector<uint32_t> lb_t(N);
    for (size_t i = 0; i < N; ++i) {
        double th = (static_cast<double>(i) / N) * 2.0 * std::numbers::pi;
        la_t[i] = mesh_twist.add_vertex(Point3D{std::cos(th), std::sin(th), 0.0});
        lb_t[i] = mesh_twist.add_vertex(Point3D{std::cos(th), std::sin(th), 4.0});
    }
    bool ok_twist = MeshTopologyOps::bridge_edge_loops(mesh_twist, la_t, lb_t, 1, 2.0); // twist by 2 vertices
    REQUIRE(ok_twist);
    REQUIRE(mesh_twist.triangles.size() == 16);
}

TEST_CASE("PH4-05.03.003: Vertex Merge (At First, At Last, At Center, By Distance threshold)") {
    // 1. Merge At Center
    SolidMesh3D mesh_center;
    uint32_t v0 = mesh_center.add_vertex(Point3D{0.0, 0.0, 0.0});
    uint32_t v1 = mesh_center.add_vertex(Point3D{2.0, 0.0, 0.0});
    uint32_t v2 = mesh_center.add_vertex(Point3D{1.0, 2.0, 0.0});
    uint32_t v3 = mesh_center.add_vertex(Point3D{1.0, -2.0, 0.0});

    mesh_center.add_triangle(v0, v1, v2);
    mesh_center.add_triangle(v0, v3, v1);

    // Merge v0 and v1 at center -> midpoint is (1.0, 0.0, 0.0)
    size_t merged = MeshTopologyOps::merge_vertices(mesh_center, {v0, v1}, VertexMergeMode::AtCenter);
    REQUIRE(merged == 1);
    Point3D p_survivor = mesh_center.vertices[v0];
    REQUIRE(is_close(p_survivor.x, 1.0));
    REQUIRE(is_close(p_survivor.y, 0.0));
    REQUIRE(is_close(p_survivor.z, 0.0));
    // Degenerate triangles where v0 == v1 were removed
    REQUIRE(mesh_center.triangles.empty());

    // 2. Merge At First
    SolidMesh3D mesh_first;
    uint32_t f0 = mesh_first.add_vertex(Point3D{0.0, 0.0, 0.0});
    uint32_t f1 = mesh_first.add_vertex(Point3D{10.0, 0.0, 0.0});
    size_t m_first = MeshTopologyOps::merge_vertices(mesh_first, {f0, f1}, VertexMergeMode::AtFirst);
    REQUIRE(m_first == 1);
    REQUIRE(is_close(mesh_first.vertices[f0].x, 0.0));

    // 3. Merge At Last
    SolidMesh3D mesh_last;
    uint32_t l0 = mesh_last.add_vertex(Point3D{0.0, 0.0, 0.0});
    uint32_t l1 = mesh_last.add_vertex(Point3D{10.0, 0.0, 0.0});
    size_t m_last = MeshTopologyOps::merge_vertices(mesh_last, {l0, l1}, VertexMergeMode::AtLast);
    REQUIRE(m_last == 1);
    REQUIRE(is_close(mesh_last.vertices[l1].x, 10.0));

    // 4. Merge By Distance Threshold
    SolidMesh3D mesh_dist;
    uint32_t d0 = mesh_dist.add_vertex(Point3D{0.0, 0.0, 0.0});
    uint32_t d1 = mesh_dist.add_vertex(Point3D{0.0001, 0.0, 0.0}); // very close (0.1 mm)
    uint32_t d2 = mesh_dist.add_vertex(Point3D{5.0, 5.0, 0.0});    // far away

    mesh_dist.add_triangle(d0, d1, d2);
    size_t m_dist = MeshTopologyOps::merge_vertices(mesh_dist, {d0, d1, d2}, VertexMergeMode::ByDistance, 0.001);
    REQUIRE(m_dist == 1);
    // d0 and d1 merged; degenerate triangle d0-d0-d2 removed
    REQUIRE(mesh_dist.triangles.empty());
}

TEST_CASE("PH4-05.03.004: Implement Edge and Face Dissolve, preserving planar polygon topology") {
    // 1. Edge Dissolve on coplanar quad
    SolidMesh3D quad_mesh;
    uint32_t v0 = quad_mesh.add_vertex(Point3D{0.0, 0.0, 0.0});
    uint32_t v1 = quad_mesh.add_vertex(Point3D{2.0, 0.0, 0.0});
    uint32_t v2 = quad_mesh.add_vertex(Point3D{2.0, 2.0, 0.0});
    uint32_t v3 = quad_mesh.add_vertex(Point3D{0.0, 2.0, 0.0});

    // Two triangles sharing diagonal (v0, v2)
    quad_mesh.add_triangle(v0, v1, v2);
    quad_mesh.add_triangle(v0, v2, v3);

    bool ok_edge = MeshTopologyOps::dissolve_edges(quad_mesh, {{v0, v2}});
    REQUIRE(ok_edge);
    // The edge (v0, v2) was dissolved; retriangulated across opposite diagonal (v1, v3)
    bool has_old_edge = false;
    for (const auto& tri : quad_mesh.triangles) {
        for (int k = 0; k < 3; ++k) {
            if ((tri[k] == v0 && tri[(k+1)%3] == v2) || (tri[k] == v2 && tri[(k+1)%3] == v0)) {
                has_old_edge = true;
            }
        }
    }
    REQUIRE(!has_old_edge);

    // 2. Face Dissolve on subdivided plane
    PlaneParams pl_p;
    pl_p.width = 4.0; pl_p.height = 4.0; pl_p.segments_x = 2; pl_p.segments_y = 2; // 8 triangles
    auto plane = ParametricPrimitiveGenerator::generate_plane(pl_p);
    REQUIRE(plane.triangles.size() == 8);

    // Dissolve two adjacent coplanar triangles {0, 1}
    bool ok_faces = MeshTopologyOps::dissolve_faces(plane, {0, 1});
    REQUIRE(ok_faces);
    // Boundary of {0, 1} preserved, internal shared edge dissolved
    auto bbox = plane.compute_bbox();
    REQUIRE(is_close(bbox.size().x, 4.0));
    REQUIRE(is_close(bbox.size().y, 4.0));
}

TEST_CASE("PH4-05.03.005: Knife and Bisect tools allowing manual slicing of geometry across planes") {
    // 1. Bisect Cube with clearing inner half-space and filling cap
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0; // [-1, 1]^3
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);
    double initial_vol = cube.compute_volume();
    REQUIRE(is_close(initial_vol, 8.0));

    Plane3D slice_plane;
    slice_plane.point = Point3D{0.0, 0.0, 0.0};
    slice_plane.normal = Vec3d{0.0, 0.0, 1.0}; // Slice along XY plane at Z = 0

    BisectOptions opt;
    opt.clear_inner = true; // Remove Z < 0
    opt.clear_outer = false;
    opt.fill_cap = true;

    bool ok = MeshTopologyOps::bisect(cube, slice_plane, opt);
    REQUIRE(ok);

    auto bbox = cube.compute_bbox();
    REQUIRE(is_close(bbox.max.z, 1.0));
    REQUIRE(is_close(bbox.min.z, 0.0)); // Lower bound truncated at Z = 0
    REQUIRE(is_close(bbox.size().z, 1.0));

    // Volume of remaining half should be approximately 4.0
    double bisected_vol = cube.compute_volume();
    REQUIRE(is_close(bisected_vol, 4.0, 0.1));

    // 2. Knife slice without clearing (topology cut)
    auto cube_knife = ParametricPrimitiveGenerator::generate_cube(cp);
    size_t tri_count_before = cube_knife.triangles.size();

    BisectOptions opt_knife;
    opt_knife.clear_inner = false;
    opt_knife.clear_outer = false;
    opt_knife.fill_cap = false;

    bool ok_knife = MeshTopologyOps::bisect(cube_knife, slice_plane, opt_knife);
    REQUIRE(ok_knife);
    // Intersected triangles were split into 3 each
    REQUIRE(cube_knife.triangles.size() > tri_count_before);
    auto bbox_knife = cube_knife.compute_bbox();
    REQUIRE(is_close(bbox_knife.size().z, 2.0)); // Intact geometry
}

TEST_CASE("PH4-05.03.006: Normal orientation inspection (stalks) and automated Recalculate Outside") {
    CubeParams cp;
    cp.width = 2.0; cp.height = 2.0; cp.depth = 2.0; // [-1, 1]^3
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);

    // 1. Generate normal stalks (length = 0.5)
    auto stalks = MeshTopologyOps::generate_normal_stalks(cube, 0.5);
    REQUIRE(stalks.size() == cube.triangles.size());
    for (const auto& stalk : stalks) {
        REQUIRE(is_close((stalk.end_point - stalk.origin).length(), 0.5));
        // Stalk origin dot normal should be positive for convex centered cube
        REQUIRE(stalk.origin.dot(stalk.normal) > 0.0);
    }

    // 2. Invert normals intentionally
    MeshTopologyOps::flip_normals(cube);
    double inv_signed_vol = cube.compute_signed_volume();
    REQUIRE(inv_signed_vol < -7.9); // Negative signed volume indicates inside-out winding

    auto inverted_stalks = MeshTopologyOps::generate_normal_stalks(cube, 0.5);
    for (const auto& stalk : inverted_stalks) {
        // Face normal now points inward (towards origin)
        REQUIRE(stalk.origin.dot(stalk.normal) < 0.0);
    }

    // 3. Automated Recalculate Outside
    bool flipped = MeshTopologyOps::recalculate_normals_outside(cube);
    REQUIRE(flipped);

    double fixed_signed_vol = cube.compute_signed_volume();
    REQUIRE(fixed_signed_vol > 7.9); // Positive signed volume restored

    auto fixed_stalks = MeshTopologyOps::generate_normal_stalks(cube, 0.5);
    for (const auto& stalk : fixed_stalks) {
        REQUIRE(stalk.origin.dot(stalk.normal) > 0.0);
    }
}

TEST_CASE("PH4-05.03.007: Manifold topology verification tools identifying non-manifold vertices, edges, and degenerate faces") {
    // 1. Clean watertight 2-manifold primitive (Icosphere)
    IcosphereParams ip;
    ip.radius = 1.0;
    ip.subdivisions = 1;
    auto ico = ParametricPrimitiveGenerator::generate_icosphere(ip);
    auto clean_report = MeshTopologyOps::verify_manifold_topology(ico);
    REQUIRE(clean_report.is_manifold);
    REQUIRE(clean_report.is_closed);
    REQUIRE(clean_report.boundary_edges.empty());
    REQUIRE(clean_report.non_manifold_edges.empty());
    REQUIRE(clean_report.non_manifold_vertices.empty());
    REQUIRE(clean_report.degenerate_faces.empty());
    REQUIRE(clean_report.isolated_vertices.empty());

    // Also verify welded cube
    CubeParams cp;
    cp.width = 1.0; cp.height = 1.0; cp.depth = 1.0;
    auto cube = ParametricPrimitiveGenerator::generate_cube(cp);
    cube.weld_vertices();
    auto cube_report = MeshTopologyOps::verify_manifold_topology(cube);
    REQUIRE(cube_report.is_manifold);
    REQUIRE(cube_report.is_closed);

    // 2. Open manifold mesh (plane / single triangle)
    SolidMesh3D open_mesh;
    open_mesh.add_vertex({0.0, 0.0, 0.0});
    open_mesh.add_vertex({1.0, 0.0, 0.0});
    open_mesh.add_vertex({0.0, 1.0, 0.0});
    open_mesh.add_triangle(0, 1, 2);

    auto open_report = MeshTopologyOps::verify_manifold_topology(open_mesh);
    REQUIRE(open_report.is_manifold);
    REQUIRE_FALSE(open_report.is_closed);
    REQUIRE(open_report.boundary_edges.size() == 3);
    REQUIRE(open_report.non_manifold_edges.empty());
    REQUIRE(open_report.non_manifold_vertices.empty());

    // 3. Degenerate zero-area face
    SolidMesh3D degen_mesh = open_mesh;
    uint32_t degen_idx = static_cast<uint32_t>(degen_mesh.triangles.size());
    degen_mesh.triangles.push_back({0, 1, 0}); // Directly insert degenerate triangle
    auto degen_report = MeshTopologyOps::verify_manifold_topology(degen_mesh);
    REQUIRE_FALSE(degen_report.is_manifold);
    REQUIRE(degen_report.degenerate_faces.size() == 1);
    REQUIRE(degen_report.degenerate_faces[0] == degen_idx);

    // 4. Non-manifold edge (3 faces sharing edge 0-1)
    SolidMesh3D edge_mesh;
    edge_mesh.add_vertex({0.0, 0.0, 0.0}); // 0
    edge_mesh.add_vertex({0.0, 0.0, 1.0}); // 1 (shared spine edge 0-1)
    edge_mesh.add_vertex({1.0, 0.0, 0.0}); // 2 (page 1)
    edge_mesh.add_vertex({0.0, 1.0, 0.0}); // 3 (page 2)
    edge_mesh.add_vertex({-1.0, 0.0, 0.0}); // 4 (page 3)
    edge_mesh.add_triangle(0, 1, 2);
    edge_mesh.add_triangle(0, 1, 3);
    edge_mesh.add_triangle(0, 1, 4);

    auto edge_report = MeshTopologyOps::verify_manifold_topology(edge_mesh);
    REQUIRE_FALSE(edge_report.is_manifold);
    REQUIRE(edge_report.non_manifold_edges.size() == 1);
    REQUIRE(edge_report.non_manifold_edges[0] == std::make_pair(0u, 1u));

    // 5. Non-manifold vertex (hourglass / pinch point at vertex 0)
    SolidMesh3D pinch_mesh;
    pinch_mesh.add_vertex({0.0, 0.0, 0.0}); // 0 (shared pinch point)
    pinch_mesh.add_vertex({-1.0, 1.0, 0.0}); // 1
    pinch_mesh.add_vertex({-1.0, -1.0, 0.0}); // 2
    pinch_mesh.add_vertex({1.0, 1.0, 0.0}); // 3
    pinch_mesh.add_vertex({1.0, -1.0, 0.0}); // 4
    pinch_mesh.add_triangle(0, 1, 2); // left triangle
    pinch_mesh.add_triangle(0, 3, 4); // right triangle touching ONLY at vertex 0

    auto pinch_report = MeshTopologyOps::verify_manifold_topology(pinch_mesh);
    REQUIRE_FALSE(pinch_report.is_manifold);
    REQUIRE(pinch_report.non_manifold_vertices.size() == 1);
    REQUIRE(pinch_report.non_manifold_vertices[0] == 0);

    // 6. Isolated vertex detection
    pinch_mesh.add_vertex({5.0, 5.0, 5.0}); // Index 5 unreferenced
    auto iso_report = MeshTopologyOps::verify_manifold_topology(pinch_mesh);
    REQUIRE(iso_report.isolated_vertices.size() == 1);
    REQUIRE(iso_report.isolated_vertices[0] == 5);
}
