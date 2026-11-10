#include "tests/test_framework.hpp"

#include "shunya/pcb/digital_event_engine.hpp"
#include "shunya/pcb/mixed_signal_bridge.hpp"

#include <cmath>
#include <vector>

using namespace shunya::pcb;
using namespace shunya::core::domain;

TEST_CASE("PCB-03-002-01: IEEE 1164 9-state bus resolution, inertial glitch rejection, and transport delay") {
    using S = StdLogic;

    // 1) Official IEEE 1164 resolution semantics
    CHECK(Ieee1164Resolver::resolve(S::One, S::Zero) == S::X);  // Strong contention
    CHECK(Ieee1164Resolver::resolve(S::Z, S::One) == S::One);   // Tri-state yielded to strong 1
    CHECK(Ieee1164Resolver::resolve(S::H, S::Zero) == S::Zero); // Weak pull-up defeated by strong 0
    CHECK(Ieee1164Resolver::resolve(S::H, S::L) == S::W);       // Weak contention
    CHECK(Ieee1164Resolver::resolve(S::U, S::One) == S::U);     // Uninitialized propagates

    // 2) Inertial delay rejects sub-delay glitches while Transport delay preserves them
    DigitalEventEngine eng;
    eng.set_initial_net("in", S::Zero);
    eng.set_initial_net("out_inertial", S::One, "G_INERT");
    eng.set_initial_net("out_transport", S::One, "G_TRANS");

    eng.add_gate({"G_INERT", GateType::Not, {"in"}, "out_inertial", 200, 200, DelayModel::Inertial});
    eng.add_gate({"G_TRANS", GateType::Not, {"in"}, "out_transport", 200, 200, DelayModel::Transport});

    // Inject 50 ps positive glitch at t=100 ps (shorter than 200 ps inertial delay)
    eng.schedule_external_pulse(100, 50, "in", S::One, S::Zero);
    // Inject clean wide pulse 400 ps at t=600 ps (longer than 200 ps inertial delay)
    eng.schedule_external_pulse(600, 400, "in", S::One, S::Zero);
    eng.run_until(1500);

    // Inertial output must NOT dip at t=300 ps; only at t=800 ps -> 1200 ps
    const auto& wf_in = eng.waveform("out_inertial");
    const auto& wf_tr = eng.waveform("out_transport");
    CHECK(wf_tr.size() > wf_in.size());
    bool glitch_rejected = true;
    for (const auto& s : wf_in) {
        if (s.time_ps < 700 && s.value == S::Zero) glitch_rejected = false;
    }
    CHECK(glitch_rejected);
    CHECK(eng.get_net_value("out_inertial") == S::One);
}

TEST_CASE("PCB-03-002-01: 4-bit Synchronous Binary Counter using DigitalEventEngine DFFs & Gates") {
    DigitalEventEngine eng;
    eng.set_initial_net("clk", StdLogic::Zero);

    // Build 4-bit toggle counter: D0 = !Q0, D1 = Q0 ^ Q1, D2 = (Q0&Q1) ^ Q2, D3 = (Q0&Q1&Q2) ^ Q3
    eng.add_dff({"DFF0", "clk", "d0", "q0", "q0n", "", 20, StdLogic::Zero, StdLogic::Zero});
    eng.add_dff({"DFF1", "clk", "d1", "q1", "", "", 20, StdLogic::Zero, StdLogic::Zero});
    eng.add_dff({"DFF2", "clk", "d2", "q2", "", "", 20, StdLogic::Zero, StdLogic::Zero});
    eng.add_dff({"DFF3", "clk", "d3", "q3", "", "", 20, StdLogic::Zero, StdLogic::Zero});

    eng.add_gate({"G_D0", GateType::Buffer, {"q0n"}, "d0", 10, 10, DelayModel::Transport});
    eng.add_gate({"G_C1", GateType::And, {"q0", "q1"}, "c1", 10, 10, DelayModel::Transport});
    eng.add_gate({"G_C2", GateType::And, {"c1", "q2"}, "c2", 10, 10, DelayModel::Transport});
    eng.add_gate({"G_D1", GateType::Xor, {"q0", "q1"}, "d1", 10, 10, DelayModel::Transport});
    eng.add_gate({"G_D2", GateType::Xor, {"c1", "q2"}, "d2", 10, 10, DelayModel::Transport});
    eng.add_gate({"G_D3", GateType::Xor, {"c2", "q3"}, "d3", 10, 10, DelayModel::Transport});

    // 10 clock cycles (period = 1000 ps): verify count progresses 0 -> 1 -> ... -> 10
    for (int cyc = 1; cyc <= 10; ++cyc) {
        const uint64_t t_rise = static_cast<uint64_t>(cyc) * 1000ULL - 500ULL;
        const uint64_t t_fall = static_cast<uint64_t>(cyc) * 1000ULL;
        eng.schedule_driver_event(t_rise, "clk", "__ext", StdLogic::One);
        eng.schedule_driver_event(t_fall, "clk", "__ext", StdLogic::Zero);
        eng.run_until(t_fall);

        const int q0 = (eng.get_net_value("q0") == StdLogic::One) ? 1 : 0;
        const int q1 = (eng.get_net_value("q1") == StdLogic::One) ? 2 : 0;
        const int q2 = (eng.get_net_value("q2") == StdLogic::One) ? 4 : 0;
        const int q3 = (eng.get_net_value("q3") == StdLogic::One) ? 8 : 0;
        CHECK((q0 + q1 + q2 + q3) == cyc);
    }
}

