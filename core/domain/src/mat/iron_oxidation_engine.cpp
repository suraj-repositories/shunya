#include "shunya/mat/iron_oxidation_engine.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

constexpr double BASELINE_CORROSION_RATE_UM_YEAR = 25.0; // Typical ISO 9223 C3 medium atmospheric corrosion (~25 um/yr)
constexpr double T_REF_K = 293.15;

double IronOxidationEngine::compute_instantaneous_corrosion_rate_um_year(
    const AtmosphericExposureEnvironment& env) noexcept {
    // 1. Humidity factor: zero or negligible below critical RH (60%)
    double f_rh = 0.01;
    if (env.relative_humidity_pct > CRITICAL_HUMIDITY_PCT) {
        const double excess = (env.relative_humidity_pct - CRITICAL_HUMIDITY_PCT) / (100.0 - CRITICAL_HUMIDITY_PCT);
        f_rh = std::pow(std::clamp(excess, 0.0, 1.0), 1.3);
    }

    // 2. Temperature Arrhenius factor
    const double t = std::clamp(env.temperature_k, 250.0, 360.0);
    const double exponent = -(ACTIVATION_ENERGY_J_MOL / GAS_CONSTANT_R) * ((1.0 / t) - (1.0 / T_REF_K));
    const double f_t = std::exp(std::clamp(exponent, -5.0, 5.0));

    // 3. Oxygen factor (diffusion-limited cathodic depolarization)
    const double f_o2 = std::clamp(env.oxygen_concentration_pct / 21.0, 0.0, 3.0);

    // 4. Salinity / Chloride acceleration factor
    const double f_salinity = 1.0 + (env.airborne_salinity_mg_m2_day / 50.0);

    return BASELINE_CORROSION_RATE_UM_YEAR * f_rh * f_t * f_o2 * f_salinity;
}

RustAccumulationState IronOxidationEngine::simulate_exposure(
    double initial_thickness_mm,
    double duration_hours,
    const AtmosphericExposureEnvironment& env) noexcept {
    RustAccumulationState state;
    state.exposure_time_hours = std::max(0.0, duration_hours);

    const double rate_um_year = compute_instantaneous_corrosion_rate_um_year(env);
    state.corrosion_rate_um_year = rate_um_year;

    // Atmospheric corrosion follows a power law: d = r * (t / 1yr)^n where n ~ 0.6 to 0.8
    // due to the partial diffusion barrier imposed by the porous rust scale.
    constexpr double POWER_EXPONENT_N = 0.70;
    const double years = duration_hours / 8760.0;
    const double penetrated_depth_um = rate_um_year * std::pow(std::max(0.0, years), POWER_EXPONENT_N);
    state.penetrated_depth_microns = penetrated_depth_um;

    // Converted iron mass per m^2: rho_Fe * d_penetrated
    // [kg/m^3] * [m] = [kg/m^2] -> * 1000 = [g/m^2]
    const double d_meters = penetrated_depth_um * 1e-6;
    state.consumed_iron_mass_g_m2 = FE_DENSITY_KG_M3 * d_meters * 1000.0;
    state.accumulated_rust_mass_g_m2 = state.consumed_iron_mass_g_m2 * RUST_MASS_EXPANSION_RATIO;

    // Rust layer thickness: porous hydrated scale with PBR ~ 2.15
    state.rust_layer_thickness_microns = penetrated_depth_um * PILLING_BEDWORTH_RATIO;

    // Structural capacity loss
    const double initial_um = std::max(1.0, initial_thickness_mm * 1000.0);
    const double remaining_um = std::max(0.0, initial_um - penetrated_depth_um);
    state.effective_thickness_reduction_pct = ((initial_um - remaining_um) / initial_um) * 100.0;
    state.residual_load_capacity_factor = remaining_um / initial_um;

    return state;
}

EpistemicScalar IronOxidationEngine::evaluate_penetration_depth(
    double initial_thickness_mm,
    double duration_hours,
    const AtmosphericExposureEnvironment& env) noexcept {
    auto state = simulate_exposure(initial_thickness_mm, duration_hours, env);

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "ISO 9223 / ASTM G101 Power-Law Atmospheric Corrosion";
    meta.reference_standard = "ISO 9223:2012 Corrosivity Categories";
    meta.relative_uncertainty_pct = 8.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(state.penetrated_depth_microns, meta);
}

} // namespace shunya::mat
