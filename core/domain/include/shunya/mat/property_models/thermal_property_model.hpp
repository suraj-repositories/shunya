#pragma once

#include <cmath>
#include <string>
#include <string_view>

namespace shunya::mat {

/**
 * @brief Structured Thermal Property Model:
 *        Temperature-dependent thermal conductivity k(T), specific heat Cp(T),
 *        thermal expansion coefficient alpha(T), melting/boiling points,
 *        heat of fusion, heat of vaporization, and Wiedemann-Franz electronic thermal transport.
 * (Roadmap task PH4-03.02.003)
 */
struct ThermalPropertyModel {
    static constexpr double LORENZ_NUMBER = 2.443551e-8; // Wiedemann-Franz Lorenz number L [W * Ohm / K^2]

    double base_thermal_conductivity_w_mk{401.0};      // e.g. Copper ~401 W/(m*K) at 293.15 K
    double thermal_conductivity_temp_coeff{-0.00025};  // Fractional dk/dT [1/K]
    double reference_temp_k{293.15};

    // Specific Heat Capacity Cp [J/(kg*K)]
    double base_specific_heat_j_kgk{385.0};            // e.g. Copper ~385 J/(kg*K)
    double cp_temp_coeff{0.0003};                      // Fractional dCp/dT [1/K]

    // Thermal Expansion
    double linear_thermal_expansion_1_k{16.5e-6};       // alpha_L [1/K] (e.g. 16.5 ppm/K for Cu)
    double volumetric_thermal_expansion_1_k{49.5e-6};   // beta_V ~ 3 * alpha_L

    // Phase Transitions
    double melting_point_k{1357.77};                    // T_m [K]
    double boiling_point_k{2835.0};                     // T_b [K]
    double heat_of_fusion_j_kg{205000.0};               // Delta H_fus [J/kg]
    double heat_of_vaporization_j_kg{4730000.0};        // Delta H_vap [J/kg]

    [[nodiscard]] double thermal_conductivity_at(double temperature_k) const noexcept;
    [[nodiscard]] double specific_heat_at(double temperature_k) const noexcept;
    [[nodiscard]] double thermal_expansion_at(double temperature_k) const noexcept;

    /**
     * @brief Computes thermal expansion strain delta_L / L_0 over temperature delta:
     *        epsilon_th = alpha_L * (T_end - T_start).
     */
    [[nodiscard]] double compute_thermal_strain(double t_start_k, double t_end_k) const noexcept;

    /**
     * @brief Computes thermal diffusivity alpha = k / (rho * Cp) [m^2 / s].
     */
    [[nodiscard]] double compute_thermal_diffusivity(double density_kg_m3, double temperature_k = 293.15) const noexcept;

    /**
     * @brief Estimates electronic thermal conductivity via Wiedemann-Franz law:
     *        k_e = L * sigma * T.
     */
    [[nodiscard]] static double estimate_electronic_thermal_conductivity(
        double electrical_conductivity_s_m, double temperature_k) noexcept;

    [[nodiscard]] std::string to_summary_string() const;
};

} // namespace shunya::mat