TEST_CASE("PCB-03-002-02: Mixed-signal sine -> comparator -> counter -> R-2R DAC analog ramp & Delta-Sigma modulator") {
    // 1) Sine wave comparator triggering 4-bit BinaryCounter -> R-2R DAC reproducing analog staircase ramp
    CircuitNetlist net;
    ElectricalComponent vac;
    vac.id = "V_SINE";
    vac.type = ElectricalComponentType::VoltageSourceAC;
    vac.waveform.dc_offset = 2.5;
    vac.waveform.amplitude = 2.5;
    vac.waveform.frequency_hz = 1000.0; // 1 ms period -> 10 rising comparator edges over 10 ms
    vac.pins = {CircuitPin::make("p", "ana_sine"), CircuitPin::make("n", "0")};
    net.add_component(vac);

    ElectricalComponent vdac_src;
    vdac_src.id = "V_DAC";
    vdac_src.type = ElectricalComponentType::VoltageSourceDC;
    vdac_src.value_primary = 0.0;
    vdac_src.pins = {CircuitPin::make("p", "dac_out"), CircuitPin::make("n", "0")};
    net.add_component(vdac_src);

    ElectricalComponent rload;
    rload.id = "R_LOAD";
    rload.type = ElectricalComponentType::Resistor;
    rload.value_primary = 10000.0;
    rload.pins = {CircuitPin::make("1", "dac_out"), CircuitPin::make("2", "0")};
    net.add_component(rload);

    DigitalEventEngine dig;
    dig.set_initial_net("cmp_clk", StdLogic::Zero, "a2d_ana_sine");
    for (int b = 0; b < 4; ++b) {
        dig.set_initial_net("bit" + std::to_string(b), StdLogic::Zero, "cnt");
    }

    MixedSignalBridge bridge;
    bridge.add_a2d({"ana_sine", "cmp_clk", ThresholdComparatorAdc{2.0, 3.0, 0.2}, 5});
    bridge.add_d2a({{"bit0", "bit1", "bit2", "bit3"}, "V_DAC", R2RDacModel{4, 5.0, 0.0, 10000.0}});

    BinaryCounter counter(4);
    auto hook = [&](uint64_t /*step*/, uint64_t /*t_ps*/, DigitalEventEngine& engine) {
        const auto clk = Ieee1164Resolver::to_4state(engine.get_net_value("cmp_clk"));
        const uint64_t prev_c = counter.count();
        counter.tick(clk);
        if (counter.count() != prev_c) {
            const auto bits = counter.count_bits();
            for (int b = 0; b < 4; ++b) {
                engine.schedule_driver_event(
                    engine.current_time_ps() + 5,
                    "bit" + std::to_string(b),
                    "cnt",
                    Ieee1164Resolver::from_4state(bits[static_cast<size_t>(b)]),
                    DelayModel::Transport);
            }
        }
    };

    auto cosim = bridge.run_cosimulation(net, dig, 0.010, 50e-6, hook);
    REQUIRE(cosim.is_ok());
    CHECK(counter.count() == 10);
    const auto* vdac_trace = cosim.value().get_voltage_trace("dac_out");
    REQUIRE(vdac_trace != nullptr);
    // R-2R DAC at count=10 with 4 bits (max 16 steps) and Vref=5.0 V => 5.0 * 10 / 16 = 3.125 V
    CHECK(std::abs(vdac_trace->back() - 3.125) < 1e-4);

    // 2) Closed-loop First-Order Delta-Sigma Modulator with MNA active integrator
    DeltaSigmaConfig ds_cfg;
    ds_cfg.v_input = 0.5;
    ds_cfg.v_ref = 1.0;
    ds_cfg.integrator_rc_s = 1e-4;
    ds_cfg.sample_period_s = 1e-6;
    ds_cfg.num_cycles = 512;

    auto ds = MixedSignalBridge::run_delta_sigma_modulator(ds_cfg);
    REQUIRE(ds.is_ok());
    // Theoretical ones density = (0.5 + 1.0) / 2.0 = 0.75, average decoded = 0.50 V
    CHECK(std::abs(ds.value().ones_density - 0.75) < 0.03);
    CHECK(std::abs(ds.value().average_digital_voltage - 0.50) < 0.06);
}
