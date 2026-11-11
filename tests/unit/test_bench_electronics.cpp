#include "tests/test_framework.hpp"

#include "shunya/bench/vcd_parser.hpp"
#include "shunya/bench/waveform_comparator.hpp"
#include "shunya/pcb/digital_event_engine.hpp"
#include "shunya/pcb/spice_simulation_engine.hpp"

#include <cmath>
#include <vector>

using namespace shunya::bench;
using namespace shunya::core::domain;
using namespace shunya::pcb;

TEST_CASE("TEST-02-001 Analog SPICE transient & AC benchmarks vs analytical ngspice reference (<0.1% RMS)") {
    // 1. Series RC low-pass step response: R = 1 kOhm, C = 1 uF -> tau = 1 ms, f_c = 159.15 Hz
    CircuitNetlist rc_net;
    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourcePulse;
    v1.waveform.v_initial = 0.0;
    v1.waveform.v_pulsed = 5.0;
    v1.waveform.delay_time_s = 0.0;
    v1.waveform.rise_time_s = 1e-9;
    v1.waveform.pulse_width_s = 10e-3;
    v1.waveform.period_s = 20e-3;
    v1.pins = {
        CircuitPin::make("v1p", "in", ElectricalPinPolarity::Positive, "V1", "+"),
        CircuitPin::make("v1n", "0",  ElectricalPinPolarity::Negative, "V1", "-")
    };
    rc_net.add_component(v1);

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {
        CircuitPin::make("r1a", "in",  ElectricalPinPolarity::PassiveBiDirectional, "R1", "1"),
        CircuitPin::make("r1b", "out", ElectricalPinPolarity::PassiveBiDirectional, "R1", "2")
    };
    rc_net.add_component(r1);

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0e-6;
    c1.pins = {
        CircuitPin::make("c1a", "out", ElectricalPinPolarity::PassiveBiDirectional, "C1", "1"),
        CircuitPin::make("c1b", "0",   ElectricalPinPolarity::PassiveBiDirectional, "C1", "2")
    };
    rc_net.add_component(c1);
    REQUIRE(rc_net.build_node_topology().is_ok());

    SpiceTransientOptions opts;
    opts.t_stop_s = 3e-3;
    opts.dt_s = 2e-6;
    opts.method = SpiceIntegrationMethod::Trapezoidal;
    opts.use_dc_op_initial = false;

    auto tr = SpiceSimulationEngine::solve_transient(rc_net, opts);
    REQUIRE(tr.is_ok());
    const auto* v_out = tr.value().get_voltage_trace("out");
    REQUIRE(v_out != nullptr);
    REQUIRE(v_out->size() == tr.value().time_points.size());

    std::vector<double> expected_v(v_out->size(), 0.0);
    constexpr double tau = 1.0e-3;
    for (size_t i = 0; i < tr.value().time_points.size(); ++i) {
        const double t = tr.value().time_points[i];
        expected_v[i] = 5.0 * (1.0 - std::exp(-t / tau));
    }

    const auto cmp = WaveformComparator::compare(expected_v, *v_out);
    CHECK(cmp.normalized_rms_error < 0.001); // < 0.1% RMS error vs exact SPICE waveform
    CHECK(cmp.cross_correlation > 0.9999);

    // 2. AC small-signal -3 dB cutoff frequency check for f_c = 1 / (2*pi*R*C) = 159.15 Hz
    CircuitNetlist ac_net;
    ElectricalComponent vac;
    vac.id = "V1";
    vac.type = ElectricalComponentType::VoltageSourceAC;
    vac.value_primary = 1.0;
    vac.pins = v1.pins;
    ac_net.add_component(vac);
    ac_net.add_component(r1);
    ac_net.add_component(c1);
    REQUIRE(ac_net.build_node_topology().is_ok());

    SpiceAcOptions ac_opts;
    ac_opts.f_start_hz = 10.0;
    ac_opts.f_stop_hz = 10000.0;
    ac_opts.points_per_decade = 25;
    ac_opts.input_source_id = "V1";
    ac_opts.output_node = "out";
    auto ac_res = SpiceSimulationEngine::solve_ac(ac_net, ac_opts);
    REQUIRE(ac_res.is_ok());

    double fc_meas = 0.0;
    double best_diff = 1e9;
    REQUIRE(!ac_res.value().bode_points.empty());
    for (const auto& pt : ac_res.value().bode_points) {
        const double d = std::abs(pt.magnitude_gain_db + 3.0103);
        if (d < best_diff) {
            best_diff = d;
            fc_meas = pt.frequency_hz;
        }
    }
    CHECK(std::abs(fc_meas - 159.15) / 159.15 < 0.05);
}

TEST_CASE("TEST-02-001 IEEE 1164 DigitalEventEngine 10,000-cycle timing & VCD round-trip verification") {
    DigitalEventEngine sim;
    sim.set_initial_net("clk", StdLogic::Zero, "__tb");

    // Toggle DFF (qbar -> d) with exact 150 ps clk-to-q propagation delay
    DigitalDffElement dff;
    dff.id = "U_TOGGLE";
    dff.clk_net = "clk";
    dff.d_net = "qb";
    dff.q_net = "q";
    dff.qbar_net = "qb";
    dff.clk_to_q_ps = 150;
    dff.stored_q = StdLogic::Zero;
    dff.last_clk = StdLogic::Zero;
    sim.add_dff(dff);

    // Drive 10,000 clean clock cycles (10 ns = 10,000 ps period -> 100 us = 100,000,000 ps)
    constexpr uint64_t kCycles = 10000;
    constexpr uint64_t kHalfPeriodPs = 5000;
    for (uint64_t c = 0; c < kCycles; ++c) {
        const uint64_t t_rise = c * 2 * kHalfPeriodPs + kHalfPeriodPs;
        const uint64_t t_fall = t_rise + kHalfPeriodPs;
        sim.schedule_driver_event(t_rise, "clk", "__tb", StdLogic::One);
        sim.schedule_driver_event(t_fall, "clk", "__tb", StdLogic::Zero);
    }

    sim.run_until(kCycles * 2 * kHalfPeriodPs + 1000);

    const auto& q_wave = sim.waveform("q");
    // Initial sample at t=0 (Zero) plus kCycles toggled samples
    REQUIRE(q_wave.size() == kCycles + 1);

    // Serialize into IEEE VCD format, parse back, and verify every nanosecond transition timestamp
    VcdTraceDocument vcd_out;
    vcd_out.timescale = "1ps";
    vcd_out.signals = {{"!", "q", 1}};
    for (size_t idx = 1; idx < q_wave.size(); ++idx) {
        const auto& s = q_wave[idx];
        vcd_out.transitions.push_back(VcdTransition{
            s.time_ps, "q", (s.value == StdLogic::One) ? '1' : '0'
        });
    }
    const std::string vcd_text = VcdParser::serialize(vcd_out);
    auto vcd_parsed = VcdParser::parse(vcd_text);
    REQUIRE(vcd_parsed.is_ok());
    REQUIRE(vcd_parsed.value().transitions.size() == kCycles);

    for (uint64_t c = 0; c < kCycles; ++c) {
        const uint64_t expected_ps = c * 2 * kHalfPeriodPs + kHalfPeriodPs + 150;
        const char expected_bit = (c % 2 == 0) ? '1' : '0';
        CHECK(vcd_parsed.value().transitions[c].timestamp == expected_ps);
        CHECK(vcd_parsed.value().transitions[c].logic_char == expected_bit);
    }
}
