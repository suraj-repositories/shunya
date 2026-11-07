#include "tests/test_framework.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// ELEC-01 & ELEC-02: Schematic Graph, Pin Model & Netlist Generation
// =============================================================================

TEST_CASE("ELEC-01: Schematic Graph and SPICE Netlist Generation") {
    CircuitNetlist netlist;

    ElectricalComponent v1;
    v1.id = "1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 5.0;
    v1.pins = {
        CircuitPin::make("v1_p", "n1"),
        CircuitPin::make("v1_n", "0")
    };
    netlist.add_component(v1);

    ElectricalComponent r1;
    r1.id = "1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {
        CircuitPin::make("r1_1", "n1"),
        CircuitPin::make("r1_2", "n2")
    };
    netlist.add_component(r1);

    ElectricalComponent c1;
    c1.id = "1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1e-6;
    c1.pins = {
        CircuitPin::make("c1_1", "n2"),
        CircuitPin::make("c1_2", "0")
    };
    netlist.add_component(c1);

    REQUIRE(netlist.build_node_topology().is_ok());
    CHECK(netlist.components().size() == 3u);
    CHECK(netlist.node_count() == 3u); // "0", "n1", "n2"

    CHECK(netlist.get_node_index("0") == 0u);
    CHECK(netlist.get_node_index("n1") > 0u);
    CHECK(netlist.get_node_index("n2") > 0u);

    std::string spice = netlist.export_spice_netlist("Test RC Circuit");
    CHECK(spice.find("V1 n1 0 DC 5") != std::string::npos);
    CHECK(spice.find("R1 n1 n2 1000") != std::string::npos);
    CHECK(spice.find("C1 n2 0 1e-06") != std::string::npos);
    CHECK(spice.find(".end") != std::string::npos);
}

TEST_CASE("ELEC-02: Pin Model, Polarities and Electrical Domains") {
    CHECK(pin_polarity_to_string(ElectricalPinPolarity::Anode) == "Anode");
    CHECK(pin_polarity_to_string(ElectricalPinPolarity::Cathode) == "Cathode");
    CHECK(pin_polarity_to_string(ElectricalPinPolarity::Drain) == "Drain");
    CHECK(pin_polarity_to_string(ElectricalPinPolarity::Gate) == "Gate");
    CHECK(pin_polarity_to_string(ElectricalPinPolarity::Source) == "Source");

    CHECK(pin_domain_to_string(PinElectricalDomain::AnalogElectrical) == "AnalogElectrical");
    CHECK(pin_domain_to_string(PinElectricalDomain::DigitalLogic) == "DigitalLogic");
    CHECK(pin_domain_to_string(PinElectricalDomain::PowerGround) == "PowerGround");

    CircuitPin pin;
    pin.pin_id = "p1";
    pin.polarity = ElectricalPinPolarity::Positive;
    pin.domain = PinElectricalDomain::AnalogElectrical;
    pin.connected_net = "vcc";

    CHECK(pin.pin_id == "p1");
    CHECK(pin.polarity == ElectricalPinPolarity::Positive);
    CHECK(pin.connected_net == "vcc");
}

// =============================================================================
// ELEC-03 & ELEC-04: Passive Devices and Waveform Sources
// =============================================================================

TEST_CASE("ELEC-03: Passive Models and Component Types") {
    CHECK(component_type_to_string(ElectricalComponentType::Resistor) == "Resistor");
    CHECK(component_type_to_string(ElectricalComponentType::Capacitor) == "Capacitor");
    CHECK(component_type_to_string(ElectricalComponentType::Inductor) == "Inductor");
    CHECK(component_type_to_string(ElectricalComponentType::Transformer) == "Transformer");

    ElectricalComponent res;
    res.type = ElectricalComponentType::Resistor;
    res.value_primary = 4700.0;
    CHECK(res.value_primary == 4700.0);

    ElectricalComponent cap;
    cap.type = ElectricalComponentType::Capacitor;
    cap.value_primary = 100e-9;
    CHECK(cap.value_primary == 100e-9);
}

