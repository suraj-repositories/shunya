#include "tests/test_framework.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/cad3d/csg_boolean.hpp"

#include <cmath>
#include <string>

using namespace shunya;

namespace {
inline bool approx_eq(double a, double b, double eps = 1e-3) {
    return std::abs(a - b) <= eps * (1.0 + std::abs(b));
}
} // namespace

TEST_CASE("PH5-22.01: Expanded Parametric Primitives (Pipe, Torus, Capsule, Helix)") {
    // 1. Pipe / Hollow Cylinder
    // Outer radius 0.05m, wall thickness 0.01m (inner radius 0.04m), length 0.20m
    auto pipe = eng::ObjectGeometrySpec::make_pipe(0.05, 0.01, 0.20);
    CHECK(pipe.kind == eng::ObjectShapeKind::Pipe);
    double expected_pipe_vol = 3.141592653589793 * (0.05 * 0.05 - 0.04 * 0.04) * 0.20;
    CHECK(approx_eq(pipe.compute_volume_m3(), expected_pipe_vol));
    CHECK(pipe.compute_surface_area_m2() > 0.0);

    auto pipe_mesh = pipe.generate_solid_mesh();
    CHECK(!pipe_mesh.vertices.empty());
    CHECK(!pipe_mesh.triangles.empty());
    auto pipe_report = pipe_mesh.analyze_manifold();
    CHECK(pipe_report.vertex_count > 0);

    // 2. Torus
    // Major radius 0.10m, minor radius 0.02m
    auto torus = eng::ObjectGeometrySpec::make_torus(0.10, 0.02);
    CHECK(torus.kind == eng::ObjectShapeKind::Torus);
    double expected_torus_vol = 2.0 * std::pow(3.141592653589793, 2.0) * 0.10 * 0.02 * 0.02;
    CHECK(approx_eq(torus.compute_volume_m3(), expected_torus_vol));

    double expected_torus_area = 4.0 * std::pow(3.141592653589793, 2.0) * 0.10 * 0.02;
    CHECK(approx_eq(torus.compute_surface_area_m2(), expected_torus_area));

    auto torus_mesh = torus.generate_solid_mesh();
    CHECK(!torus_mesh.vertices.empty());
    CHECK(!torus_mesh.triangles.empty());

    // 3. Capsule
    // Radius 0.02m, total length 0.10m (cylinder length = 0.10 - 2*0.02 = 0.06m)
    auto capsule = eng::ObjectGeometrySpec::make_capsule(0.02, 0.10);
    CHECK(capsule.kind == eng::ObjectShapeKind::Capsule);
    double pi = 3.141592653589793;
    double expected_capsule_vol = (pi * 0.02 * 0.02 * 0.06) + ((4.0 / 3.0) * pi * 0.02 * 0.02 * 0.02);
    CHECK(approx_eq(capsule.compute_volume_m3(), expected_capsule_vol));

    auto capsule_mesh = capsule.generate_solid_mesh();
    CHECK(!capsule_mesh.vertices.empty());
    CHECK(!capsule_mesh.triangles.empty());

    // 4. Helical Coil (SweptPath)
    // Wire radius 0.0005m (1mm wire), coil radius 0.01m, height 0.04m, 10 turns
    auto helix = eng::ObjectGeometrySpec::make_helical_coil(0.0005, 0.01, 0.04, 10);
    CHECK(helix.kind == eng::ObjectShapeKind::SweptPath);
    CHECK(helix.compute_volume_m3() > 0.0);
    CHECK(helix.compute_surface_area_m2() > 0.0);

    auto helix_mesh = helix.generate_solid_mesh();
    CHECK(!helix_mesh.vertices.empty());
    CHECK(!helix_mesh.triangles.empty());
}

