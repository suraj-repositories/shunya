#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>
#include <cmath>
#include <unordered_map>

namespace shunya::core::domain::env {

using Vector3d = base::math::Vector3<double>;

// =============================================================================
// ENV-01: World Coordinate System, Gravity & Simulation Units
// =============================================================================

enum class UpAxis {
    Y_Up,
    Z_Up
};

enum class Handedness {
    RightHanded,
    LeftHanded
};

struct WorldCoordinateSystem {
    UpAxis up_axis = UpAxis::Y_Up;
    Handedness handedness = Handedness::RightHanded;
    double scale_to_meters = 1.0; // 1.0 = meters, 1e-3 = mm, 1e-2 = cm
    Vector3d origin_offset{0.0, 0.0, 0.0};
    Vector3d gravity_vector{0.0, -9.80665, 0.0}; // m/s^2

    [[nodiscard]] double gravity_magnitude() const noexcept {
        return std::sqrt(gravity_vector.x * gravity_vector.x +
                         gravity_vector.y * gravity_vector.y +
                         gravity_vector.z * gravity_vector.z);
    }
};

// =============================================================================
// ENV-02: Environmental Medium Profiles
// =============================================================================

enum class MediumType {
    Vacuum,
    AirAtmosphere,
    WaterFluid,
    ThermalChamber,
    Cryogenic,
    Custom
};

struct EnvironmentalProfile {
    std::string profile_name = "Standard Atmosphere";
    MediumType type = MediumType::AirAtmosphere;
    double ambient_temperature = 293.15; // 20 °C (Kelvin)
    double ambient_pressure = 101325.0;  // 1 atm (Pascals)
    double medium_density = 1.204;       // kg / m^3 (Air at 20 °C)
    double dynamic_viscosity = 1.825e-5; // Pa * s
    double thermal_conductivity = 0.0257; // W / (m * K)
    double relative_humidity = 50.0;     // %RH (0 to 100)
    double dielectric_permittivity = 1.00059;

    [[nodiscard]] double temperature_celsius() const noexcept {
        return ambient_temperature - 273.15;
    }

    [[nodiscard]] double pressure_kpa() const noexcept {
        return ambient_pressure / 1000.0;
    }
};

// =============================================================================
// ENV-03: Environmental Fields (Vector and Scalar)
// =============================================================================

struct SpatialFieldSample {
    double scalar_value = 0.0;
    Vector3d vector_value{0.0, 0.0, 0.0};
};

class EnvironmentalField {
public:
    virtual ~EnvironmentalField() = default;

    [[nodiscard]] virtual std::string field_name() const = 0;
    [[nodiscard]] virtual SpatialFieldSample evaluate_at(const Vector3d& pos, double time = 0.0) const = 0;
};

/// Constant uniform vector field (e.g. uniform gravity or uniform magnetic field)
class UniformVectorField : public EnvironmentalField {
public:
    UniformVectorField(std::string name, Vector3d vec)
        : name_(std::move(name)), value_(vec) {}

    [[nodiscard]] std::string field_name() const override { return name_; }
    [[nodiscard]] SpatialFieldSample evaluate_at(const Vector3d& /*pos*/, double /*time*/ = 0.0) const override {
        return SpatialFieldSample{
            .scalar_value = std::sqrt(value_.x * value_.x + value_.y * value_.y + value_.z * value_.z),
            .vector_value = value_
        };
    }

private:
    std::string name_;
    Vector3d value_;
};

/// Barometric atmospheric pressure field: P(y) = P0 * exp(-M * g * y / (R * T))
class BarometricPressureField : public EnvironmentalField {
public:
    BarometricPressureField(double p0 = 101325.0, double temp = 288.15, double g = 9.80665)
        : p0_(p0), temp_(temp), g_(g) {}