TEST_CASE("ELEC-04: Independent and Dependent Sources, Waveform Evaluation") {
    WaveformParams sine_wave;
    sine_wave.dc_offset = 2.0;
    sine_wave.amplitude = 3.0;
    sine_wave.frequency_hz = 1000.0;
    sine_wave.phase_deg = 0.0;

    // At t = 0: 2.0 + 3.0 * sin(0) = 2.0
    CHECK(std::abs(sine_wave.evaluate_sine(0.0) - 2.0) < 1e-6);
    // At t = 0.25 ms (quarter period for 1kHz): sin(pi/2) = 1.0 -> 2.0 + 3.0 = 5.0
    CHECK(std::abs(sine_wave.evaluate_sine(0.25e-3) - 5.0) < 1e-4);

    WaveformParams pulse_wave;
    pulse_wave.v_initial = 0.0;
    pulse_wave.v_pulsed = 5.0;
    pulse_wave.delay_time_s = 1e-4;
    pulse_wave.rise_time_s = 1e-6;
    pulse_wave.pulse_width_s = 1e-3;
    pulse_wave.period_s = 2e-3;

    // Before delay
    CHECK(pulse_wave.evaluate_pulse(0.0) == 0.0);
    // During pulse on-time (t = 0.5 ms)
    CHECK(pulse_wave.evaluate_pulse(0.5e-3) == 5.0);
}

// =============================================================================
// ELEC-05 & ELEC-13: Ground Checks, Floating Nodes & Circuit Diagnostics
// =============================================================================

TEST_CASE("ELEC-05 & ELEC-13: Ground Validation, Floating Nodes, Source Loops") {
    // 1. Missing ground detection
    CircuitNetlist no_ground_circuit;
    ElectricalComponent r1;
    r1.id = "R1";
    r1.pins = {
        CircuitPin::make("p1", "n1"),
        CircuitPin::make("p2", "n2")
    };
    no_ground_circuit.add_component(r1);
    REQUIRE(no_ground_circuit.build_node_topology().is_ok());

    CHECK_FALSE(CircuitDiagnostics::has_valid_ground(no_ground_circuit));
    auto issues1 = CircuitDiagnostics::inspect_circuit(no_ground_circuit);
    bool found_no_ground = false;
    for (const auto& is : issues1) {
        if (is.code == "ELEC_NO_GROUND") found_no_ground = true;
    }
    CHECK(found_no_ground);

    // 2. Floating node detection
    CircuitNetlist floating_circuit;
    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.pins = {
        CircuitPin::make("v1_p", "n1"),
        CircuitPin::make("v1_n", "0")
    };
    floating_circuit.add_component(v1);

    // R dangling on n2
    ElectricalComponent r_dangling;
    r_dangling.id = "R2";
    r_dangling.pins = {
        CircuitPin::make("r2_1", "n1"),
        CircuitPin::make("r2_2", "dangling_node")
    };
    floating_circuit.add_component(r_dangling);
    REQUIRE(floating_circuit.build_node_topology().is_ok());

    auto floating = CircuitDiagnostics::find_floating_nodes(floating_circuit);
    CHECK(floating.size() == 1u);
    CHECK(floating.front() == "dangling_node");

    // 3. Voltage source loop conflict
    CircuitNetlist loop_circuit;
    ElectricalComponent va;
    va.id = "VA";
    va.type = ElectricalComponentType::VoltageSourceDC;
    va.value_primary = 5.0;
    va.pins = {
        CircuitPin::make("va_p", "n1"),
        CircuitPin::make("va_n", "0")
    };

    ElectricalComponent vb;
    vb.id = "VB";
    vb.type = ElectricalComponentType::VoltageSourceDC;
    vb.value_primary = 10.0; // Conflicting voltage in parallel!
    vb.pins = {
        CircuitPin::make("vb_p", "n1"),
        CircuitPin::make("vb_n", "0")
    };
    loop_circuit.add_component(va);
    loop_circuit.add_component(vb);
    REQUIRE(loop_circuit.build_node_topology().is_ok());

    auto loops = CircuitDiagnostics::find_voltage_source_loops(loop_circuit);
    CHECK(loops.size() == 1u);
}

