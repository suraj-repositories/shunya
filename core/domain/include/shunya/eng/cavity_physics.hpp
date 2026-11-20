#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::eng {

enum class CavityKind : uint8_t {
    SealedVacuum,
    FillGas,
    LiquidFill,
    OpenToEnvironment
};

[[nodiscard]] constexpr std::string_view cavity_kind_to_string(CavityKind k) noexcept {
    switch (k) {
    case CavityKind::SealedVacuum:      return "SealedVacuum";
    case CavityKind::FillGas:           return "FillGas";
    case CavityKind::LiquidFill:        return "LiquidFill";
    case CavityKind::OpenToEnvironment: return "OpenToEnvironment";
    }
    return "FillGas";
}

[[nodiscard]] inline core::base::Result<CavityKind> cavity_kind_from_string(std::string_view s) {
    if (s == "SealedVacuum" || s == "Vacuum") return CavityKind::SealedVacuum;
    if (s == "FillGas" || s == "Gas") return CavityKind::FillGas;
    if (s == "LiquidFill" || s == "Liquid") return CavityKind::LiquidFill;
    if (s == "OpenToEnvironment" || s == "Open") return CavityKind::OpenToEnvironment;
    return core::base::Error{.code = 4320, .domain = core::base::ErrorDomain::Validation, .severity = core::base::Severity::Error, .message = "Unknown cavity kind: " + std::string(s), .details = ""};
}

struct GasSpeciesFraction {
    std::string formula;       // e.g. "Ar", "N2", "Kr", "Xe", "He", "H2", "CO2", "Air"
    double mole_fraction{1.0}; // fraction 0.0 .. 1.0 (sum = 1.0)
    double molar_mass_kg_mol{0.039948}; // kg/mol (Argon default)
};

struct CavitySpec {
    CavityKind kind{CavityKind::FillGas};
    double fill_pressure_pa{80000.0};       // 80 kPa for standard bulb
    double fill_temp_k{293.15};             // Fill sealing temp (room temp)
    double cavity_volume_m3{1.5e-4};        // ~150 cm^3 for A19 envelope
    std::vector<GasSpeciesFraction> gas_composition{
        {"Ar", 0.95, 0.039948},
        {"N2", 0.05, 0.028013}
    };
    double residual_vacuum_pressure_pa{1e-5}; // Residual vacuum if kind == SealedVacuum
    double outgassing_rate_pa_m3_s{1e-9};   // Outgassing rate from interior walls
    bool has_chemical_getter{true};         // Getter (e.g. Ba/P) absorbing residual O2/H2O
    double wall_thickness_m{0.0008};        // 0.8 mm glass envelope
    double envelope_radius_m{0.030};        // 30 mm radius
    std::string liquid_fluid_name;          // For LiquidFill (e.g. "Mineral Oil")
};

struct CavityThermodynamicsReport {
    double mole_count{0.0};
    double total_gas_mass_kg{0.0};
    double current_pressure_pa{80000.0};
    double pressure_rise_ratio{1.0};
    double wall_mechanical_stress_pa{0.0};
    double paschen_breakdown_voltage_v{0.0};
    double gas_thermal_conductivity_w_mk{0.0};
    double heat_transfer_gas_w{0.0};
    double sublimation_rate_kg_m2_s{0.0};
    double sublimation_suppression_factor{1.0};
    double predicted_lifespan_hours{1000.0};
    bool is_seal_intact{true};
    std::vector<std::string> seal_warnings;
};

struct IncandescentBulbOperatingPoint {
    double input_voltage_v_rms{120.0};
    double cold_resistance_ohm{9.5};
    double hot_resistance_ohm{240.0};
    double operating_current_a{0.50};
    double nominal_power_w{60.0};
    double filament_temp_k{2750.0};
    double envelope_temp_k{380.0};
    double radiation_power_w{53.0};
    double gas_conduction_loss_w{4.5};
    double lead_loss_w{2.5};
    double luminous_efficacy_lm_w{15.0};
    double total_luminous_flux_lm{900.0};
    double filament_sublimation_suppression_pct{95.0};
    double expected_lifespan_hours{1000.0};
    bool safe_glass_envelope_temp{true}; // true if envelope_temp_k < softening point (820 K)
};

class CavityPhysicsEngine {
public:
    static constexpr double R_GAS = 8.314462618;          // J/(mol*K)
    static constexpr double SIGMA_SB = 5.670374419e-8;    // Stefan-Boltzmann W/(m^2*K^4)
    static constexpr double T_GLASS_SOFTENING_K = 820.0;  // Soda-lime / Borosilicate glass softening

    /**
     * @brief Computes gas moles, pressure at operating temperature, wall mechanical stress,
     *        Paschen arcing voltage, and sublimation suppression.
     */
    [[nodiscard]] static CavityThermodynamicsReport evaluate_cavity(
        const CavitySpec& spec,
        double operating_core_temp_k = 2750.0,
        double ambient_temp_k = 293.15,
        double gap_distance_m = 0.005);

    /**
     * @brief Simulates the coupled electrical-thermal-optical equilibrium of the canonical
     *        A19 Incandescent Light Bulb.
     */
    [[nodiscard]] static IncandescentBulbOperatingPoint evaluate_incandescent_bulb(
        const CavitySpec& cavity,
        double input_voltage_v = 120.0,
        double wire_diameter_m = 0.000045,
        double uncoiled_length_m = 0.58,
        double coiled_length_m = 0.025,
        double ambient_temp_k = 293.15);

    /**
     * @brief Evaluates Paschen's Law breakdown voltage across a gas gap.
     * @param pressure_pa Gas pressure in Pascals
     * @param gap_m Distance in meters
     * @param gas_type "Ar", "N2", "Air", "He", etc.
     */
    [[nodiscard]] static double compute_paschen_breakdown_voltage(
        double pressure_pa,
        double gap_m,
        std::string_view gas_type = "Ar");

    /**
     * @brief Evaluates Tungsten sublimation mass loss rate [kg/(m^2*s)] via Langmuir's equation.
     */
    [[nodiscard]] static double compute_tungsten_sublimation_rate(
        double temp_k,
        double fill_pressure_pa,
        bool has_inert_gas);

    /**
     * @brief Checks seal integrity and gas tightness of cavity boundary.
     */
    [[nodiscard]] static bool verify_seal_integrity(
        const CavitySpec& spec,
        std::vector<std::string>& out_warnings);
};

} // namespace shunya::eng