    [[nodiscard]] std::string field_name() const override { return "BarometricPressure"; }
    [[nodiscard]] SpatialFieldSample evaluate_at(const Vector3d& pos, double /*time*/ = 0.0) const override {
        constexpr double M_AIR = 0.0289644; // kg / mol
        constexpr double R_GAS = 8.314462618; // J / (mol * K)
        const double altitude = std::max(0.0, pos.y);
        const double exponent = -(M_AIR * g_ * altitude) / (R_GAS * temp_);
        const double p = p0_ * std::exp(exponent);
        return SpatialFieldSample{.scalar_value = p, .vector_value = {0.0, 0.0, 0.0}};
    }

private:
    double p0_;
    double temp_;
    double g_;
};

// =============================================================================
// ENV-04: Boundary Condition Specifications
// =============================================================================

enum class LabBoundaryType {
    ClampedRigidWall,
    FreeSurface,
    IsothermalWall,
    AdiabaticWall,
    PeriodicBoundary,
    RadiationSink
};

struct LabBoundaryCondition {
    std::string boundary_id;
    std::string target_surface_or_region;
    LabBoundaryType type = LabBoundaryType::ClampedRigidWall;
    double specified_temperature = 293.15; // K (for isothermal)
    double specified_flux = 0.0;           // W/m^2 (for heat flux)
    bool lock_translation_x = true;
    bool lock_translation_y = true;
    bool lock_translation_z = true;
    bool lock_rotation = true;
};

// =============================================================================
// ENV-07: Experiment Fixtures & Test Benches
// =============================================================================

enum class FixtureType {
    RigidClamp,
    OpticalBreadboard,
    VibrationIsolationTable,
    ThermalChamberEnclosure,
    VirtualInstrumentRack
};

struct ExperimentFixture {
    std::string fixture_id;
    std::string name;
    FixtureType type = FixtureType::RigidClamp;
    Vector3d mount_position{0.0, 0.0, 0.0};
    Vector3d grid_spacing{0.025, 0.025, 0.0}; // 25 mm standard optical breadboard grid
    double mass_rating = 100.0;               // kg max load
    double resonance_frequency = 1500.0;      // Hz
    bool is_grounded = true;
};

// =============================================================================
// ENV-08: Environment Validation & Diagnostic Issues
// =============================================================================

struct EnvironmentValidationIssue {
    std::string parameter_name;
    std::string message;
    bool is_fatal = true;
};

// =============================================================================
// ENV-06: Virtual Lab Environment Presets & Manager
// =============================================================================

class VirtualLabEnvironment {
public:
    explicit VirtualLabEnvironment(std::string env_name = "Default Virtual Lab");

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) noexcept { name_ = std::move(name); }

    [[nodiscard]] const WorldCoordinateSystem& coordinates() const noexcept { return coords_; }
    void set_coordinates(const WorldCoordinateSystem& coords) noexcept { coords_ = coords; }

    [[nodiscard]] const EnvironmentalProfile& profile() const noexcept { return profile_; }
    void set_profile(const EnvironmentalProfile& prof) noexcept { profile_ = prof; }

    void add_field(std::shared_ptr<EnvironmentalField> field);
    [[nodiscard]] const std::vector<std::shared_ptr<EnvironmentalField>>& fields() const noexcept { return fields_; }
    [[nodiscard]] std::shared_ptr<EnvironmentalField> get_field(const std::string& name) const;

    void add_boundary_condition(const LabBoundaryCondition& bc);
    [[nodiscard]] const std::vector<LabBoundaryCondition>& boundary_conditions() const noexcept { return boundaries_; }

    void add_fixture(const ExperimentFixture& fix);
    [[nodiscard]] const std::vector<ExperimentFixture>& fixtures() const noexcept { return fixtures_; }

    [[nodiscard]] std::vector<EnvironmentValidationIssue> validate() const;

    // Presets
    [[nodiscard]] static VirtualLabEnvironment create_standard_sea_level();
    [[nodiscard]] static VirtualLabEnvironment create_high_altitude_test(double altitude_meters);
    [[nodiscard]] static VirtualLabEnvironment create_deep_space_vacuum();
    [[nodiscard]] static VirtualLabEnvironment create_thermal_chamber(double min_temp_c, double max_temp_c);
    [[nodiscard]] static VirtualLabEnvironment create_underwater_submerged(double depth_meters);

private:
    std::string name_;
    WorldCoordinateSystem coords_;
    EnvironmentalProfile profile_;
    std::vector<std::shared_ptr<EnvironmentalField>> fields_;
    std::vector<LabBoundaryCondition> boundaries_;
    std::vector<ExperimentFixture> fixtures_;
};

} // namespace shunya::core::domain::env
