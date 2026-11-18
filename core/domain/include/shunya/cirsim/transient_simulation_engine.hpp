#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/chem/battery_cell_model.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::cirsim {

enum class TransientMethod {
    BackwardEuler,
    Trapezoidal,
    GearBDF2
};

struct TransientOptions {
    double t_start{0.0};
    double t_stop{1.0e-3};           // 1 ms
    double dt_initial{1.0e-6};        // 1 us
    double dt_min{1.0e-9};            // 1 ns
    double dt_max{1.0e-5};            // 10 us
    TransientMethod method{TransientMethod::Trapezoidal};
    bool enable_adaptive_stepping{true};
    double lte_tolerance{1.0e-4};     // 100 uV local truncation error
    uint32_t max_newton_iterations{50};
};

struct OscilloscopeProbe {
    std::string probe_id;
    std::string target_label; // Node name or branch name
    uint32_t target_node{0};
    bool is_current_probe{false};
    std::vector<double> time_history;
    std::vector<double> value_history;

    void add_sample(double t, double val) {
        time_history.push_back(t);
        value_history.push_back(val);
    }
};

struct BatteryTransientState {
    std::string component_id;
    double current_soc{1.0};
    double terminal_voltage{4.2};
    double cell_temperature_k{298.15};
    double capacity_ah{2.5};
    double internal_resistance_ohms{0.045};
};

struct InductorTransientState {
    std::string component_id;
    double l0{1.0e-3};               // nominal inductance (H)
    double isat{5.0};                // saturation current (A)
    double current{0.0};             // instantaneous current (A)
    double stored_energy_joules{0.0};
    double back_emf_volts{0.0};
};

struct TransientSimulationResult {
    bool success{false};
    std::vector<double> time_points;
    std::unordered_map<uint32_t, std::vector<double>> node_voltages; // [node_idx] -> time trace
    std::unordered_map<std::string, std::vector<double>> branch_currents; // [comp_id] -> time trace
    std::vector<OscilloscopeProbe> probes;
    std::unordered_map<std::string, BatteryTransientState> battery_states;
    std::unordered_map<std::string, InductorTransientState> inductor_states;
    size_t accepted_steps{0};
    size_t rejected_steps{0};
};

/**
 * @brief High-precision Transient Time-Domain Circuit Simulation Engine.
 * Supports Trapezoidal and Gear (BDF2) implicit integration, LTE adaptive time-stepping,
 * dynamic capacitors/inductors with magnetic saturation, 7-region Li-ion battery integration,
 * real-time virtual oscilloscope probes, and 555-timer / PWM regressions.
 * Fulfills roadmap tasks PH4-08.04.001 through PH4-08.04.007.
 */
class TransientSimulationEngine {
public:
    explicit TransientSimulationEngine(TransientOptions options = {})
        : options_(options) {}

    /**
     * @brief Adds an interactive oscilloscope probe point to monitor during simulation.
     */
    void add_probe(const OscilloscopeProbe& probe) {
        probes_.push_back(probe);
    }

    /**
     * @brief Executes the transient time-domain simulation.
     */
    [[nodiscard]] core::base::Result<TransientSimulationResult> solve(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components
    );

    /**
     * @brief Calculates magnetic saturation inductance L(I) = L0 / (1 + (I / Isat)^2).
     */
    [[nodiscard]] static double evaluate_saturable_inductance(
        double l0, double isat, double current
    );

private:
    TransientOptions options_;
    std::vector<OscilloscopeProbe> probes_;
};

} // namespace shunya::cirsim
