#include "tests/test_framework.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_curve.hpp"
#include "shunya/core/domain/material_assignment.hpp"

#include <cmath>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("Material schema, state, provenance and version metadata (MAT-01)") {
    auto id = base::Uuid::generate_random();
    Material mat(id, "Titanium Alloy Ti-6Al-4V", MaterialPhysicalState::Solid, 4430.0);
    mat.set_description("Grade 5 titanium alloy");
    mat.set_composition("Ti 90%, Al 6%, V 4%");
    mat.set_provenance("ASTM B265");
    mat.set_source("Metals Handbook");
    mat.set_version({2, 1, 0});

    REQUIRE(mat.id() == id);
    REQUIRE(mat.name() == "Titanium Alloy Ti-6Al-4V");
    REQUIRE(mat.state() == MaterialPhysicalState::Solid);
    REQUIRE(std::abs(mat.density() - 4430.0) < 1e-6);
    REQUIRE(mat.description() == "Grade 5 titanium alloy");
    REQUIRE(mat.composition() == "Ti 90%, Al 6%, V 4%");
    REQUIRE(mat.provenance() == "ASTM B265");
    REQUIRE(mat.source() == "Metals Handbook");
    REQUIRE(mat.version() == base::Version(2, 1, 0));

    // Material state strings
    REQUIRE(material_physical_state_to_string(MaterialPhysicalState::Solid) == "Solid");
    REQUIRE(material_physical_state_to_string(MaterialPhysicalState::Liquid) == "Liquid");
    REQUIRE(material_physical_state_to_string(MaterialPhysicalState::Gas) == "Gas");
    REQUIRE(material_physical_state_to_string(MaterialPhysicalState::Plasma) == "Plasma");

    auto s1 = material_physical_state_from_string("Liquid");
    REQUIRE(s1.is_ok());
    REQUIRE(s1.value() == MaterialPhysicalState::Liquid);

    auto s2 = material_physical_state_from_string("Gas");
    REQUIRE(s2.is_ok());
    REQUIRE(s2.value() == MaterialPhysicalState::Gas);

    auto s_invalid = material_physical_state_from_string("BoseEinsteinCondensate");
    REQUIRE(s_invalid.is_err());
}

TEST_CASE("Mechanical properties and effective modulus calculations (MAT-02)") {
    MechanicalProperties mech;
    mech.youngs_modulus_pa = 200.0e9; // 200 GPa
    mech.poissons_ratio = 0.25;
    mech.yield_strength_pa = 400.0e6; // 400 MPa
    mech.ultimate_strength_pa = 600.0e6; // 600 MPa
    mech.tangent_modulus_pa = 20.0e9; // 20 GPa plastic modulus

    // Analytical: G = E / (2 * (1 + nu)) = 200e9 / (2 * 1.25) = 80 GPa
    double expected_G = 80.0e9;
    REQUIRE(std::abs(mech.effective_shear_modulus() - expected_G) < 1e3);

    // Analytical: K = E / (3 * (1 - 2*nu)) = 200e9 / (3 * 0.5) = 133.333 GPa
    double expected_K = 200.0e9 / 1.5;
    REQUIRE(std::abs(mech.effective_bulk_modulus() - expected_K) < 1e3);

    // Explicit shear modulus overrides calculation
    mech.shear_modulus_pa = 82.0e9;
    REQUIRE(std::abs(mech.effective_shear_modulus() - 82.0e9) < 1e-6);

    Material mat(base::Uuid::generate_random(), "Steel Test", MaterialPhysicalState::Solid, 7800.0);
    mat.set_mechanical(mech);

    // Elastic evaluation: strain = 0.001 -> stress = 200e9 * 0.001 = 200 MPa (< yield 400 MPa)
    auto stress_elastic = mat.evaluate_stress(0.001);
    REQUIRE(stress_elastic.is_ok());
    REQUIRE(std::abs(stress_elastic.value() - 200.0e6) < 1.0);

    // Plastic evaluation: yield strain = 400e6 / 200e9 = 0.002
    // For strain = 0.005: stress = 400e6 + 20e9 * (0.005 - 0.002) = 400e6 + 60e6 = 460 MPa
    auto stress_plastic = mat.evaluate_stress(0.005);
    REQUIRE(stress_plastic.is_ok());
    REQUIRE(std::abs(stress_plastic.value() - 460.0e6) < 1.0);
}

