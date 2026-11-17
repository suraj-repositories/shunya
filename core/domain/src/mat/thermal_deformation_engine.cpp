#include "shunya/mat/thermal_deformation_engine.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

ThermalDeformationState ThermalDeformationEngine::compute_thermal_deformation(
    const eng::PhysicalObject& object,
    const MaterialPropertyBundle& material,
    double target_temperature_k,
    const ConstraintBoundaryCondition& boundary) noexcept {
    ThermalDeformationState state;
    state.operating_temperature_k = target_temperature_k;
    state.delta_t_k = target_temperature_k - REFERENCE_TEMPERATURE_K;

    const double alpha = material.thermal.linear_thermal_expansion_1_k > 0.0
                             ? material.thermal.linear_thermal_expansion_1_k
                             : 16.5e-6; // Default Cu if unset

    // Thermal strain epsilon_th = alpha * delta_T
    const double eps = alpha * state.delta_t_k;
    state.thermal_strain = {eps, eps, eps};

    // Free dimensional changes (modified by constraint: constrained axis cannot freely expand)
    const double fx = 1.0 + (1.0 - std::clamp(boundary.constraint_x, 0.0, 1.0)) * eps;
    const double fy = 1.0 + (1.0 - std::clamp(boundary.constraint_y, 0.0, 1.0)) * eps;
    const double fz = 1.0 + (1.0 - std::clamp(boundary.constraint_z, 0.0, 1.0)) * eps;

    state.deformed_dimensions_m.x = object.geometry.dim_x_m * std::max(0.01, fx);
    state.deformed_dimensions_m.y = object.geometry.dim_y_m * std::max(0.01, fy);
    state.deformed_dimensions_m.z = object.geometry.dim_z_m * std::max(0.01, fz);

    state.initial_volume_m3 = object.geometry.compute_volume_m3();
    const double beta_v = 3.0 * alpha;
    state.deformed_volume_m3 = state.initial_volume_m3 * (1.0 + beta_v * state.delta_t_k);

    // Constrained thermal stress: sigma_i = -eta_i * E * eps_th
    const double e_mod = material.mechanical.young_modulus_pa;
    state.thermal_stress_pa.x = -boundary.constraint_x * e_mod * eps;
    state.thermal_stress_pa.y = -boundary.constraint_y * e_mod * eps;
    state.thermal_stress_pa.z = -boundary.constraint_z * e_mod * eps;

    // Von Mises equivalent thermal stress
    const double s_x = state.thermal_stress_pa.x;
    const double s_y = state.thermal_stress_pa.y;
    const double s_z = state.thermal_stress_pa.z;
    state.von_mises_thermal_stress_pa = std::sqrt(
        0.5 * ((s_x - s_y) * (s_x - s_y) + (s_y - s_z) * (s_y - s_z) + (s_z - s_x) * (s_z - s_x)));

    const double yield = material.mechanical.yield_strength_pa;
    if (yield > 0.0 && state.von_mises_thermal_stress_pa > 0.0) {
        state.thermal_safety_factor = yield / state.von_mises_thermal_stress_pa;
        state.exceeds_yield_strength = (state.von_mises_thermal_stress_pa >= yield);
    } else {
        state.thermal_safety_factor = 99.0;
        state.exceeds_yield_strength = false;
    }

    return state;
}

eng::PhysicalObject ThermalDeformationEngine::update_geometry_with_thermal_expansion(
    const eng::PhysicalObject& object,
    const MaterialPropertyBundle& material,
    double target_temperature_k) noexcept {
    eng::PhysicalObject updated = object;
    updated.operating_temp_k = target_temperature_k;

    auto def_state = compute_thermal_deformation(object, material, target_temperature_k, {});
    updated.geometry.dim_x_m = def_state.deformed_dimensions_m.x;
    updated.geometry.dim_y_m = def_state.deformed_dimensions_m.y;
    updated.geometry.dim_z_m = def_state.deformed_dimensions_m.z;
    if (updated.geometry.radius_m > 0.0) {
        const double alpha = material.thermal.linear_thermal_expansion_1_k;
        updated.geometry.radius_m *= (1.0 + alpha * def_state.delta_t_k);
    }

    return updated;
}

EpistemicScalar ThermalDeformationEngine::evaluate_thermal_stress(
    const eng::PhysicalObject& object,
    const MaterialPropertyBundle& material,
    double target_temperature_k,
    const ConstraintBoundaryCondition& boundary) noexcept {
    auto def_state = compute_thermal_deformation(object, material, target_temperature_k, boundary);

    CalculationClassification meta;
    meta.tier = CalculationTier::AnalyticalFormulation;
    meta.source_model_or_formula = "Thermo-Elastic Continuum Stress sigma_th = -eta * E * alpha * DeltaT";
    meta.reference_standard = "Timoshenko Theory of Elasticity";
    meta.relative_uncertainty_pct = 2.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(def_state.von_mises_thermal_stress_pa, meta);
}

} // namespace shunya::mat
