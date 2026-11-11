#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::mat {

enum class EpistemicStatus : uint8_t {
    ConfirmedScientific,
    ApproximateSimulation,
    ExperimentalAssumption,
    Unsupported
};

[[nodiscard]] constexpr std::string_view epistemic_status_to_string(EpistemicStatus s) noexcept {
    switch (s) {
    case EpistemicStatus::ConfirmedScientific:    return "Confirmed Scientific";
    case EpistemicStatus::ApproximateSimulation:  return "Approximate Simulation";
    case EpistemicStatus::ExperimentalAssumption: return "Experimental Assumption";
    case EpistemicStatus::Unsupported:            return "Unsupported";
    }
    return "Approximate Simulation";
}

enum class EvaluatedPhase : uint8_t {
    Solid,
    MeltingTwoPhase,
    Liquid,
    BoilingTwoPhase,
    Gas,
    Supercritical,
    Unknown
};

[[nodiscard]] constexpr std::string_view evaluated_phase_to_string(EvaluatedPhase p) noexcept {
    switch (p) {
    case EvaluatedPhase::Solid:           return "Solid";
    case EvaluatedPhase::MeltingTwoPhase: return "Melting (Two-Phase)";
    case EvaluatedPhase::Liquid:          return "Liquid";
    case EvaluatedPhase::BoilingTwoPhase: return "Boiling (Two-Phase)";
    case EvaluatedPhase::Gas:             return "Gas";
    case EvaluatedPhase::Supercritical:   return "Supercritical Fluid";
    case EvaluatedPhase::Unknown:         return "Unknown";
    }
    return "Unknown";
}

template <typename T>
struct EvaluatedProperty {
    std::optional<T> value;
    std::string unit;
    EpistemicStatus epistemic{EpistemicStatus::Unsupported};
    std::string model_note;

    [[nodiscard]] bool available() const noexcept { return value.has_value(); }
};

struct ElementBehaviorReport {
    int atomic_number{0};
    std::string symbol;
    std::string name;
    double temperature_k{298.15};
    double pressure_pa{101325.0};

    // Phase & transitions
    EvaluatedPhase phase{EvaluatedPhase::Unknown};
    double effective_melting_point_k{0.0};
    double effective_boiling_point_k{0.0};
    bool has_melting_point{false};
    bool has_boiling_point{false};
    std::string phase_note;

    // Temperature & pressure dependent physical/transport properties
    EvaluatedProperty<double> density_kg_m3;
    EvaluatedProperty<double> specific_heat_j_kgk;
    EvaluatedProperty<double> thermal_conductivity_w_mk;
    EvaluatedProperty<double> thermal_expansion_1_k;
    EvaluatedProperty<double> thermal_strain_fraction;
    EvaluatedProperty<double> electrical_conductivity_s_m;
    EvaluatedProperty<double> electrical_resistivity_ohm_m;

    bool is_superconducting{false};
    std::string magnetic_state;

    // Chemical reactivity, stability, corrosion, combustion
    std::string reactivity_class;
    double reactivity_index{0.0}; // 0.0 (inert) to 1.0 (extreme)
    bool is_radioactive{false};
    bool oxidation_susceptible{false};
    bool corrosion_susceptible{false};
    bool combustion_risk{false};
    bool stable_at_conditions{true};

    std::vector<std::string> warnings;
    std::vector<std::string> unsupported_properties;
};

/**
 * @brief Evaluates natural physical and chemical behavior of elements from ElementDatabase
 *        without fabricating missing scientific parameters.
 */
class ElementBehaviorEngine {
public:
    [[nodiscard]] static ElementBehaviorReport evaluate(
        const matdb::ElementRecord& elem,
        double temperature_k,
        double pressure_pa = 101325.0,
        bool oxidizing_atmosphere = true);

    [[nodiscard]] static core::base::Result<ElementBehaviorReport> evaluate_by_symbol(
        std::string_view symbol,
        double temperature_k,
        double pressure_pa = 101325.0,
        bool oxidizing_atmosphere = true);

    // Standard reduction potential (V vs SHE) for key engineering metals where validated; nullopt if unknown
    [[nodiscard]] static std::optional<double> standard_reduction_potential_v(int atomic_number) noexcept;
};

} // namespace shunya::mat
