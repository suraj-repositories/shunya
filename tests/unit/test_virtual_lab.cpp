#include "tests/test_framework.hpp"
#include "shunya/core/domain/virtual_lab.hpp"

#include <cmath>

using namespace shunya::core::domain::env;
using namespace shunya::core::base::math;

// =============================================================================
// ENV-01: World Coordinate System, Scale & Gravity
// =============================================================================

TEST_CASE("ENV-01: World Coordinate System and Gravity Vector") {
    WorldCoordinateSystem coords;
    CHECK(coords.up_axis == UpAxis::Y_Up);
    CHECK(coords.handedness == Handedness::RightHanded);
    CHECK(coords.scale_to_meters == 1.0);
    CHECK(approx_equal(coords.gravity_magnitude(), 9.80665, 1e-4));

    coords.gravity_vector = {0.0, 0.0, -1.62}; // Moon gravity, Z-up
    coords.up_axis = UpAxis::Z_Up;
    CHECK(coords.up_axis == UpAxis::Z_Up);
    CHECK(approx_equal(coords.gravity_magnitude(), 1.62, 1e-4));
}

// =============================================================================
// ENV-02: Environmental Profiles
// =============================================================================

TEST_CASE("ENV-02: Environmental Medium Profiles and Properties") {
    EnvironmentalProfile air;
    CHECK(air.type == MediumType::AirAtmosphere);
    CHECK(approx_equal(air.temperature_celsius(), 20.0, 0.01));
    CHECK(approx_equal(air.pressure_kpa(), 101.325, 1e-3));
    CHECK(air.medium_density > 1.0);
    CHECK(air.dynamic_viscosity > 1e-6);

    EnvironmentalProfile vacuum;
    vacuum.type = MediumType::Vacuum;
    vacuum.ambient_pressure = 1e-9;
    vacuum.medium_density = 1e-15;
    CHECK(vacuum.type == MediumType::Vacuum);
    CHECK(vacuum.ambient_pressure < 1e-6);
}

// =============================================================================
// ENV-03: Vector and Scalar Spatial Fields
// =============================================================================

TEST_CASE("ENV-03: Environmental Spatial Fields Evaluation") {
    UniformVectorField mag_field("EarthMagneticField", Vector3d{0.0, 50e-6, 0.0}); // 50 uT
    CHECK(mag_field.field_name() == "EarthMagneticField");

    auto sample = mag_field.evaluate_at(Vector3d{10.0, 20.0, 30.0});
    CHECK(approx_equal(sample.scalar_value, 50e-6, 1e-9));
    CHECK(approx_equal(sample.vector_value.y, 50e-6, 1e-9));

    BarometricPressureField pressure_field(101325.0, 288.15, 9.80665);
    auto p_sea_level = pressure_field.evaluate_at(Vector3d{0.0, 0.0, 0.0});
    CHECK(approx_equal(p_sea_level.scalar_value, 101325.0, 1.0));

    // At 5500 m (half standard atmosphere height), pressure should be roughly half (~50 kPa)
    auto p_5500m = pressure_field.evaluate_at(Vector3d{0.0, 5500.0, 0.0});
    CHECK(p_5500m.scalar_value < 60000.0);
    CHECK(p_5500m.scalar_value > 45000.0);
}

// =============================================================================
// ENV-04: Boundary Condition Specifications
// =============================================================================

TEST_CASE("ENV-04: Lab Boundary Conditions and Constraints") {
    LabBoundaryCondition wall_bc;
    wall_bc.boundary_id = "table_surface";
    wall_bc.target_surface_or_region = "Surface_Z0";
    wall_bc.type = LabBoundaryType::ClampedRigidWall;
    wall_bc.lock_translation_x = true;
    wall_bc.lock_translation_y = true;
    wall_bc.lock_translation_z = true;
    wall_bc.lock_rotation = true;

    CHECK(wall_bc.type == LabBoundaryType::ClampedRigidWall);
    CHECK(wall_bc.lock_translation_y);

    LabBoundaryCondition heat_sink_bc;
    heat_sink_bc.boundary_id = "chilled_plate";
    heat_sink_bc.type = LabBoundaryType::IsothermalWall;
    heat_sink_bc.specified_temperature = 278.15; // 5 °C

    CHECK(heat_sink_bc.type == LabBoundaryType::IsothermalWall);
    CHECK(approx_equal(heat_sink_bc.specified_temperature, 278.15));
}

// =============================================================================
// ENV-05: Spatially Varying Fields & Initial Conditions
// =============================================================================

