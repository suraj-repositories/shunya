#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace shunya::mat {

enum class ElectricalClassification : uint8_t {
    Superconductor,
    Conductor,
    Semiconductor,
    Insulator
};

[[nodiscard]] constexpr std::string_view electrical_classification_to_string(ElectricalClassification c) noexcept {
    switch (c) {
    case ElectricalClassification::Superconductor: return "Superconductor";
    case ElectricalClassification::Conductor:      return "Conductor";
    case ElectricalClassification::Semiconductor:  return "Semiconductor";
    case ElectricalClassification::Insulator:      return "Insulator";
    }
    return "Unknown";
}

/**
 * @brief Structured Electrical Property Model:
 *        Electrical conductivity sigma(T), resistivity rho(T), charge carrier density,
 *        electron mobility, bandgap Eg, dielectric constant epsilon_r, and breakdown field E_break.
 * (Roadmap task PH4-03.02.002)
 */
struct ElectricalPropertyModel {
    static constexpr double ELEMENTARY_CHARGE = 1.602176634e-19; // Coulombs
    static constexpr double BOLTZMANN_K_EV = 8.617333262e-5;    // eV / K

    ElectricalClassification classification{ElectricalClassification::Conductor};
    double base_conductivity_s_m{5.96e7};     // e.g. Copper ~5.96e7 S/m at 293.15 K
    double base_resistivity_ohm_m{1.68e-8};   // 1 / sigma [Ohm * m]
    double tcr_per_k{0.00393};                // Temperature coefficient of resistance [1/K]
    double reference_temp_k{293.15};

    // Microscopic Carrier Dynamics (Drude model)
    double carrier_density_m3{8.49e28};       // n [electrons / m^3]
    double carrier_mobility_m2_vs{0.0044};    // mu_e [m^2 / (V * s)]
    double bandgap_ev{0.0};                   // E_g [eV] (0 for metals, 1.1 for Si, > 4 for insulators)

    // Dielectric & Breakdown
    double relative_permittivity{1.0};        // epsilon_r (dimensionless >= 1.0)
    double breakdown_field_v_m{0.0};          // E_break [V/m]

    [[nodiscard]] double conductivity_at(double temperature_k) const noexcept;
    [[nodiscard]] double resistivity_at(double temperature_k) const noexcept;

    /**
     * @brief Computes theoretical Drude conductivity sigma = n * e * mu_e.
     */
    [[nodiscard]] double compute_drude_conductivity() const noexcept;

    /**
     * @brief Computes dielectric breakdown voltage V_break = E_break * thickness.
     */
    [[nodiscard]] double dielectric_breakdown_voltage(double thickness_m) const noexcept;

    [[nodiscard]] std::string to_summary_string() const;
};

} // namespace shunya::mat
