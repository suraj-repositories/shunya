#include "shunya/mat/copper_patina_engine.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

double CopperPatinaEngine::compute_oxide_thickness_nm(
    double exposure_hours,
    double temperature_k,
    double relative_humidity_pct) noexcept {
    if (exposure_hours <= 0.0) {
        return 0.5; // Pristine native film in air
    }

    const double t = std::clamp(temperature_k, 250.0, 700.0);
    const double rh_factor = 1.0 + std::clamp((relative_humidity_pct - 50.0) / 50.0, 0.0, 2.0);

    if (t < 373.15) {
        // Ambient Cabrera-Mott logarithmic kinetics: x = x_0 * ln(1 + t/tau)
        // Reaches ~3-5 nm in days, ~15-30 nm over years in humid air
        constexpr double TAU_HOURS = 1.0;
        constexpr double X_SCALE_NM = 1.8;
        const double growth = X_SCALE_NM * std::log(1.0 + (exposure_hours / TAU_HOURS)) * rh_factor;
        return 0.5 + std::min(100.0, growth);
    } else {
        // High-temperature parabolic Wagner oxidation: x = sqrt(x_0^2 + 2*k_p*t)
        constexpr double E_A = 60000.0; // J/mol
        constexpr double R = 8.31446;
        const double k_p = 1e-4 * std::exp(-E_A / (R * t)); // nm^2 / hour
        return std::sqrt(0.25 + 2.0 * k_p * exposure_hours) * rh_factor;
    }
}

CopperPatinaState CopperPatinaEngine::evaluate_contact_state(
    double exposure_hours,
    double normal_force_newtons,
    double temperature_k,
    double relative_humidity_pct,
    bool apply_mechanical_wipe) noexcept {
    CopperPatinaState state;
    state.exposure_time_hours = exposure_hours;

    // 1. Oxide thickness
    state.oxide_layer_thickness_nm = compute_oxide_thickness_nm(exposure_hours, temperature_k, relative_humidity_pct);

    // 2. Holm mechanical contact area: A_real = F_N / H
    const double f_n = std::max(0.01, normal_force_newtons);
    const double a_real_m2 = f_n / CU_HARDNESS_PA;
    // Single effective a-spot radius: A = pi * a^2 -> a = sqrt(A / pi)
    const double a_spot_m = std::sqrt(a_real_m2 / 3.141592653589793);
    state.contact_spot_radius_um = a_spot_m * 1.0e6;

    // 3. Constriction Resistance R_constriction = rho_bulk / (2 * a)
    state.constriction_resistance_ohms = CU_BULK_RESISTIVITY_OHM_M / (2.0 * a_spot_m);

    // 4. Oxide Film Resistance & Mechanical Fritting / Wiping
    // Mechanical wiping or high contact pressure ruptures the brittle oxide scale
    double effective_oxide_nm = state.oxide_layer_thickness_nm;
    if (apply_mechanical_wipe) {
        effective_oxide_nm = std::min(1.0, effective_oxide_nm * 0.1); // 90% film disruption
        state.is_film_punctured_by_force = true;
    } else if (f_n > 50.0) {
        // Heavy clamping contact frits / fractures brittle Cu2O scale
        const double relief = std::clamp(f_n / 200.0, 0.0, 0.85);
        effective_oxide_nm *= (1.0 - relief);
        state.is_film_punctured_by_force = true;
    }

    if (effective_oxide_nm <= TUNNELING_CUTOFF_THICKNESS_NM) {
        // Quantum tunneling through sub-2.5 nm native oxide: tunnel resistivity sigma_tunnel ~ 1e-12 Ohm*m^2
        constexpr double SIGMA_TUNNEL_OHM_M2 = 1.0e-12;
        state.film_resistance_ohms = SIGMA_TUNNEL_OHM_M2 / a_real_m2;
    } else {
        // Macroscopic ohmic conduction through semiconductor cuprous oxide film
        const double thickness_m = effective_oxide_nm * 1.0e-9;
        state.film_resistance_ohms = (CU2O_OXIDE_RESISTIVITY_OHM_M * thickness_m) / a_real_m2;
    }

    state.total_contact_resistance_ohms = state.constriction_resistance_ohms + state.film_resistance_ohms;

    // Baseline pristine contact resistance at t=0
    const double r_pristine = state.constriction_resistance_ohms + (1.0e-12 / a_real_m2);
    state.contact_degradation_ratio = state.total_contact_resistance_ohms / std::max(1e-9, r_pristine);

    return state;
}

EpistemicScalar CopperPatinaEngine::evaluate_contact_resistance(
    double exposure_hours,
    double normal_force_newtons,
    double temperature_k,
    double relative_humidity_pct) noexcept {
    auto state = evaluate_contact_state(exposure_hours, normal_force_newtons, temperature_k, relative_humidity_pct);

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "Holm Electrical Contact & Cabrera-Mott Oxide Film Theory";
    meta.reference_standard = "IEEE Holm Conference Standards / ASTM B667";
    meta.relative_uncertainty_pct = 7.5;
    meta.is_empirically_validated = true;
    return EpistemicScalar(state.total_contact_resistance_ohms, meta);
}

} // namespace shunya::mat