TEST_CASE("Thermal properties and scalar evaluation (MAT-03)") {
    ThermalProperties therm;
    therm.thermal_conductivity = 45.0;     // 45 W/(m*K)
    therm.specific_heat_capacity = 500.0;  // 500 J/(kg*K)
    therm.thermal_expansion_coeff = 1.1e-5;// 11 ppm/K
    therm.emissivity = 0.75;
    therm.melting_point_k = 1800.0;
    therm.boiling_point_k = 3000.0;
    therm.latent_heat_fusion = 2.5e5;
    therm.latent_heat_vaporization = 6.0e6;

    Material mat(base::Uuid::generate_random(), "Thermal Alloy", MaterialPhysicalState::Solid, 8000.0);
    mat.set_thermal(therm);

    auto k_res = mat.evaluate_thermal_conductivity(300.0);
    REQUIRE(k_res.is_ok());
    REQUIRE(std::abs(k_res.value() - 45.0) < 1e-6);

    auto cp_res = mat.evaluate_specific_heat(300.0);
    REQUIRE(cp_res.is_ok());
    REQUIRE(std::abs(cp_res.value() - 500.0) < 1e-6);
}

TEST_CASE("Electrical and electromagnetic properties (MAT-04)") {
    ElectricalProperties elec;
    elec.electrical_conductivity = 5.0e7; // 50 MS/m
    elec.relative_permittivity = 1.0;
    elec.relative_permeability = 100.0;
    elec.dielectric_breakdown_strength = 2.0e7;
    elec.temp_coeff_resistance = 0.004;   // 0.4% / K

    // Effective resistivity = 1 / 5e7 = 2e-8 Ohm*m
    double expected_rho = 2.0e-8;
    REQUIRE(std::abs(elec.effective_resistivity() - expected_rho) < 1e-12);

    Material mat(base::Uuid::generate_random(), "Electrical Conductor", MaterialPhysicalState::Solid, 8900.0);
    mat.set_electrical(elec);

    // Evaluate resistivity at T0 (293.15 K)
    auto r0 = mat.evaluate_electrical_resistivity(293.15);
    REQUIRE(r0.is_ok());
    REQUIRE(std::abs(r0.value() - expected_rho) < 1e-12);

    // Evaluate resistivity at T0 + 50 K (343.15 K): rho = 2e-8 * (1 + 0.004 * 50) = 2e-8 * 1.2 = 2.4e-8
    auto r50 = mat.evaluate_electrical_resistivity(343.15);
    REQUIRE(r50.is_ok());
    REQUIRE(std::abs(r50.value() - 2.4e-8) < 1e-12);
}

TEST_CASE("Fluid and chemical properties (MAT-05)") {
    FluidProperties fl;
    fl.dynamic_viscosity = 1.0e-3;       // 1 mPa*s
    fl.kinematic_viscosity = 1.0e-6;     // 1 mm^2/s
    fl.diffusivity = 1.5e-9;             // m^2/s
    fl.surface_tension = 0.072;          // 72 mN/m
    fl.molecular_weight = 0.018015;      // kg/mol
    fl.chemical_formula = "H2O";

    Material mat(base::Uuid::generate_random(), "Water Sample", MaterialPhysicalState::Liquid, 1000.0);
    mat.set_fluid(fl);

    REQUIRE(mat.fluid().dynamic_viscosity.has_value());
    REQUIRE(std::abs(*mat.fluid().dynamic_viscosity - 1.0e-3) < 1e-9);
    REQUIRE(mat.fluid().chemical_formula == "H2O");
    REQUIRE(std::abs(*mat.fluid().molecular_weight - 0.018015) < 1e-9);
}