// =============================================================================
// ELEC-06 & ELEC-07: MNA Assembly and DC Operating Point
// =============================================================================

TEST_CASE("ELEC-06 & ELEC-07: MNA System Assembly and DC Operating Point") {
    // Resistive divider: Vin = 12V, R1 = 3k, R2 = 1k -> Vout = 12 * 1 / (3 + 1) = 3V
    CircuitNetlist divider = CircuitBenchmarkFactory::create_voltage_divider(12.0, 3000.0, 1000.0);

    auto mna_res = MnaAssembler::assemble_dc_system(divider);
    REQUIRE(mna_res.is_ok());
    auto sys = mna_res.value();
    CHECK(sys.num_nodes == 2u); // "in", "out"
    CHECK(sys.num_branches == 1u); // branch for V1

    DcOperatingPointSolver solver;
    auto op_res = solver.solve(divider);
    REQUIRE(op_res.is_ok());
    auto op = op_res.value();
    CHECK(op.converged);

    CHECK(std::abs(op.get_voltage("in") - 12.0) < 1e-4);
    CHECK(std::abs(op.get_voltage("out") - 3.0) < 1e-4);

    // Current through R1 and R2 is 12V / 4k = 3 mA
    CHECK(std::abs(op.get_current("R1") - 0.003) < 1e-5);
    CHECK(std::abs(op.get_current("R2") - 0.003) < 1e-5);

    // Power dissipated in R2: P = I^2 * R = (0.003)^2 * 1000 = 9 mW
    CHECK(std::abs(op.get_power("R2") - 0.009) < 1e-5);
}

// =============================================================================
// ELEC-08: Transient Analysis
// =============================================================================

TEST_CASE("ELEC-08: Transient Analysis of RC and RL Circuits") {
    // RC Filter: Vin = 5V step, R = 1k, C = 1uF -> tau = R*C = 1 ms
    CircuitNetlist rc = CircuitBenchmarkFactory::create_rc_filter(5.0, 1000.0, 1e-6);

    TransientSolver solver;
    solver.t_stop = 3e-3; // 3 ms = 3 time constants (expect 1 - e^-3 = 95% = 4.75V)
    solver.time_step = 1e-5;
    solver.use_dc_op_initial_state = false; // Start from uncharged capacitor

    auto tran_res = solver.solve(rc);
    REQUIRE(tran_res.is_ok());
    auto tran = tran_res.value();
    CHECK(tran.success);
    CHECK(tran.time_points.size() > 100u);

    auto v_out = tran.get_voltage_trace("out");
    REQUIRE(v_out != nullptr);
    CHECK(v_out->front() == 0.0); // Initially 0V

    // At tau = 1 ms (index around 100), Vc should be ~5 * (1 - e^-1) = 5 * 0.63212 = 3.16V
    size_t tau_idx = static_cast<size_t>(1e-3 / 1e-5);
    double v_tau = (*v_out)[tau_idx];
    CHECK(std::abs(v_tau - 3.16) < 0.1);

    // Near 3 ms, Vc should approach ~4.75V
    double v_end = v_out->back();
    CHECK(std::abs(v_end - 4.75) < 0.15);
}

// =============================================================================
// ELEC-09 & ELEC-10: Diode and Transistor Non-linear Models
// =============================================================================

