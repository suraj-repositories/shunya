#pragma once

#include "shunya/mat/epistemic_value.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::mat {

/**
 * @brief NIST Shomate equation polynomial coefficients:
 *        t = T / 1000 (T in Kelvin)
 *        Cp = A + B*t + C*t^2 + D*t^3 + E/t^2 [J / (mol * K)]
 *        H - H_298 = A*t + B*t^2/2 + C*t^3/3 + D*t^4/4 - E/t + F - H [kJ / mol]
 *        S = A*ln(t) + B*t + C*t^2/2 + D*t^3/3 - E/(2*t^2) + G [J / (mol * K)]
 * (Roadmap task PH4-03.03.005)
 */
struct ShomateCoefficients {
    std::string substance_name;
    double t_min_k{298.15};
    double t_max_k{2000.0};
    double molar_mass_g_mol{63.546};

    double A{0.0};
    double B{0.0};
    double C{0.0};
    double D{0.0};
    double E{0.0};
    double F{0.0};
    double G{0.0};
    double H{0.0};

    [[nodiscard]] constexpr bool is_temperature_in_range(double temp_k) const noexcept {
        return temp_k >= (t_min_k - 1.0) && temp_k <= (t_max_k + 1.0);
    }
};

/**
 * @brief Shomate polynomial thermodynamic evaluation engine for high-temperature
 *        heat capacity, enthalpy, and entropy calculations.
 */
class ShomateEngine {
public:
    /**
     * @brief Computes molar isobaric heat capacity Cp_m [J / (mol * K)].
     */
    [[nodiscard]] static EpistemicScalar compute_molar_heat_capacity(
        double temp_k, const ShomateCoefficients& coeff) noexcept;

    /**
     * @brief Computes specific isobaric heat capacity Cp [J / (kg * K)].
     */
    [[nodiscard]] static EpistemicScalar compute_specific_heat_capacity(
        double temp_k, const ShomateCoefficients& coeff) noexcept;

    /**
     * @brief Computes standard molar enthalpy increment H(T) - H(298.15) [kJ / mol].
     */
    [[nodiscard]] static EpistemicScalar compute_molar_enthalpy_increment(
        double temp_k, const ShomateCoefficients& coeff) noexcept;

    /**
     * @brief Computes standard absolute molar entropy S(T) [J / (mol * K)].
     */
    [[nodiscard]] static EpistemicScalar compute_molar_entropy(
        double temp_k, const ShomateCoefficients& coeff) noexcept;

    // Authoritative NIST Standard References
    [[nodiscard]] static ShomateCoefficients copper_solid_nist() noexcept;
    [[nodiscard]] static ShomateCoefficients aluminum_solid_nist() noexcept;
    [[nodiscard]] static ShomateCoefficients iron_solid_nist() noexcept;
};

} // namespace shunya::mat
