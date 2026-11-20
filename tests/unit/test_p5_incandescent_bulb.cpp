#include "tests/test_framework.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/cavity_physics.hpp"
#include "shunya/eng/physical_object.hpp"
#include <cmath>

using namespace shunya;

TEST_CASE("PH5-24.01: Cavity Modeling, Real Gas PVT & Paschen Arcing Voltage") {
    eng::CavitySpec spec;
    spec.kind = eng::CavityKind::FillGas;
    spec.fill_pressure_pa = 80000.0; // 80 kPa
    spec.fill_temp_k = 293.15;
    spec.cavity_volume_m3 = 1.5e-4; // 150 cm^3
    spec.wall_thickness_m = 0.0008; // 0.8 mm
    spec.envelope_radius_m = 0.030; // 30 mm
    spec.gas_composition = {
        {"Ar", 0.95, 0.039948},
        {"N2", 0.05, 0.028013}
    };

    auto rep = eng::CavityPhysicsEngine::evaluate_cavity(spec, 2750.0);

    // Moles: n = PV / (RT) = (80000 * 1.5e-4) / (8.314 * 293.15) ~ 4.92e-3 mol
    CHECK(rep.mole_count > 4.5e-3);
    CHECK(rep.mole_count < 5.5e-3);

    // Total gas mass: ~0.19 grams
    CHECK(rep.total_gas_mass_kg > 0.00015);
    CHECK(rep.total_gas_mass_kg < 0.00025);

    // Operating pressure rises with gas temperature
    CHECK(rep.current_pressure_pa > spec.fill_pressure_pa);
    CHECK(rep.pressure_rise_ratio > 1.2);

    // Wall stress: deltaP * r / (2t) < 30 MPa (safe for glass)
    CHECK(rep.wall_mechanical_stress_pa > 0.0);
    CHECK(rep.wall_mechanical_stress_pa < 30e6);

    // Paschen arcing breakdown voltage across 5mm gap in 80 kPa Argon: > 500 V
    CHECK(rep.paschen_breakdown_voltage_v > 500.0);

    // Seal integrity is intact
    CHECK(rep.is_seal_intact == true);
    CHECK(rep.seal_warnings.empty());
}

TEST_CASE("PH5-24.02: Seal Integrity Checker Rejection & Vacuum Outgassing") {
    // 1. Open cavity fails seal check
    eng::CavitySpec open_spec;
    open_spec.kind = eng::CavityKind::OpenToEnvironment;
    std::vector<std::string> warnings;
    CHECK(eng::CavityPhysicsEngine::verify_seal_integrity(open_spec, warnings) == false);
    CHECK(!warnings.empty());

    // 2. Ultra-thin wall (< 0.2 mm) fails seal check
    eng::CavitySpec thin_spec;
    thin_spec.kind = eng::CavityKind::FillGas;
    thin_spec.wall_thickness_m = 0.00005; // 0.05 mm (unsafe)
    warnings.clear();
    CHECK(eng::CavityPhysicsEngine::verify_seal_integrity(thin_spec, warnings) == false);

    // 3. Vacuum cavity without getter suffers outgassing
    eng::CavitySpec vac_no_getter;
    vac_no_getter.kind = eng::CavityKind::SealedVacuum;
    vac_no_getter.has_chemical_getter = false;
    vac_no_getter.residual_vacuum_pressure_pa = 1e-5;
    vac_no_getter.outgassing_rate_pa_m3_s = 1e-8;
    auto vac_rep = eng::CavityPhysicsEngine::evaluate_cavity(vac_no_getter, 2750.0);
    CHECK(vac_rep.current_pressure_pa > vac_no_getter.residual_vacuum_pressure_pa);
    CHECK(vac_rep.predicted_lifespan_hours <= 50.0);
}

TEST_CASE("PH5-24.03: Incandescent Light Bulb Worked Example (60W A19 at 120V)") {
    eng::CavitySpec bulb_cavity;
    bulb_cavity.kind = eng::CavityKind::FillGas;
    bulb_cavity.fill_pressure_pa = 80000.0;
    bulb_cavity.fill_temp_k = 293.15;
    bulb_cavity.gas_composition = {
        {"Ar", 0.95, 0.039948},
        {"N2", 0.05, 0.028013}
    };

    auto pt = eng::CavityPhysicsEngine::evaluate_incandescent_bulb(bulb_cavity, 120.0);

    // Electrical Operating Point: 120 V, 240 Ohm hot, 0.50 A, 60 W
    CHECK(std::abs(pt.input_voltage_v_rms - 120.0) < 1e-3);
    CHECK(std::abs(pt.hot_resistance_ohm - 240.0) < 1.0);
    CHECK(std::abs(pt.operating_current_a - 0.50) < 0.02);
    CHECK(std::abs(pt.nominal_power_w - 60.0) < 1.0);

    // Thermal Equilibrium:
    // Hot filament: 2750 K
    CHECK(std::abs(pt.filament_temp_k - 2750.0) < 10.0);
    // Outer glass envelope: 380 K (below 820 K softening limit)
    CHECK(pt.envelope_temp_k < 450.0);
    CHECK(pt.safe_glass_envelope_temp == true);

    // Conservation of Energy: P_rad (~53W) + P_gas (~4.5W) + P_lead (~2.5W) = 60W
    CHECK(pt.radiation_power_w > 50.0);
    CHECK(pt.radiation_power_w <= 55.0);
    CHECK(pt.gas_conduction_loss_w > 3.0);
    CHECK(pt.gas_conduction_loss_w < 6.0);
    const double total_dissipated = pt.radiation_power_w + pt.gas_conduction_loss_w + pt.lead_loss_w;
    CHECK(std::abs(total_dissipated - pt.nominal_power_w) < 1.0);

    // Optical Output: ~15 lm/W efficacy, ~900 lumens
    CHECK(pt.luminous_efficacy_lm_w >= 14.0);
    CHECK(pt.total_luminous_flux_lm >= 840.0);
    CHECK(pt.total_luminous_flux_lm <= 960.0);

    // Sublimation Suppression: 95% reduction with 80 kPa Argon, 1000 hours lifespan
    CHECK(pt.filament_sublimation_suppression_pct >= 90.0);
    CHECK(pt.expected_lifespan_hours >= 1000.0);

    // Comparison against vacuum bulb:
    eng::CavitySpec vacuum_cavity;
    vacuum_cavity.kind = eng::CavityKind::SealedVacuum;
    auto vac_pt = eng::CavityPhysicsEngine::evaluate_incandescent_bulb(vacuum_cavity, 120.0);
    CHECK(vac_pt.filament_sublimation_suppression_pct == 0.0);
    CHECK(vac_pt.expected_lifespan_hours <= 100.0); // significantly shorter lifespan
}