TEST_CASE("ELEC-09: Diode Shockley Model and Non-linear Operating Point") {
    double id = 0.0, gd = 0.0;
    // Reverse bias: negligible current
    NonLinearDeviceState::evaluate_diode(-1.0, 1e-14, 0.02585, 1.0, id, gd);
    CHECK(id < 0.0);
    CHECK(std::abs(id) < 1e-12);

    // Forward bias at 0.7V: significant current
    NonLinearDeviceState::evaluate_diode(0.7, 1e-14, 0.02585, 1.0, id, gd);
    CHECK(id > 1e-4);
    CHECK(gd > 1e-3);

    // Non-linear diode circuit with series resistor:
    // Vin = 5V, R = 1k, Diode to ground.
    // Diode drop should settle near ~0.65 - 0.72V
    CircuitNetlist d_circuit;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 5.0;
    v1.pins = {
        CircuitPin::make("v1_p", "in"),
        CircuitPin::make("v1_n", "0")
    };
    d_circuit.add_component(v1);

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {
        CircuitPin::make("r1_1", "in"),
        CircuitPin::make("r1_2", "d_node")
    };
    d_circuit.add_component(r1);

    ElectricalComponent d1;
    d1.id = "D1";
    d1.type = ElectricalComponentType::Diode;
    d1.is_saturation_current = 1e-14;
    d1.pins = {
        CircuitPin::make("d1_a", "d_node", ElectricalPinPolarity::Anode),
        CircuitPin::make("d1_k", "0", ElectricalPinPolarity::Cathode)
    };
    d_circuit.add_component(d1);

    REQUIRE(d_circuit.build_node_topology().is_ok());

    DcOperatingPointSolver solver;
    auto res = solver.solve(d_circuit);
    REQUIRE(res.is_ok());
    CHECK(res.value().converged);

    double vd = res.value().get_voltage("d_node");
    CHECK(vd > 0.6);
    CHECK(vd < 0.8);
}

TEST_CASE("ELEC-10: MOSFET and BJT Transistor Models") {
    // NMOS Cutoff
    double ids = 0.0, gm = 0.0, gds = 0.0;
    NonLinearDeviceState::evaluate_mosfet_nmos(0.5, 5.0, 1.0, 1e-3, 0.0, ids, gm, gds);
    CHECK(ids == 0.0);
    CHECK(gm == 0.0);

    // NMOS Saturation (Vgs = 3V, Vth = 1V -> Vgst = 2V; Vds = 5V > Vgst)
    NonLinearDeviceState::evaluate_mosfet_nmos(3.0, 5.0, 1.0, 1e-3, 0.0, ids, gm, gds);
    // Ids = 0.5 * Kp * Vgst^2 = 0.5 * 1e-3 * 4 = 2 mA
    CHECK(std::abs(ids - 2e-3) < 1e-5);
    CHECK(gm > 0.0);

    // NMOS Inverter circuit benchmark
    // Vdd = 5V, Rload = 2k, Vin = 0V (MOS cut off -> Vout = 5V)
    CircuitNetlist inv_off = CircuitBenchmarkFactory::create_nmos_inverter(5.0, 2000.0, 0.0);
    DcOperatingPointSolver solver;
    auto res_off = solver.solve(inv_off);
    REQUIRE(res_off.is_ok());
    CHECK(std::abs(res_off.value().get_voltage("vout") - 5.0) < 1e-4);

    // When Vin = 3V (MOS conductive -> Vout pulled down)
    CircuitNetlist inv_on = CircuitBenchmarkFactory::create_nmos_inverter(5.0, 2000.0, 3.0);
    auto res_on = solver.solve(inv_on);
    REQUIRE(res_on.is_ok());
    CHECK(res_on.value().get_voltage("vout") < 2.0);
}

// =============================================================================
// ELEC-11: AC Small-Signal Frequency Response Analysis
// =============================================================================

