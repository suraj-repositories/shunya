#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>
#include <shunya/core/domain/material_runtime.hpp>

namespace shunya::core::domain {

// MATRT-09: Solver Domain Typed Properties

struct ThermalDomainProperties {
    double thermal_conductivity{1.0};       // W/(m*K)
    double specific_heat_capacity{1000.0};   // J/(kg*K)
    double density{1000.0};                  // kg/m^3
    double thermal_expansion_coeff{1.0e-5};  // 1/K
    double emissivity{0.8};                  // dimensionless (0..1)
};

struct MechanicalDomainProperties {
    double youngs_modulus{200.0e9};          // Pa
    double poissons_ratio{0.3};              // dimensionless
    double shear_modulus{76.9e9};            // Pa
    double bulk_modulus{166.7e9};            // Pa
    double yield_strength{250.0e6};          // Pa
    double tangent_modulus{2.0e9};           // Pa
    double density{7850.0};                  // kg/m^3
};

struct ElectricalDomainProperties {
    double electrical_conductivity{1.0e7};   // S/m
    double electrical_resistivity{1.0e-7};   // Ohm*m
    double relative_permittivity{1.0};       // dimensionless (>= 1.0)
    double relative_permeability{1.0};       // dimensionless (>= 0.0)
    double loss_tangent{0.0};                // dimensionless
};

struct FluidDomainProperties {
    double dynamic_viscosity{1.0e-3};        // Pa*s
    double kinematic_viscosity{1.0e-6};      // m^2/s
    double density{1000.0};                  // kg/m^3
    double speed_of_sound{1500.0};           // m/s
    double bulk_modulus{2.2e9};              // Pa
};

struct OpticalDomainProperties {
    double refractive_index_real{1.5};       // n
    double refractive_index_imag{0.0};       // kappa
    double absorption_coeff{0.0};            // 1/m
    double reflectivity{0.04};               // R at normal incidence
};

// MATRT-09: Material-to-Domain Solvers Adapter
class MaterialDomainAdapter {
public:
    [[nodiscard]] static base::Result<ThermalDomainProperties> extract_thermal(
        const MaterialDefinition& mat, const MaterialState& state);

    [[nodiscard]] static base::Result<MechanicalDomainProperties> extract_mechanical(
        const MaterialDefinition& mat, const MaterialState& state);

    [[nodiscard]] static base::Result<ElectricalDomainProperties> extract_electrical(
        const MaterialDefinition& mat, const MaterialState& state);

    [[nodiscard]] static base::Result<FluidDomainProperties> extract_fluid(
        const MaterialDefinition& mat, const MaterialState& state);

    [[nodiscard]] static base::Result<OpticalDomainProperties> extract_optical(
        const MaterialDefinition& mat, const MaterialState& state);
};

} // namespace shunya::core::domain