TEST_CASE("PH5-22.02: Revolved Profile, CSG Booleans & Shelling Operations") {
    // 1. Revolved Incandescent Bulb Profile
    eng::ObjectGeometrySpec bulb_spec;
    bulb_spec.kind = eng::ObjectShapeKind::RevolvedProfile;
    bulb_spec.radius_m = 0.030; // 30mm max radius
    bulb_spec.dim_y_m = 0.110;  // 110mm height

    auto bulb_mesh = bulb_spec.generate_solid_mesh();
    CHECK(!bulb_mesh.vertices.empty());
    CHECK(!bulb_mesh.triangles.empty());
    auto bulb_report = bulb_mesh.analyze_manifold();
    CHECK(bulb_report.vertex_count > 0);
    CHECK(bulb_report.face_count > 0);

    // 2. CSG Boolean Operations
    auto box_a = eng::ObjectGeometrySpec::make_box(0.10, 0.10, 0.10).generate_solid_mesh();
    auto box_b = eng::ObjectGeometrySpec::make_box(0.06, 0.12, 0.06).generate_solid_mesh();

    // CSG Difference (cutting hole through box)
    auto diff_mesh = eng::ObjectGeometrySpec::boolean_difference(box_a, box_b);
    CHECK(!diff_mesh.vertices.empty());
    CHECK(!diff_mesh.triangles.empty());

    // CSG Union
    auto union_mesh = eng::ObjectGeometrySpec::boolean_union(box_a, box_b);
    CHECK(!union_mesh.vertices.empty());
    CHECK(!union_mesh.triangles.empty());

    // CSG Intersection
    auto inter_mesh = eng::ObjectGeometrySpec::boolean_intersection(box_a, box_b);
    CHECK(!inter_mesh.vertices.empty());
    CHECK(!inter_mesh.triangles.empty());

    // 3. Shell / Hollow Body Operation
    auto solid_box = eng::ObjectGeometrySpec::make_box(0.10, 0.10, 0.10).generate_solid_mesh();
    auto shelled_mesh = eng::ObjectGeometrySpec::shell_solid_mesh(solid_box, 0.01);
    CHECK(!shelled_mesh.vertices.empty());
    CHECK(!shelled_mesh.triangles.empty());
}

TEST_CASE("PH5-22.03: Incandescent Bulb Template Structure & Region Allocation") {
    eng::PhysicalObjectManager mgr;
    core::domain::MaterialRegistry reg;

    core::domain::Material glass(core::base::Uuid::generate_random(), "Borosilicate Glass",
                                 core::domain::MaterialPhysicalState::Solid, 2230.0);
    core::domain::Material tungsten(core::base::Uuid::generate_random(), "Pure Tungsten",
                                   core::domain::MaterialPhysicalState::Solid, 19250.0);
    core::domain::ElectricalProperties elec_w;
    elec_w.electrical_conductivity = 1.8e7;
    tungsten.set_electrical(elec_w);

    core::domain::Material aluminum(core::base::Uuid::generate_random(), "Aluminum Screw Base",
                                   core::domain::MaterialPhysicalState::Solid, 2700.0);
    core::domain::ElectricalProperties elec_al;
    elec_al.electrical_conductivity = 3.5e7;
    aluminum.set_electrical(elec_al);

    core::domain::Material brass(core::base::Uuid::generate_random(), "Brass Contact",
                                core::domain::MaterialPhysicalState::Solid, 8500.0);
    core::domain::ElectricalProperties elec_br;
    elec_br.electrical_conductivity = 1.5e7;
    brass.set_electrical(elec_br);

    const auto g_id = glass.id();
    const auto t_id = tungsten.id();
    const auto a_id = aluminum.id();
    const auto b_id = brass.id();

    REQUIRE(reg.register_material(std::move(glass)).is_ok());
    REQUIRE(reg.register_material(std::move(tungsten)).is_ok());
    REQUIRE(reg.register_material(std::move(aluminum)).is_ok());
    REQUIRE(reg.register_material(std::move(brass)).is_ok());

    eng::PhysicalObject bulb;
    bulb.name = "Incandescent_Bulb_A19";
    bulb.category = "Lighting";
    bulb.geometry.kind = eng::ObjectShapeKind::RevolvedProfile;
    bulb.geometry.radius_m = 0.030;
    bulb.geometry.dim_y_m = 0.110;
    bulb.operating_temp_k = 293.15;
    bulb.operating_pressure_pa = 80000.0;

    bulb.regions = {
        {"envelope", "Glass Envelope", g_id, 0.70, 0.001, false},
        {"filament", "Tungsten Coil", t_id, 0.05, 0.00025, true},
        {"base", "Screw Shell", a_id, 0.20, 0.0005, true},
        {"contact", "Base Contact", b_id, 0.05, 0.001, true}
    };

    auto add_res = mgr.add_object(bulb, reg);
    CHECK(add_res.is_ok());

    const auto* stored = mgr.get_object(add_res.value());
    CHECK(stored != nullptr);
    CHECK(stored->regions.size() == 4);
    CHECK(stored->computed.valid == true);
    CHECK(stored->computed.volume_m3 > 0.0);
    CHECK(stored->computed.mass_kg > 0.0);
    CHECK(stored->computed.has_conductive_path == true);
}
