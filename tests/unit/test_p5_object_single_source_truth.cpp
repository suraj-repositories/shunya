#include "tests/test_framework.hpp"

#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/physical_object.hpp"

#include <cmath>
#include <string>

using namespace shunya;
using namespace shunya::core;
using namespace shunya::eng;

namespace {
inline bool approx_eq(double a, double b, double eps = 1e-4) {
    return std::abs(a - b) <= eps * (1.0 + std::abs(b));
}
} // namespace

TEST_CASE("PH5-20.01: Single Source of Truth for Computed Physical Tensors") {
    domain::MaterialRegistry registry;

    // Register pure Aluminum (density 2700 kg/m3)
    domain::Material al(base::Uuid::generate_random(), "Aluminum 6061",
                         domain::MaterialPhysicalState::Solid, 2700.0);
    domain::ThermalProperties tp;
    tp.thermal_conductivity = 167.0;
    tp.specific_heat_capacity = 896.0;
    tp.thermal_expansion_coeff = 23.1e-6;
    al.set_thermal(tp);

    domain::ElectricalProperties ep;
    ep.electrical_conductivity = 3.5e7;
    ep.electrical_resistivity = 1.0 / 3.5e7;
    al.set_electrical(ep);

    domain::MechanicalProperties mp;
    mp.youngs_modulus_pa = 68.9e9;
    al.set_mechanical(mp);

    const base::Uuid al_id = al.id();
    auto reg_res = registry.register_material(std::move(al));
    REQUIRE(reg_res.is_ok());

    PhysicalObjectManager manager;

    // 1. Create a Box object
    PhysicalObject box;
    box.name = "Al_Chassis_Box";
    box.operating_temp_k = 293.15;
    box.geometry.kind = ObjectShapeKind::Box;
    box.geometry.dim_x_m = 0.1; // w
    box.geometry.dim_y_m = 0.2; // h
    box.geometry.dim_z_m = 0.3; // d
    box.regions = {{"whole", "Solid Aluminum", al_id, 1.0, 0.0, true}};

    auto add_res = manager.add_object(box, registry);
    REQUIRE(add_res.is_ok());
    const base::Uuid box_id = add_res.value();

    const PhysicalObject* stored_box = manager.get_object(box_id);
    REQUIRE(stored_box != nullptr);

    // Assert Volume = w * h * d = 0.1 * 0.2 * 0.3 = 0.006 m3
    CHECK(approx_eq(stored_box->computed.volume_m3, 0.006));
    // Assert Mass = rho * V = 2700 * 0.006 = 16.2 kg
    CHECK(approx_eq(stored_box->computed.mass_kg, 16.2));
    // Assert Effective Density = M / V = 2700.0 kg/m3 exactly
    CHECK(approx_eq(stored_box->computed.effective_density_kg_m3, 2700.0));
    // Assert Surface Area = 2*(w*h + h*d + w*d) = 2*(0.02 + 0.06 + 0.03) = 0.22 m2
    CHECK(approx_eq(stored_box->computed.surface_area_m2, 0.22));
    // Assert Thermal Mass = M * Cp = 16.2 * 896 = 14515.2 J/K
    CHECK(approx_eq(stored_box->computed.thermal_mass_j_k, 14515.2));

    // Assert Inertia Tensor for Box
    // I_xx = (1/12) * M * (h^2 + d^2) = (1/12) * 16.2 * (0.04 + 0.09) = 0.1755
    CHECK(approx_eq(stored_box->computed.inertia_ixx, 0.1755));
    // I_yy = (1/12) * M * (w^2 + d^2) = (1/12) * 16.2 * (0.01 + 0.09) = 0.1350
    CHECK(approx_eq(stored_box->computed.inertia_iyy, 0.1350));
    // I_zz = (1/12) * M * (w^2 + h^2) = (1/12) * 16.2 * (0.01 + 0.04) = 0.0675
    CHECK(approx_eq(stored_box->computed.inertia_izz, 0.0675));

    // 2. Reactive recalculation on shape & dimension change to Sphere
    PhysicalObject sphere = *stored_box;
    sphere.geometry.kind = ObjectShapeKind::Sphere;
    sphere.geometry.radius_m = 0.05; // 5 cm radius

    auto upd_res = manager.update_object(sphere, registry);
    REQUIRE(upd_res.is_ok());

    const PhysicalObject* stored_sphere = manager.get_object(box_id);
    REQUIRE(stored_sphere != nullptr);

    constexpr double kPi = 3.14159265358979323846;
    const double expected_sphere_v = (4.0 / 3.0) * kPi * 0.05 * 0.05 * 0.05;
    const double expected_sphere_m = 2700.0 * expected_sphere_v;
    const double expected_sphere_area = 4.0 * kPi * 0.05 * 0.05;

    CHECK(approx_eq(stored_sphere->computed.volume_m3, expected_sphere_v));
    CHECK(approx_eq(stored_sphere->computed.mass_kg, expected_sphere_m));
    CHECK(approx_eq(stored_sphere->computed.surface_area_m2, expected_sphere_area));
    CHECK(approx_eq(stored_sphere->computed.effective_density_kg_m3, 2700.0));

    // Assert Sphere Inertia: I = (2/5) * M * r^2
    const double expected_sphere_I = (2.0 / 5.0) * expected_sphere_m * 0.05 * 0.05;
    CHECK(approx_eq(stored_sphere->computed.inertia_ixx, expected_sphere_I));
    CHECK(approx_eq(stored_sphere->computed.inertia_iyy, expected_sphere_I));
    CHECK(approx_eq(stored_sphere->computed.inertia_izz, expected_sphere_I));

    // 3. Reactive Recalculation on Material Perturbation (+10% rho)
    domain::Material al_perturbed = *registry.get_material(al_id);
    al_perturbed.set_density(2970.0); // +10%
    (void)registry.unregister_material(al_id);
    REQUIRE(registry.register_material(al_perturbed).is_ok());

    manager.recalculate_for_material(al_id, registry);

    const PhysicalObject* stored_recalc = manager.get_object(box_id);
    REQUIRE(stored_recalc != nullptr);
    CHECK(approx_eq(stored_recalc->computed.effective_density_kg_m3, 2970.0));
    CHECK(approx_eq(stored_recalc->computed.mass_kg, expected_sphere_v * 2970.0));
}