TEST_CASE("Material curves and multi-mode interpolation/extrapolation (MAT-07)") {
    MaterialCurve curve("thermal_conductivity", CurveVariable::Temperature, InterpolationMode::Linear, ExtrapolationPolicy::ClampToBoundary);
    curve.add_point(300.0, 50.0);
    curve.add_point(400.0, 45.0);
    curve.add_point(500.0, 38.0);
    curve.add_point(600.0, 30.0);

    REQUIRE(curve.point_count() == 4);

    // Exact point
    auto e300 = curve.evaluate(300.0);
    REQUIRE(e300.is_ok());
    REQUIRE(std::abs(e300.value() - 50.0) < 1e-6);

    // Linear interpolation: mid-point 350 -> (50 + 45)/2 = 47.5
    auto e350 = curve.evaluate(350.0);
    REQUIRE(e350.is_ok());
    REQUIRE(std::abs(e350.value() - 47.5) < 1e-6);

    // Boundary clamp policy
    auto e_low = curve.evaluate(200.0);
    REQUIRE(e_low.is_ok());
    REQUIRE(std::abs(e_low.value() - 50.0) < 1e-6);

    auto e_high = curve.evaluate(800.0);
    REQUIRE(e_high.is_ok());
    REQUIRE(std::abs(e_high.value() - 30.0) < 1e-6);

    // Linear extrapolation policy
    curve.set_extrapolation_policy(ExtrapolationPolicy::LinearExtrapolate);
    // Slope at high end: (30 - 38) / (600 - 500) = -8 / 100 = -0.08 W/(m*K^2)
    // At 700: 30 + (-0.08) * (700 - 600) = 22.0
    auto e_extrap = curve.evaluate(700.0);
    REQUIRE(e_extrap.is_ok());
    REQUIRE(std::abs(e_extrap.value() - 22.0) < 1e-6);

    // Error on extrapolation policy
    curve.set_extrapolation_policy(ExtrapolationPolicy::ErrorOnExtrapolate);
    auto e_err = curve.evaluate(800.0);
    REQUIRE(e_err.is_err());

    // Cubic spline interpolation
    MaterialCurve spline_curve("specific_heat", CurveVariable::Temperature, InterpolationMode::CubicSpline, ExtrapolationPolicy::ClampToBoundary);
    spline_curve.add_point(200.0, 400.0);
    spline_curve.add_point(300.0, 480.0);
    spline_curve.add_point(400.0, 520.0);
    spline_curve.add_point(500.0, 540.0);
    spline_curve.add_point(600.0, 550.0);

    // Test intermediate point
    auto e_spline = spline_curve.evaluate(350.0);
    REQUIRE(e_spline.is_ok());
    REQUIRE(e_spline.value() > 480.0);
    REQUIRE(e_spline.value() < 520.0);

    // Curve JSON roundtrip
    std::string json = curve.to_json();
    auto deserialized = MaterialCurve::from_json(json);
    REQUIRE(deserialized.is_ok());
    REQUIRE(deserialized.value().name() == "thermal_conductivity");
    REQUIRE(deserialized.value().point_count() == 4);

    // Material integration: evaluate property prefers curve over scalar
    Material mat(base::Uuid::generate_random(), "Steel Curve Test", MaterialPhysicalState::Solid, 7850.0);
    ThermalProperties tp;
    tp.thermal_conductivity = 100.0; // scalar default
    mat.set_thermal(tp);

    // Without curve: returns 100.0
    REQUIRE(std::abs(mat.evaluate_thermal_conductivity(350.0).value() - 100.0) < 1e-6);

    // With curve added: returns curve value (47.5)
    curve.set_extrapolation_policy(ExtrapolationPolicy::ClampToBoundary);
    mat.add_curve(curve);
    REQUIRE(std::abs(mat.evaluate_thermal_conductivity(350.0).value() - 47.5) < 1e-6);
}

TEST_CASE("Material assignment and priority overrides (MAT-06)") {
    MaterialRegistry registry;
    auto mat1 = Material::structural_steel();
    auto mat2 = Material::aluminium_6061();
    auto id1 = mat1.id();
    auto id2 = mat2.id();

    REQUIRE(registry.register_material(mat1).is_ok());
    REQUIRE(registry.register_material(mat2).is_ok());
    REQUIRE(registry.material_count() == 2);
    REQUIRE(registry.has_material(id1));
    REQUIRE(registry.has_material("Structural Steel"));

    auto entity_id = base::Uuid::generate_random();

    // Assign mat1 as default material
    REQUIRE(registry.assign_material(entity_id, id1, "default", 0).is_ok());
    const auto* assigned_mat = registry.get_material_for_target(entity_id);
    REQUIRE(assigned_mat != nullptr);
    REQUIRE(assigned_mat->id() == id1);

    // Query unassigned subdomain falls back to default
    const auto* sub_mat = registry.get_material_for_target(entity_id, "boundary_face");
    REQUIRE(sub_mat != nullptr);
    REQUIRE(sub_mat->id() == id1);

    // Assign mat2 specifically to "boundary_face"
    REQUIRE(registry.assign_material(entity_id, id2, "boundary_face", 0).is_ok());
    const auto* sub_mat_explicit = registry.get_material_for_target(entity_id, "boundary_face");
    REQUIRE(sub_mat_explicit != nullptr);
    REQUIRE(sub_mat_explicit->id() == id2);

    // Priority conflict resolution
    // Attempting to override with lower priority should fail
    auto lower_prio_res = registry.assign_material(entity_id, id1, "boundary_face", -1);
    REQUIRE(lower_prio_res.is_err());

    // Higher priority override succeeds
    auto higher_prio_res = registry.assign_material(entity_id, id1, "boundary_face", 10);
    REQUIRE(higher_prio_res.is_ok());
    REQUIRE(registry.get_material_for_target(entity_id, "boundary_face")->id() == id1);

    // Unassign subdomain
    REQUIRE(registry.unassign_material(entity_id, "boundary_face").is_ok());
    // Should now fall back to default
    REQUIRE(registry.get_material_for_target(entity_id, "boundary_face")->id() == id1);

    // Clear all assignments for entity
    registry.clear_assignments_for_target(entity_id);
    REQUIRE(registry.get_material_for_target(entity_id) == nullptr);
}

