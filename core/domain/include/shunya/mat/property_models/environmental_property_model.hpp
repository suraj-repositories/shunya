#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace shunya::mat {

enum class GalvanicRiskLevel : uint8_t {
    Compatible,           // Delta V <= 0.15 V
    ControlledAcceptable, // Delta V <= 0.25 V
    ModerateRisk,         // Delta V <= 0.50 V
    SevereCorrosionHazard // Delta V > 0.50 V
};

[[nodiscard]] constexpr std::string_view galvanic_risk_to_string(GalvanicRiskLevel r) noexcept {
    switch (r) {
    case GalvanicRiskLevel::Compatible:           return "Compatible (Delta V <= 0.15 V)";
    case GalvanicRiskLevel::ControlledAcceptable: return "Controlled / Acceptable (Delta V <= 0.25 V)";
    case GalvanicRiskLevel::ModerateRisk:         return "Moderate Risk (Delta V <= 0.50 V)";
    case GalvanicRiskLevel::SevereCorrosionHazard:return "Severe Galvanic Hazard (Delta V > 0.50 V)";
    }
    return "Unknown";
}

/**
 * @brief Structured Environmental Property Model:
 *        Fickian moisture absorption, UV degradation rate, atmospheric oxidation kinetics,
 *        Pilling-Bedworth passivating oxide criteria, and galvanic series compatibility.
 * (Roadmap task PH4-03.02.005)
 */
struct EnvironmentalPropertyModel {
    static constexpr double GAS_CONSTANT_R = 8.31446261815324;

    // Moisture Absorption (Fickian)
    double saturation_moisture_content_pct{0.0};    // M_inf [% wt] (0% for metals, 1-3% for polyamides)
    double moisture_diffusion_coefficient_m2_s{0.0}; // D [m^2/s]
    double hygroscopic_expansion_coefficient{0.0};   // beta_M [strain / % moisture]

    // UV Radiation Resistance
    double uv_resistance_index{1.0};                // [0..1] (1.0 for metals/ceramics)
    double uv_photodegradation_rate_per_year{0.0};  // [% loss / year in outdoor sunlight]

    // Atmospheric Oxidation Kinetics & Pilling-Bedworth
    double pilling_bedworth_ratio{1.77};             // PBR (Al ~1.28, Cu ~1.77, Fe ~2.1, Mg ~0.81)
    double oxidation_parabolic_rate_kp{1.0e-12};    // k_p [kg^2 / (m^4 * s)]
    double oxidation_activation_energy_j_mol{120000.0}; // E_a [J/mol]

    // Galvanic Series Compatibility
    double standard_reduction_potential_v{+0.34};   // E^0 vs SHE [V] (Cu = +0.34V, Zn = -0.76V, Fe = -0.44V, Au = +1.50V)

    /**
     * @brief Pilling-Bedworth criterion: 1.0 <= PBR <= 2.0 indicates continuous, non-porous,
     *        passivating protective oxide scale.
     */
    [[nodiscard]] bool is_oxide_protective() const noexcept;

    /**
     * @brief Computes hygroscopic swelling strain epsilon_hygro = beta_M * M%.
     */
    [[nodiscard]] double compute_hygroscopic_strain(double moisture_content_pct) const noexcept;

    /**
     * @brief Computes oxide mass gain per unit area (Delta m / A) via Wagner's parabolic law:
     *        (Delta m / A) = sqrt(k_p(T) * t).
     */
    [[nodiscard]] double compute_oxide_mass_gain(double time_seconds, double temperature_k = 293.15) const noexcept;

    /**
     * @brief Evaluates galvanic potential difference Delta V = |E1 - E2| and assigns risk tier.
     */
    [[nodiscard]] static std::pair<double, GalvanicRiskLevel> evaluate_galvanic_coupling(
        double potential_v1, double potential_v2) noexcept;

    [[nodiscard]] std::string to_summary_string() const;
};

} // namespace shunya::mat