TEST_CASE("PH5-20.02: Name Uniqueness, Auto-Suffixing, Inline Rename & Duplication") {
    domain::MaterialRegistry registry;
    domain::Material cu(base::Uuid::generate_random(), "Copper OFHC",
                         domain::MaterialPhysicalState::Solid, 8960.0);
    const base::Uuid cu_id = cu.id();
    REQUIRE(registry.register_material(std::move(cu)).is_ok());

    PhysicalObjectManager manager;

    // 1. Add first object named Copper_Test_Bar
    PhysicalObject o1;
    o1.name = "Copper_Test_Bar";
    o1.geometry.kind = ObjectShapeKind::Cylinder;
    o1.geometry.radius_m = 0.005;
    o1.geometry.dim_y_m = 0.2;
    o1.regions = {{"whole", "Copper Core", cu_id, 1.0, 0.0, true}};

    auto res1 = manager.add_object(o1, registry);
    REQUIRE(res1.is_ok());
    CHECK(manager.get_object(res1.value())->name == "Copper_Test_Bar");

    // 2. Add second object with the identical name -> must auto-suffix to Copper_Test_Bar_1
    PhysicalObject o2 = o1;
    o2.id = base::Uuid::generate_random();
    auto res2 = manager.add_object(o2, registry);
    REQUIRE(res2.is_ok());
    CHECK(manager.get_object(res2.value())->name == "Copper_Test_Bar_1");

    // 3. Add third object with the identical name -> must auto-suffix to Copper_Test_Bar_2
    PhysicalObject o3 = o1;
    o3.id = base::Uuid::generate_random();
    auto res3 = manager.add_object(o3, registry);
    REQUIRE(res3.is_ok());
    CHECK(manager.get_object(res3.value())->name == "Copper_Test_Bar_2");

    // 4. Duplicate object o1 -> must auto-suffix to Copper_Test_Bar_3
    auto dup_res = manager.duplicate_object(res1.value(), registry);
    REQUIRE(dup_res.is_ok());
    CHECK(manager.get_object(dup_res.value())->name == "Copper_Test_Bar_3");

    // 5. Inline rename with collision protection
    // Renaming o1 to an existing name "Copper_Test_Bar_1" should fail
    auto ren_fail = manager.rename_object(res1.value(), "Copper_Test_Bar_1", registry);
    CHECK(ren_fail.is_err());
    CHECK(ren_fail.error().code == 4318);

    // Renaming o1 to a valid unique name should succeed
    auto ren_ok = manager.rename_object(res1.value(), "Custom_Busbar_A", registry);
    CHECK(ren_ok.is_ok());
    CHECK(manager.get_object(res1.value())->name == "Custom_Busbar_A");
}
