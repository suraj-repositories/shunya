#pragma once

#include "shunya/mat/epistemic_value.hpp"

#include <string>

namespace shunya::mat {

struct AtmosphericExposureEnvironment {
    double relative_humidity_pct{65.0};       // RH [0..100%]
    double temperature_k{293.15};             // T [K]
    double oxygen_concentration_pct{21.0};    // [O2] in air (standard ~21.0%)
    double airborne_salinity_mg_m2_day{0.0};  // Cl- deposition rate
    double sulfur_dioxide_ug_m3{0.0};         // SO2 atmospheric pollutant
};

struct RustAccumulationState {
    double exposure_time_hours{0.0};
    double corrosion_rate_um_year{0.0};
    double penetrated_depth_microns{0.0};
    double consumed_iron_mass_g_m2{0.0};
    double accumulated_rust_mass_g_m2{0.0};
    double rust_layer_thickness_microns{0.0};
    double effective_thickness_reduction_pct{0.0};
    double residual_load_capacity_factor{1.0}; // Residual fraction of original cross-section
};

/**
 * @brief Atmospheric environmental exposure simulation engine modeling iron oxidation
 *        and rust accumulation per ISO 9223 / ASTM G101.
 * (Roadmap task PH4-03.04.001)
 */
class IronOxidationEngine {
public:
    static constexpr double CRITICAL_HUMIDITY_PCT = 60.0;    // Threshold for electrolytic water film
    static constexpr double ACTIVATION_ENERGY_J_MOL = 35000.0; // Fe oxidation Arrhenius barrier
    static constexpr double GAS_CONSTANT_R = 8.314462618;
    static constexpr double FE_DENSITY_KG_M3 = 7874.0;
    static constexpr double RUST_DENSITY_KG_M3 = 3800.0;     // Hydrated FeOOH / Fe2O3.H2O porous rust
    static constexpr double RUST_MASS_EXPANSION_RATIO = 1.60; // 1g Fe yields ~1.60g hydrated rust
    static constexpr double PILLING_BEDWORTH_RATIO = 2.15;   // Rust volume expansion ratio

    /**
     * @brief Computes instantaneous corrosion rate [microns / year] under atmospheric conditions.
     */
    [[nodiscard]] static double compute_instantaneous_corrosion_rate_um_year(
        const AtmosphericExposureEnvironment& env) noexcept;

    /**
     * @brief Simulates time-integrated exposure and rust accumulation for a plate of initial thickness.
     */
    [[nodiscard]] static RustAccumulationState simulate_exposure(
        double initial_thickness_mm,
        double duration_hours,
        const AtmosphericExposureEnvironment& env) noexcept;

    /**
     * @brief Evaluates penetrated oxidation depth as an EpistemicScalar.
     */
    [[nodiscard]] static EpistemicScalar evaluate_penetration_depth(
        double initial_thickness_mm,
        double duration_hours,
        const AtmosphericExposureEnvironment& env) noexcept;
};

} // namespace shunya::mat