TEST_CASE("Material validation and preset engineering catalog (MAT-08)") {
    // 1. Nominal Preset Library Validation
    MaterialRegistry registry;
    registry.register_standard_materials();
    REQUIRE(registry.material_count() == 6);

    auto reg_report = registry.validate_registry();
    REQUIRE(!reg_report.has_errors());

    // Check presets exist
    REQUIRE(registry.has_material("Structural Steel"));
    REQUIRE(registry.has_material("Aluminium 6061-T6"));
    REQUIRE(registry.has_material("Copper (Electrolytic Tough Pitch)"));
    REQUIRE(registry.has_material("Silicon (Monocrystalline)"));
    REQUIRE(registry.has_material("Air (Standard Sea Level)"));
    REQUIRE(registry.has_material("Water (Pure Liquid 20 C)"));

    // 2. Anomaly Detection and Bounds Validation
    Material invalid_mat(base::Uuid{}, "", MaterialPhysicalState::Solid, -50.0);
    auto rep = invalid_mat.validate();
    REQUIRE(rep.has_error("MAT_NIL_ID"));
    REQUIRE(rep.has_error("MAT_EMPTY_NAME"));
    REQUIRE(rep.has_error("MAT_INVALID_DENSITY"));

    // Thermodynamic checks
    MechanicalProperties bad_mech;
    bad_mech.youngs_modulus_pa = -100.0;
    bad_mech.poissons_ratio = 0.6; // Thermodynamic stability requires nu < 0.5
    bad_mech.yield_strength_pa = 500e6;
    bad_mech.ultimate_strength_pa = 400e6; // Inversion: ultimate < yield!
    invalid_mat.set_mechanical(bad_mech);

    ThermalProperties bad_therm;
    bad_therm.thermal_conductivity = -10.0;
    bad_therm.emissivity = 1.5; // Emissivity must be <= 1.0
    bad_therm.melting_point_k = -5.0; // Below absolute zero
    invalid_mat.set_thermal(bad_therm);

    ElectricalProperties bad_elec;
    bad_elec.relative_permittivity = 0.5; // Permittivity cannot be < 1.0
    invalid_mat.set_electrical(bad_elec);

    auto rep2 = invalid_mat.validate();
    REQUIRE(rep2.has_error("MAT_INVALID_YOUNGS_MODULUS"));
    REQUIRE(rep2.has_error("MAT_INVALID_POISSON_RATIO"));
    REQUIRE(rep2.has_error("MAT_STRENGTH_INVERSION"));
    REQUIRE(rep2.has_error("MAT_INVALID_THERMAL_CONDUCTIVITY"));
    REQUIRE(rep2.has_error("MAT_INVALID_EMISSIVITY"));
    REQUIRE(rep2.has_error("MAT_BELOW_ABSOLUTE_ZERO"));
    REQUIRE(rep2.has_error("MAT_INVALID_PERMITTIVITY"));

    // 3. Serialization Roundtrip
    auto steel = Material::structural_steel();
    std::string json_str = steel.to_json();
    auto roundtrip_res = Material::from_json(json_str);
    REQUIRE(roundtrip_res.is_ok());

    const auto& rt_mat = roundtrip_res.value();
    REQUIRE(rt_mat.name() == steel.name());
    REQUIRE(std::abs(rt_mat.density() - steel.density()) < 1e-6);
    REQUIRE(rt_mat.mechanical().youngs_modulus_pa.has_value());
    REQUIRE(std::abs(*rt_mat.mechanical().youngs_modulus_pa - 200.0e9) < 1e-3);
    REQUIRE(rt_mat.thermal().thermal_conductivity.has_value());
    REQUIRE(std::abs(*rt_mat.thermal().thermal_conductivity - 50.0) < 1e-3);

    // Registry serialization roundtrip
    std::string reg_json = registry.to_json();
    auto reg_rt = MaterialRegistry::from_json(reg_json);
    REQUIRE(reg_rt.is_ok());
    REQUIRE(reg_rt.value().material_count() == 6);
}
