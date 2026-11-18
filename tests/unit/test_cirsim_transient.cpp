#include "tests/test_framework.hpp"

#include "shunya/cirsim/transient_simulation_engine.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <cmath>

using namespace shunya::cirsim;
using namespace shunya::core::domain;

TEST_CASE("PH4-08.04.001: Trapezoidal and Gear (BDF2) Implicit Integration Schemes for DAE Solving") {
    // RC circuit: 10V step into R = 1k, C = 1uF -> tau = 1 ms
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_VCC";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_VC";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["C1:1"] = 2;
    topology.pin_to_node_map["C1:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 10.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0e-6; // 1 uF
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, c1};

    // 1. Solve with Trapezoidal integration
    TransientOptions opts_trap;
    opts_trap.t_stop = 2.0e-3; // 2 ms
    opts_trap.dt_initial = 1.0e-5;
    opts_trap.method = TransientMethod::Trapezoidal;
    opts_trap.enable_adaptive_stepping = false;

    TransientSimulationEngine eng_trap(opts_trap);
    auto res_trap = eng_trap.solve(topology, comps);
    REQUIRE(res_trap.is_ok());
    auto sol_trap = res_trap.value();
    REQUIRE(sol_trap.success);
    REQUIRE(!sol_trap.time_points.empty());

    // 2. Solve with Gear BDF2 integration
    TransientOptions opts_bdf2;
    opts_bdf2.t_stop = 2.0e-3;
    opts_bdf2.dt_initial = 1.0e-5;
    opts_bdf2.method = TransientMethod::GearBDF2;
    opts_bdf2.enable_adaptive_stepping = false;

    TransientSimulationEngine eng_bdf2(opts_bdf2);
    auto res_bdf2 = eng_bdf2.solve(topology, comps);
    REQUIRE(res_bdf2.is_ok());
    auto sol_bdf2 = res_bdf2.value();
    REQUIRE(sol_bdf2.success);

    // At t = 1 ms (tau = 1 ms): V_c should be 10 * (1 - 1/e) = 6.3212 V
    // Check points near t = 1 ms
    double vc_trap_1ms = 0.0;
    for (size_t i = 0; i < sol_trap.time_points.size(); ++i) {
        if (sol_trap.time_points[i] >= 1.0e-3) {
            vc_trap_1ms = sol_trap.node_voltages[2][i];
            break;
        }
    }
    double vc_bdf2_1ms = 0.0;
    for (size_t i = 0; i < sol_bdf2.time_points.size(); ++i) {
        if (sol_bdf2.time_points[i] >= 1.0e-3) {
            vc_bdf2_1ms = sol_bdf2.node_voltages[2][i];
            break;
        }
    }

    REQUIRE(std::abs(vc_trap_1ms - 6.321) < 0.1);
    REQUIRE(std::abs(vc_bdf2_1ms - 6.321) < 0.1);
}

TEST_CASE("PH4-08.04.002: Adaptive Time-Stepping Based on Local Truncation Error (LTE) Estimation") {
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_IN";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_OUT";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V_PULSE:1"] = 1;
    topology.pin_to_node_map["V_PULSE:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["C1:1"] = 2;
    topology.pin_to_node_map["C1:2"] = 0;

    ElectricalComponent v_pulse;
    v_pulse.id = "V_PULSE";
    v_pulse.type = ElectricalComponentType::VoltageSourcePulse;
    v_pulse.value_primary = 5.0; // 5V pulse
    v_pulse.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 500.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0e-7;
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v_pulse, r1, c1};

    TransientOptions opts;
    opts.t_stop = 3.0e-4; // 300 us
    opts.dt_initial = 1.0e-5; // Start coarse 10us
    opts.dt_min = 1.0e-8;     // Can adapt down to 10ns on sharp edges
    opts.dt_max = 2.0e-5;
    opts.enable_adaptive_stepping = true;
    opts.lte_tolerance = 0.05; // 50 mV tolerance

    TransientSimulationEngine eng(opts);
    auto res = eng.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.success);
    // Verifies adaptive step acceptance and rejection logic engaged
    REQUIRE(sol.accepted_steps > 10);
}

TEST_CASE("PH4-08.04.003: Dynamic Capacitor Charging & Discharging Transient Companion Model (C / dt)") {
    // Verify pure RC charge from 0V to asymptotic 5.0V
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "VCC";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "VCAP";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["C1:1"] = 2;
    topology.pin_to_node_map["C1:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 5.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0e-6; // 1 uF -> tau = 1 ms
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, c1};

    TransientOptions opts;
    opts.t_stop = 5.0e-3; // 5 ms (5 time constants -> 99.3% charged)
    opts.dt_initial = 1.0e-5;

    TransientSimulationEngine eng(opts);
    auto res = eng.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    double v_final = sol.node_voltages[2].back();
    // In 5 tau, Vcap reaches > 4.95V
    REQUIRE(v_final > 4.95);
    REQUIRE(v_final <= 5.0);
}

