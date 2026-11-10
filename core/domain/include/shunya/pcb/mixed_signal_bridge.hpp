#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/pcb/dac_adc_models.hpp"
#include "shunya/pcb/digital_event_engine.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace shunya::pcb {

struct AnalogToDigitalBridgeSpec {
    std::string analog_node;
    std::string digital_net;
    ThresholdComparatorAdc comparator;
    uint64_t prop_delay_ps{10};
};

struct DigitalToAnalogBridgeSpec {
    std::vector<std::string> digital_bits_lsb_first;
    std::string analog_source_id; // Controlled VoltageSourceDC component in CircuitNetlist
    R2RDacModel dac;
};

struct DeltaSigmaConfig {
    double v_input{0.5};          // Constant analog input voltage (within [-v_ref, +v_ref])
    double v_ref{1.0};            // +/- 1-bit feedback DAC voltage
    double integrator_rc_s{1e-4}; // RC time constant of analog integrator
    double sample_period_s{1e-6}; // Clock period (oversampling clock)
    uint32_t num_cycles{256};     // Number of clock samples
};

struct DeltaSigmaResult {
    std::vector<double> time_points;
    std::vector<double> integrator_voltage;
    std::vector<StdLogic> bitstream;
    double average_digital_voltage{0.0};
    double ones_density{0.0};
};

/**
 * @brief Mixed-signal co-simulation bridge synchronizing MNA continuous-time analog nodes and IEEE 1164 digital nets.
 */
class MixedSignalBridge {
public:
    using DigitalStepHook = std::function<void(uint64_t step_idx, uint64_t time_ps, DigitalEventEngine& digital)>;

    void add_a2d(AnalogToDigitalBridgeSpec spec);
    void add_d2a(DigitalToAnalogBridgeSpec spec);

    /**
     * @brief Runs synchronized mixed-signal transient simulation alternating MNA analog stepping with IEEE 1164 events.
     */
    [[nodiscard]] core::base::Result<core::domain::TransientResult> run_cosimulation(
        core::domain::CircuitNetlist& analog_netlist,
        DigitalEventEngine& digital_engine,
        double t_stop_s,
        double sync_dt_s,
        DigitalStepHook hook = nullptr);

    /**
     * @brief Closed-loop 1st-order Delta-Sigma modulator combining continuous-time MNA RC integrator & clocked comparator.
     */
    [[nodiscard]] static core::base::Result<DeltaSigmaResult> run_delta_sigma_modulator(
        const DeltaSigmaConfig& cfg);

private:
    std::vector<AnalogToDigitalBridgeSpec> a2d_channels_;
    std::vector<DigitalToAnalogBridgeSpec> d2a_channels_;
};

} // namespace shunya::pcb
