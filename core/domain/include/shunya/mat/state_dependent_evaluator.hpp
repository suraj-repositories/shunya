#pragma once

#include "shunya/mat/epistemic_value.hpp"
#include "shunya/mat/property_models/material_property_bundle.hpp"

#include <vector>

namespace shunya::mat {

struct ThermodynamicState {
    double temperature_k{293.15};
    double pressure_pa{101325.0}; // Standard 1 atm (101,325 Pa)

    [[nodiscard]] constexpr bool is_standard_ambient() const noexcept {
        return std::abs(temperature_k - 293.15) < 1.0 &&
               std::abs(pressure_pa - 101325.0) < 100.0;
    }
};

/**
 * @brief State-dependent material property evaluation engine:
 *        P_calc = f(T, P, composition)
 *        Models thermo-mechanical equation of state (EOS), thermal softening,
 *        piezo-resistivity, and phase proximity.
 * (Roadmap task PH4-03.03.003)
 */
class StateDependentEvaluator {
public:
    /**
     * @brief Evaluates mass density rho(T, P) [kg/m^3] via volumetric thermal expansion
     *        and isothermal compressibility: rho = rho_0 * [1 - beta_V * (T - T_0) + (P - P_0) / K].
     */
    [[nodiscard]] static EpistemicScalar evaluate_density(
        const MaterialPropertyBundle& bundle,
        const ThermodynamicState& state) noexcept;

    /**
     * @brief Evaluates Young's modulus E(T) [Pa] with thermal softening approaching melting point:
     *        E(T) = E_0 * max(0.01, 1 - 0.5 * (T - T_0) / (T_m - T_0)).
     */
    [[nodiscard]] static EpistemicScalar evaluate_young_modulus(
        const MaterialPropertyBundle& bundle,
        const ThermodynamicState& state) noexcept;

    /**
     * @brief Evaluates electrical conductivity sigma(T, P) [S/m] combining metallic TCR
     *        and piezo-resistive pressure response.
     */
    [[nodiscard]] static EpistemicScalar evaluate_electrical_conductivity(
        const MaterialPropertyBundle& bundle,
        const ThermodynamicState& state) noexcept;

    /**
     * @brief Evaluates thermal conductivity k(T, P) [W/(m*K)].
     */
    [[nodiscard]] static EpistemicScalar evaluate_thermal_conductivity(
        const MaterialPropertyBundle& bundle,
        const ThermodynamicState& state) noexcept;

    /**
     * @brief Evaluates temperature/pressure-softened yield strength sigma_y(T) [Pa]
     *        via Johnson-Cook thermal softening factor: [1 - ((T - T_0)/(T_m - T_0))^m].
     */
    [[nodiscard]] static EpistemicScalar evaluate_yield_strength(
        const MaterialPropertyBundle& bundle,
        const ThermodynamicState& state,
        double softening_exponent_m = 1.0) noexcept;

    /**
     * @brief Evaluates complete MaterialPropertyBundle adjusted to state (T, P).
     */
    [[nodiscard]] static MaterialPropertyBundle evaluate_state_bundle(
        const MaterialPropertyBundle& base_bundle,
        const ThermodynamicState& state) noexcept;
};

} // namespace shunya::mat