TEST_CASE("ENV-05: Spatially Varying Field Gradients") {
    VirtualLabEnvironment env("FieldTestEnv");
    env.add_field(std::make_shared<BarometricPressureField>(101325.0, 293.15, 9.80665));

    auto field = env.get_field("BarometricPressure");
    REQUIRE(field != nullptr);

    const double p0 = field->evaluate_at(Vector3d{0.0, 0.0, 0.0}).scalar_value;
    const double p100 = field->evaluate_at(Vector3d{0.0, 100.0, 0.0}).scalar_value;
    const double p1000 = field->evaluate_at(Vector3d{0.0, 1000.0, 0.0}).scalar_value;

    CHECK(p0 > p100);
    CHECK(p100 > p1000);
}

// =============================================================================
// ENV-06: Environment Presets
// =============================================================================

TEST_CASE("ENV-06: Standard Virtual Lab Presets") {
    // 1. Sea Level
    auto sea_level = VirtualLabEnvironment::create_standard_sea_level();
    CHECK(sea_level.profile().type == MediumType::AirAtmosphere);
    CHECK(approx_equal(sea_level.profile().ambient_pressure, 101325.0));
    CHECK(sea_level.fixtures().size() == 1u);

    // 2. High Altitude (10,000 meters)
    auto high_alt = VirtualLabEnvironment::create_high_altitude_test(10000.0);
    CHECK(high_alt.profile().ambient_pressure < 35000.0); // Cruising altitude pressure
    CHECK(high_alt.profile().ambient_temperature < 230.0); // Cold upper troposphere

    // 3. Deep Space Vacuum
    auto space = VirtualLabEnvironment::create_deep_space_vacuum();
    CHECK(space.profile().type == MediumType::Vacuum);
    CHECK(approx_equal(space.coordinates().gravity_magnitude(), 0.0));
    CHECK(space.profile().ambient_temperature < 3.0);

    // 4. Underwater Submerged (50m depth)
    auto ocean = VirtualLabEnvironment::create_underwater_submerged(50.0);
    CHECK(ocean.profile().type == MediumType::WaterFluid);
    // At 50m, pressure is ~ 1 atm + 5 atm ~ 600 kPa
    CHECK(ocean.profile().ambient_pressure > 500000.0);
    CHECK(ocean.profile().medium_density > 990.0);
}

// =============================================================================
// ENV-07: Experiment Fixtures & Test Benches
// =============================================================================

TEST_CASE("ENV-07: Experiment Fixtures and Breadboard Tables") {
    VirtualLabEnvironment env("LabFixtureEnv");

    ExperimentFixture bench;
    bench.fixture_id = "optical_table_1";
    bench.name = "Pneumatic Vibration Isolation Table";
    bench.type = FixtureType::VibrationIsolationTable;
    bench.mount_position = {0.0, 0.8, 0.0};
    bench.grid_spacing = {0.025, 0.025, 0.0}; // 25 mm M6 hole pattern
    bench.mass_rating = 500.0;
    bench.resonance_frequency = 1.5; // 1.5 Hz low-frequency isolation

    env.add_fixture(bench);
    CHECK(env.fixtures().size() == 1u);
    CHECK(env.fixtures()[0].type == FixtureType::VibrationIsolationTable);
    CHECK(env.fixtures()[0].grid_spacing.x == 0.025);
}

// =============================================================================
// ENV-08: Environment Validation & Contradiction Detection
// =============================================================================

TEST_CASE("ENV-08: Environment Parameter Validation and Error Detection") {
    VirtualLabEnvironment valid_env = VirtualLabEnvironment::create_standard_sea_level();
    auto valid_issues = valid_env.validate();
    CHECK(valid_issues.empty());

    // Test invalid negative temperature
    VirtualLabEnvironment invalid_env;
    EnvironmentalProfile bad_prof = invalid_env.profile();
    bad_prof.ambient_temperature = -10.0; // Negative Kelvin!
    bad_prof.ambient_pressure = -500.0;   // Negative Pascals!
    bad_prof.medium_density = -1.0;       // Negative density!
    invalid_env.set_profile(bad_prof);

    auto issues = invalid_env.validate();
    CHECK(issues.size() >= 3u);

    bool found_temp_error = false;
    bool found_press_error = false;
    for (const auto& issue : issues) {
        if (issue.parameter_name == "ambient_temperature") found_temp_error = true;
        if (issue.parameter_name == "ambient_pressure") found_press_error = true;
    }
    CHECK(found_temp_error);
    CHECK(found_press_error);

    // Test invalid boundary condition
    LabBoundaryCondition bad_bc;
    bad_bc.type = LabBoundaryType::IsothermalWall;
    bad_bc.target_surface_or_region = "HeaterPlate";
    bad_bc.specified_temperature = -50.0; // Negative Kelvin
    invalid_env.add_boundary_condition(bad_bc);

    auto issues_with_bc = invalid_env.validate();
    bool found_bc_error = false;
    for (const auto& issue : issues_with_bc) {
        if (issue.parameter_name == "specified_temperature") found_bc_error = true;
    }
    CHECK(found_bc_error);
}
