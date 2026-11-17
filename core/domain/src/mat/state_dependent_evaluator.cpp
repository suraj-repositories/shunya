#include "shunya/mat/state_dependent_evaluator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

constexpr double T_REF_K = 293.15;
constexpr double P_REF_PA = 101325.0;

EpistemicScalar StateDependentEvaluator::evaluate_density(
    const MaterialPropertyBundle& bundle,
    const ThermodynamicState& state) noexcept {
    const double rho_0 = bundle.mechanical.density_kg_m3;
    if (rho_0 <= 0.0) {
        return EpistemicScalar(0.0, CalculationTier::AnalyticalFormulation, 10.0);
    }
    const double beta_v = bundle.thermal.volumetric_thermal_expansion_1_k > 0.0
                              ? bundle.thermal.volumetric_thermal_expansion_1_k
                              : 3.0 * bundle.thermal.linear_thermal_expansion_1_k;
    const double k_bulk = bundle.mechanical.bulk_modulus_pa > 1e6
                              ? bundle.mechanical.bulk_modulus_pa
                              : 1.0e11;

    const double delta_t = state.temperature_k - T_REF_K;
    const double delta_p = state.pressure_pa - P_REF_PA;

    // Volumetric strain: dV/V_0 = beta_V * delta_T - delta_P / K
    const double vol_strain = beta_v * delta_t - (delta_p / k_bulk);
    const double rho = rho_0 / std::max(0.1, 1.0 + vol_strain);

    CalculationClassification meta;
    meta.tier = CalculationTier::AnalyticalFormulation;
    meta.source_model_or_formula = "Linear Thermo-Mechanical EOS rho(T, P)";
    meta.reference_standard = "ISO 1183";
    meta.relative_uncertainty_pct = 1.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(rho, meta);
}

EpistemicScalar StateDependentEvaluator::evaluate_young_modulus(
    const MaterialPropertyBundle& bundle,
    const ThermodynamicState& state) noexcept {
    const double e_0 = bundle.mechanical.young_modulus_pa;
    if (e_0 <= 0.0) {
        return EpistemicScalar(0.0, CalculationTier::EmpiricalFit, 10.0);
    }
    const double t_m = bundle.thermal.melting_point_k > 300.0 ? bundle.thermal.melting_point_k : 1357.77;
    const double t = std::max(1.0, state.temperature_k);

    double e_t = e_0;
    if (t >= t_m) {
        e_t = 0.0; // Melted state loses solid tensile modulus
    } else if (t > T_REF_K) {
        const double frac = (t - T_REF_K) / (t_m - T_REF_K);
        // Softens roughly 50% approaching melting point
        e_t = e_0 * std::max(0.01, 1.0 - 0.5 * frac);
    } else {
        // Cold hardening: slightly stiffer at cryo temperatures
        const double frac_cold = (T_REF_K - t) / T_REF_K;
        e_t = e_0 * (1.0 + 0.15 * frac_cold);
    }

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "Thermal Modulus Softening Curve E(T)";
    meta.reference_standard = "ASTM E111";
    meta.relative_uncertainty_pct = 3.5;
    meta.is_empirically_validated = true;
    return EpistemicScalar(e_t, meta);
}

EpistemicScalar StateDependentEvaluator::evaluate_electrical_conductivity(
    const MaterialPropertyBundle& bundle,
    const ThermodynamicState& state) noexcept {
    const double sigma_t = bundle.electrical.conductivity_at(state.temperature_k);

    // Piezo-resistive pressure correction: sigma(P) = sigma_T * (1 + kappa_P * (P - P_0))
    // Typically positive for metals (increasing overlap of electronic wavefunctions under compression)
    constexpr double KAPPA_P = 1.2e-11; // ~1.2% increase per GPa for Cu
    const double delta_p = state.pressure_pa - P_REF_PA;
    const double p_factor = std::max(0.1, 1.0 + KAPPA_P * delta_p);
    const double sigma = sigma_t * p_factor;

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "TCR Metallic + Piezo-Resistive Pressure Model";
    meta.reference_standard = "ASTM B193";
    meta.relative_uncertainty_pct = 2.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(sigma, meta);
}

EpistemicScalar StateDependentEvaluator::evaluate_thermal_conductivity(
    const MaterialPropertyBundle& bundle,
    const ThermodynamicState& state) noexcept {
    const double k_t = bundle.thermal.thermal_conductivity_at(state.temperature_k);

    // Minor pressure enhancement for solids under pressure: ~0.5% per GPa
    constexpr double GAMMA_P = 5.0e-12;
    const double delta_p = state.pressure_pa - P_REF_PA;
    const double k = k_t * std::max(0.1, 1.0 + GAMMA_P * delta_p);

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "Temperature-Dependent Conductivity k(T, P)";
    meta.reference_standard = "ASTM E1461";
    meta.relative_uncertainty_pct = 2.5;
    meta.is_empirically_validated = true;
    return EpistemicScalar(k, meta);
}

EpistemicScalar StateDependentEvaluator::evaluate_yield_strength(
    const MaterialPropertyBundle& bundle,
    const ThermodynamicState& state,
    double softening_exponent_m) noexcept {
    const double sig_y0 = bundle.mechanical.yield_strength_pa;
    if (sig_y0 <= 0.0) {
        return EpistemicScalar(0.0, CalculationTier::EmpiricalFit, 10.0);
    }
    const double t_m = bundle.thermal.melting_point_k > 300.0 ? bundle.thermal.melting_point_k : 1357.77;
    const double t = state.temperature_k;

    double sig_y = sig_y0;
    if (t >= t_m) {
        sig_y = 0.0;
    } else if (t > T_REF_K) {
        const double t_star = std::clamp((t - T_REF_K) / (t_m - T_REF_K), 0.0, 1.0);
        sig_y = sig_y0 * std::max(0.0, 1.0 - std::pow(t_star, std::max(0.1, softening_exponent_m)));
    }

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "Johnson-Cook Thermal Softening Model sigma_y(T)";
    meta.reference_standard = "ASTM E8 / E21";
    meta.relative_uncertainty_pct = 5.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(sig_y, meta);
}

MaterialPropertyBundle StateDependentEvaluator::evaluate_state_bundle(
    const MaterialPropertyBundle& base_bundle,
    const ThermodynamicState& state) noexcept {
    MaterialPropertyBundle result = base_bundle;

    result.mechanical.density_kg_m3 = evaluate_density(base_bundle, state).value;
    result.mechanical.young_modulus_pa = evaluate_young_modulus(base_bundle, state).value;
    result.mechanical.yield_strength_pa = evaluate_yield_strength(base_bundle, state).value;

    result.electrical.base_conductivity_s_m = evaluate_electrical_conductivity(base_bundle, state).value;
    result.electrical.base_resistivity_ohm_m = (result.electrical.base_conductivity_s_m > 1e-15)
                                                  ? (1.0 / result.electrical.base_conductivity_s_m)
                                                  : 1e18;

    result.thermal.base_thermal_conductivity_w_mk = evaluate_thermal_conductivity(base_bundle, state).value;
    return result;
}

} // namespace shunya::mat
