#include "shunya/core/domain/virtual_lab.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::core::domain::env {

VirtualLabEnvironment::VirtualLabEnvironment(std::string env_name)
    : name_(std::move(env_name)) {
    // Default uniform gravity field
    fields_.push_back(std::make_shared<UniformVectorField>("Gravity", coords_.gravity_vector));
}

void VirtualLabEnvironment::add_field(std::shared_ptr<EnvironmentalField> field) {
    if (field) {
        fields_.push_back(std::move(field));
    }
}

std::shared_ptr<EnvironmentalField> VirtualLabEnvironment::get_field(const std::string& name) const {
    for (const auto& f : fields_) {
        if (f->field_name() == name) return f;
    }
    return nullptr;
}

void VirtualLabEnvironment::add_boundary_condition(const LabBoundaryCondition& bc) {
    boundaries_.push_back(bc);
}

void VirtualLabEnvironment::add_fixture(const ExperimentFixture& fix) {
    fixtures_.push_back(fix);
}

std::vector<EnvironmentValidationIssue> VirtualLabEnvironment::validate() const {
    std::vector<EnvironmentValidationIssue> issues;

    // 1. Temperature validation
    if (profile_.ambient_temperature < 0.0) {
        issues.push_back(EnvironmentValidationIssue{
            .parameter_name = "ambient_temperature",
            .message = "Ambient temperature cannot be below absolute zero (0 Kelvin).",
            .is_fatal = true
        });
    }

    // 2. Pressure validation
    if (profile_.ambient_pressure < 0.0) {
        issues.push_back(EnvironmentValidationIssue{
            .parameter_name = "ambient_pressure",
            .message = "Absolute ambient pressure cannot be negative.",
            .is_fatal = true
        });
    }

    // 3. Density validation
    if (profile_.medium_density < 0.0) {
        issues.push_back(EnvironmentValidationIssue{
            .parameter_name = "medium_density",
            .message = "Medium density cannot be negative.",
            .is_fatal = true
        });
    }

    // 4. Gravity validation
    const double g_mag = coords_.gravity_magnitude();
    if (g_mag > 1000.0) {
        issues.push_back(EnvironmentValidationIssue{
            .parameter_name = "gravity_vector",
            .message = "Gravity magnitude exceeds 1000 m/s^2; potential unphysical numerical overflow.",
            .is_fatal = false
        });
    }

    // 5. Boundary conditions validation
    for (const auto& bc : boundaries_) {
        if (bc.type == LabBoundaryType::IsothermalWall && bc.specified_temperature < 0.0) {
            issues.push_back(EnvironmentValidationIssue{
                .parameter_name = "specified_temperature",
                .message = "Isothermal boundary temperature on target '" + bc.target_surface_or_region + "' is below 0 K.",
                .is_fatal = true
            });
        }
    }

    return issues;
}

// =============================================================================
// Presets
// =============================================================================

VirtualLabEnvironment VirtualLabEnvironment::create_standard_sea_level() {
    VirtualLabEnvironment env("Standard Sea-Level Laboratory");
    EnvironmentalProfile p;
    p.profile_name = "ISO Standard Atmosphere 20C";
    p.type = MediumType::AirAtmosphere;
    p.ambient_temperature = 293.15; // 20 °C
    p.ambient_pressure = 101325.0;  // 1 atm
    p.medium_density = 1.204;
    p.dynamic_viscosity = 1.825e-5;
    p.thermal_conductivity = 0.0257;
    p.relative_humidity = 50.0;
    env.set_profile(p);

    ExperimentFixture bench;
    bench.fixture_id = "opt_bench_01";
    bench.name = "Optical Breadboard Table";
    bench.type = FixtureType::OpticalBreadboard;
    bench.grid_spacing = {0.025, 0.025, 0.0};
    bench.mass_rating = 250.0;
    bench.is_grounded = true;
    env.add_fixture(bench);

    return env;
}