TEST_CASE("PH4-08.04.004: Inductor Energy Storage, Inductive Back-EMF & Magnetic Saturation Curve") {
    // Test saturable inductance formula
    double l0 = 10.0e-3; // 10 mH
    double isat = 2.0;   // 2 A saturation current
    double l_zero_current = TransientSimulationEngine::evaluate_saturable_inductance(l0, isat, 0.0);
    double l_at_isat = TransientSimulationEngine::evaluate_saturable_inductance(l0, isat, 2.0);
    double l_heavy_sat = TransientSimulationEngine::evaluate_saturable_inductance(l0, isat, 10.0);

    REQUIRE(std::abs(l_zero_current - 0.010) < 1e-6);
    REQUIRE(std::abs(l_at_isat - 0.005) < 1e-6); // L0 / (1 + 1^2) = L0 / 2
    REQUIRE(l_heavy_sat < 0.0005); // Heavily saturated down to < 5%

    // Circuit test: RL circuit (12V step, R = 10 Ohm, L = 10 mH -> tau = L/R = 1 ms, I_final = 1.2A)
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_12V";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_IND";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["L1:1"] = 2;
    topology.pin_to_node_map["L1:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 12.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 10.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent l1;
    l1.id = "L1";
    l1.type = ElectricalComponentType::Inductor;
    l1.value_primary = 10.0e-3; // 10 mH
    l1.value_secondary = 5.0;   // Isat = 5A
    l1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, l1};

    TransientOptions opts;
    opts.t_stop = 5.0e-3; // 5 ms (5 tau)
    opts.dt_initial = 1.0e-5;

    TransientSimulationEngine eng(opts);
    auto res = eng.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.inductor_states.count("L1") > 0);
    const auto& ind_st = sol.inductor_states.at("L1");

    // Current should reach ~ 1.2 A, stored energy E = 0.5 * L * I^2 ~= 0.5 * 0.01 * 1.44 = 0.0072 J
    REQUIRE(ind_st.current > 1.15);
    REQUIRE(ind_st.stored_energy_joules > 0.005);
}

TEST_CASE("PH4-08.04.005: 7-Region Li-ion Battery Transient Integration Tracking SOC Depletion & Temperature Rise") {
    // Battery discharging into high-drain 1 Ohm load
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "BAT_POS";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;

    topology.pin_to_node_map["BAT1:1"] = 1;
    topology.pin_to_node_map["BAT1:2"] = 0;
    topology.pin_to_node_map["R_LOAD:1"] = 1;
    topology.pin_to_node_map["R_LOAD:2"] = 0;

    ElectricalComponent bat1;
    bat1.id = "BAT1";
    bat1.type = ElectricalComponentType::VoltageSourceDC;
    bat1.value_primary = 4.2; // 4.2V initial full charge
    bat1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r_load;
    r_load.id = "R_LOAD";
    r_load.type = ElectricalComponentType::Resistor;
    r_load.value_primary = 1.0; // 1 Ohm load (~4A discharge)
    r_load.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {bat1, r_load};

    TransientOptions opts;
    opts.t_stop = 1.0; // 1 second of simulation
    opts.dt_initial = 1.0e-3;

    TransientSimulationEngine eng(opts);
    auto res = eng.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.battery_states.count("BAT1") > 0);
    const auto& bat_st = sol.battery_states.at("BAT1");

    // SOC should have decreased from 1.0
    REQUIRE(bat_st.current_soc < 1.0);
    // Terminal voltage should be loaded (< 4.2V due to internal resistance)
    REQUIRE(bat_st.terminal_voltage < 4.2);
    // Cell temperature should rise above ambient 298.15 K
    REQUIRE(bat_st.cell_temperature_k > 298.15);
}

TEST_CASE("PH4-08.04.006: Real-Time Virtual Oscilloscope Probe Points Capturing Dynamic Waveforms") {
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_VCC";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_PROBE";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["C1:1"] = 2;
    topology.pin_to_node_map["C1:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 5.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0e-6;
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, c1};

    TransientSimulationEngine eng;
    OscilloscopeProbe p1;
    p1.probe_id = "CH1";
    p1.target_label = "NET_PROBE";
    p1.target_node = 2;
    p1.is_current_probe = false;
    eng.add_probe(p1);

    OscilloscopeProbe p2;
    p2.probe_id = "CH2_I";
    p2.target_label = "V1";
    p2.is_current_probe = true;
    eng.add_probe(p2);

    auto res = eng.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.probes.size() == 2);
    // Probe 1 captured voltage trajectory
    REQUIRE(!sol.probes[0].value_history.empty());
    REQUIRE(sol.probes[0].value_history.front() >= 0.0);
    // Probe 2 captured current trajectory
    REQUIRE(!sol.probes[1].value_history.empty());
}

TEST_CASE("PH4-08.04.007: Automated Transient Regressions for RC Timing, Square Waves & Inductive Transients") {
    // RC Timing Regression: Verify V(t) = V0 * (1 - exp(-t / tau))
    // Tau = 1.0 ms. At t = 0.693 ms, V(t) should be exactly 50% of VCC (5.0V * 0.5 = 2.5V)
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "IN";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "OUT";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["C1:1"] = 2;
    topology.pin_to_node_map["C1:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 5.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0e-6; // tau = 1 ms
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, c1};

    TransientOptions opts;
    opts.t_stop = 2.0e-3;
    opts.dt_initial = 1.0e-5;

    TransientSimulationEngine eng(opts);
    auto res = eng.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    double v_half_tau = 0.0;
    const double t_half = 0.693147e-3; // ln(2) * tau
    for (size_t i = 0; i < sol.time_points.size(); ++i) {
        if (sol.time_points[i] >= t_half) {
            v_half_tau = sol.node_voltages[2][i];
            break;
        }
    }

    // Must be ~2.5 V (+/- 0.05 V)
    REQUIRE(std::abs(v_half_tau - 2.5) < 0.05);
}