TEST_CASE("ELEC-11: AC Small-Signal Analysis and Cutoff Frequency") {
    // Low-Pass RC filter: R = 1k, C = 159.15 nF -> fc = 1 / (2 * pi * R * C) = 1000 Hz
    double r = 1000.0;
    constexpr double PI_VAL = 3.14159265358979323846;
    double c = 1.0 / (2.0 * PI_VAL * 1000.0 * r); // ~159.15 nF
    CircuitNetlist rc = CircuitBenchmarkFactory::create_rc_filter(1.0, r, c);

    DcOperatingPointSolver dc_solver;
    auto dc_res = dc_solver.solve(rc);
    REQUIRE(dc_res.is_ok());

    AcSmallSignalSolver ac_solver;
    ac_solver.f_start_hz = 10.0;
    ac_solver.f_stop_hz = 100000.0;
    ac_solver.points_per_decade = 10;
    ac_solver.input_source_id = "V1";
    ac_solver.output_node = "out";

    auto ac_res = ac_solver.solve(rc, dc_res.value());
    REQUIRE(ac_res.is_ok());
    auto ac = ac_res.value();
    CHECK(ac.success);
    CHECK(ac.bode_points.size() > 20u);

    // At low frequencies (e.g. 10 Hz), gain should be near 0 dB
    CHECK(std::abs(ac.bode_points.front().magnitude_gain_db) < 0.2);

    // At high frequencies (100 kHz), gain should be heavily attenuated (< -30 dB)
    CHECK(ac.bode_points.back().magnitude_gain_db < -30.0);

    // Find point closest to 1000 Hz: gain should be ~ -3 dB
    double gain_at_fc = 0.0;
    for (const auto& pt : ac.bode_points) {
        if (std::abs(pt.frequency_hz - 1000.0) < 200.0) {
            gain_at_fc = pt.magnitude_gain_db;
            break;
        }
    }
    CHECK(std::abs(gain_at_fc - (-3.0)) < 0.8);
}

// =============================================================================
// ELEC-12: Circuit Probes and Power Metrics
// =============================================================================

TEST_CASE("ELEC-12: Circuit Probes, RMS, Average, and Energy Calculations") {
    TransientResult dummy_tran;
    dummy_tran.time_points = {0.0, 0.001, 0.002, 0.003, 0.004};
    // 5V DC trace
    dummy_tran.node_voltages["out"] = {5.0, 5.0, 5.0, 5.0, 5.0};
    dummy_tran.node_voltages["in"] = {10.0, 10.0, 10.0, 10.0, 10.0};

    CircuitProbe p_single;
    p_single.id = "p1";
    p_single.type = ProbeType::NodeVoltage;
    p_single.node_pos = "out";
    auto trace_single = ProbeMeasurementEngine::evaluate_probe_trace(p_single, dummy_tran);
    CHECK(trace_single.size() == 5u);
    CHECK(ProbeMeasurementEngine::compute_average_value(trace_single) == 5.0);
    CHECK(ProbeMeasurementEngine::compute_rms_value(trace_single) == 5.0);
    CHECK(ProbeMeasurementEngine::compute_peak_to_peak(trace_single) == 0.0);

    CircuitProbe p_diff;
    p_diff.id = "p2";
    p_diff.type = ProbeType::DifferentialVoltage;
    p_diff.node_pos = "in";
    p_diff.node_neg = "out";
    auto trace_diff = ProbeMeasurementEngine::evaluate_probe_trace(p_diff, dummy_tran);
    CHECK(ProbeMeasurementEngine::compute_average_value(trace_diff) == 5.0);

    // Energy calculation: 10 W for 4 ms -> E = 0.040 Joules
    std::vector<double> power(5, 10.0);
    double energy = ProbeMeasurementEngine::compute_total_energy(dummy_tran.time_points, power);
    CHECK(std::abs(energy - 0.040) < 1e-6);
}

// =============================================================================
// ELEC-14: Comprehensive Circuit Benchmarks
// =============================================================================

TEST_CASE("ELEC-14: Comprehensive Circuit Validation Suite") {
    // Benchmark 1: Symmetrical Voltage Divider (10V -> 5V)
    auto bench_div = CircuitBenchmarkFactory::create_voltage_divider(10.0, 500.0, 500.0);
    DcOperatingPointSolver solver;
    auto res_div = solver.solve(bench_div);
    REQUIRE(res_div.is_ok());
    CHECK(std::abs(res_div.value().get_voltage("out") - 5.0) < 1e-5);

    // Benchmark 2: Rectifier with AC source
    auto bench_rect = CircuitBenchmarkFactory::create_diode_rectifier(10.0, 50.0, 1000.0, 100e-6);
    CHECK(bench_rect.components().size() >= 3u);
    CHECK(CircuitDiagnostics::has_valid_ground(bench_rect));
}