VirtualLabEnvironment VirtualLabEnvironment::create_high_altitude_test(double altitude_meters) {
    VirtualLabEnvironment env("High Altitude Flight Simulation");
    const double alt = std::max(0.0, altitude_meters);

    // Standard troposphere temperature lapse: T = 288.15 - 0.0065 * h (down to 11000m)
    const double temp_k = std::max(216.65, 288.15 - 0.0065 * alt);

    // Barometric pressure formula
    constexpr double M_AIR = 0.0289644;
    constexpr double R_GAS = 8.314462618;
    constexpr double G_ACC = 9.80665;
    const double press = 101325.0 * std::exp(-(M_AIR * G_ACC * alt) / (R_GAS * 288.15));

    EnvironmentalProfile p;
    p.profile_name = "High Altitude at " + std::to_string(static_cast<int>(alt)) + "m";
    p.type = MediumType::AirAtmosphere;
    p.ambient_temperature = temp_k;
    p.ambient_pressure = press;
    p.medium_density = (press * M_AIR) / (R_GAS * temp_k);
    env.set_profile(p);

    env.add_field(std::make_shared<BarometricPressureField>(101325.0, temp_k, G_ACC));
    return env;
}

VirtualLabEnvironment VirtualLabEnvironment::create_deep_space_vacuum() {
    VirtualLabEnvironment env("Deep Space Vacuum Environment");
    WorldCoordinateSystem coords;
    coords.gravity_vector = {0.0, 0.0, 0.0}; // microgravity
    env.set_coordinates(coords);

    EnvironmentalProfile p;
    p.profile_name = "Deep Space Hard Vacuum";
    p.type = MediumType::Vacuum;
    p.ambient_temperature = 2.725; // CMB temperature ~2.725 K
    p.ambient_pressure = 1e-12;   // Ultra-high vacuum (Pa)
    p.medium_density = 1e-18;
    p.dynamic_viscosity = 0.0;
    p.thermal_conductivity = 0.0;
    p.relative_humidity = 0.0;
    env.set_profile(p);

    LabBoundaryCondition sink;
    sink.boundary_id = "space_rad_sink";
    sink.target_surface_or_region = "FarField";
    sink.type = LabBoundaryType::RadiationSink;
    sink.specified_temperature = 2.725;
    env.add_boundary_condition(sink);

    return env;
}

VirtualLabEnvironment VirtualLabEnvironment::create_thermal_chamber(double min_temp_c, double max_temp_c) {
    VirtualLabEnvironment env("Thermal Environmental Test Chamber");

    EnvironmentalProfile p;
    p.profile_name = "Thermal Chamber [" + std::to_string(static_cast<int>(min_temp_c)) + "C to " +
                     std::to_string(static_cast<int>(max_temp_c)) + "C]";
    p.type = MediumType::ThermalChamber;
    p.ambient_temperature = 273.15 + 0.5 * (min_temp_c + max_temp_c);
    p.ambient_pressure = 101325.0;
    env.set_profile(p);

    ExperimentFixture chamber;
    chamber.fixture_id = "chamber_box";
    chamber.name = "Climatic Chamber Enclosure";
    chamber.type = FixtureType::ThermalChamberEnclosure;
    chamber.mass_rating = 50.0;
    env.add_fixture(chamber);

    return env;
}

VirtualLabEnvironment VirtualLabEnvironment::create_underwater_submerged(double depth_meters) {
    VirtualLabEnvironment env("Submerged Hydrodynamic Environment");
    const double depth = std::max(0.0, depth_meters);

    constexpr double RHO_WATER = 998.2;
    constexpr double G_ACC = 9.80665;
    const double hydrostatic_p = 101325.0 + (RHO_WATER * G_ACC * depth);

    EnvironmentalProfile p;
    p.profile_name = "Freshwater Submerged at " + std::to_string(static_cast<int>(depth)) + "m";
    p.type = MediumType::WaterFluid;
    p.ambient_temperature = 288.15; // 15 °C water
    p.ambient_pressure = hydrostatic_p;
    p.medium_density = RHO_WATER;
    p.dynamic_viscosity = 1.002e-3; // Pa * s (Water at 20 °C)
    p.thermal_conductivity = 0.598; // W / (m * K)
    p.dielectric_permittivity = 80.1; // Water relative permittivity
    env.set_profile(p);

    return env;
}

} // namespace shunya::core::domain::env
