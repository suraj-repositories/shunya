#include "tests/test_framework.hpp"

#include "shunya/pcb/netlist_compiler.hpp"
#include "shunya/pcb/parasitic_extractor.hpp"
#include "shunya/pcb/spice_simulation_engine.hpp"

#include <algorithm>
#include <cmath>

using namespace shunya::pcb;
using namespace shunya::core::domain;

TEST_CASE("PCB-03-001-01: Trace Parasitic Extractor computes 100mm x 0.25mm x 1oz Cu resistance ~0.19 Ohm & RLC") {
    PcbTrackSegment trace;
    trace.net_name = "SIG_CLK";
    trace.layer_index = 0;
    trace.width_mm = 0.25;
    trace.start_mm = {0.0, 0.0};
    trace.end_mm = {100.0, 0.0};

    // 1 oz Cu = 0.035 mm, standard FR4 h = 0.20 mm, er = 4.4, 20 C
    auto ext = ParasiticExtractor::extract_track(trace, 0.035, 0.20, 4.4, 20.0);
    REQUIRE(ext.is_ok());
    const auto& p = ext.value();

    // R = 1.68e-8 * 0.1 / (2.5e-4 * 3.5e-5) = 0.192 Ohm (~0.19 Ohm)
    CHECK(std::abs(p.resistance_ohm - 0.192) < 0.005);
    CHECK(p.inductance_nh > 20.0 && p.inductance_nh < 150.0);
    CHECK(p.capacitance_pf > 1.0 && p.capacitance_pf < 30.0);

    // Via barrel extraction check
    PcbVia via;
    via.net_name = "SIG_CLK";
    via.drill_mm = 0.30;
    via.pad_diameter_mm = 0.60;
    auto via_ext = ParasiticExtractor::extract_via(via, 1.6, 0.025, 4.4, 20.0);
    REQUIRE(via_ext.is_ok());
    CHECK(via_ext.value().resistance_ohm > 0.0005 && via_ext.value().resistance_ohm < 0.01);
    CHECK(via_ext.value().inductance_nh > 0.5 && via_ext.value().inductance_nh < 3.0);
}

TEST_CASE("PCB-03-001-01: NetlistCompiler converts schematic + PCB traces into SPICE netlist with trace parasitics") {
    SchematicDocument doc("FilterBoard");
    SchematicSheet root;
    root.id = "root";
    root.title = "RC Filter";

    SchematicComponent v1;
    v1.ref_des = "V1";
    v1.pins.push_back({"V1", "1", "in", PinElectricalType::PowerOutput, {0.0, 10.0}, false});
    v1.pins.push_back({"V1", "2", "gnd", PinElectricalType::PowerInput, {0.0, 0.0}, false});

    SchematicComponent r1;
    r1.ref_des = "R1";
    r1.pins.push_back({"R1", "1", "a", PinElectricalType::Passive, {10.0, 10.0}, false});
    r1.pins.push_back({"R1", "2", "b", PinElectricalType::Passive, {20.0, 10.0}, false});

    SchematicComponent c1;
    c1.ref_des = "C1";
    c1.pins.push_back({"C1", "1", "a", PinElectricalType::Passive, {20.0, 10.0}, false});
    c1.pins.push_back({"C1", "2", "b", PinElectricalType::Passive, {20.0, 0.0}, false});

    root.components = {v1, r1, c1};
    root.wires = {
        {{0.0, 10.0}, {10.0, 10.0}, ""}, // V1.1 -> R1.1 (VIN)
        {{0.0, 0.0}, {20.0, 0.0}, ""}    // V1.2 -> C1.2 (GND)
    };
    root.labels = {
        {"VIN", false, {5.0, 10.0}},
        {"OUT", false, {20.0, 10.0}},
        {"GND", true, {10.0, 0.0}}
    };
    REQUIRE(doc.add_sheet(root).is_ok());

    std::vector<ComponentSpiceSpec> specs = {
        {"V1", ElectricalComponentType::VoltageSourceDC, 5.0, 0.0, 0.0, {}, {"1", "2"}},
        {"R1", ElectricalComponentType::Resistor, 1000.0, 0.0, 0.0, {}, {"1", "2"}},
        {"C1", ElectricalComponentType::Capacitor, 1e-6, 0.0, 0.0, {}, {"1", "2"}}
    };

    PcbTrackSegment out_trace;
    out_trace.net_name = "OUT";
    out_trace.width_mm = 0.25;
    out_trace.start_mm = {0.0, 0.0};
    out_trace.end_mm = {50.0, 0.0};

    auto stack = BoardStackupManager::standard_4layer_fr4();
    auto compiled = NetlistCompiler::compile(doc, specs, {out_trace}, {}, &stack, true);
    REQUIRE(compiled.is_ok());
    CHECK(compiled.value().spice_deck.find("R_PAR_OUT") != std::string::npos);
    CHECK(compiled.value().spice_deck.find("C_PAR_OUT") != std::string::npos);
    CHECK(compiled.value().net_parasitics.at("OUT").total_resistance_ohm > 0.08);
}

TEST_CASE("PCB-03-001-02: SPICE Trapezoidal and Gear-2 RC transient matches analytical V0*(1-exp(-t/RC)) < 0.05% error") {
    // V0 = 5.0 V DC step, R = 1 kOhm, C = 1 uF => tau = 1 ms
    auto netlist = CircuitBenchmarkFactory::create_rc_filter(5.0, 1000.0, 1e-6);

    for (auto method : {SpiceIntegrationMethod::Trapezoidal, SpiceIntegrationMethod::Gear2}) {
        SpiceTransientOptions opts;
        opts.t_stop_s = 3e-3;
        opts.dt_s = 5e-6; // 200 samples per time constant
        opts.method = method;
        opts.use_dc_op_initial = false;

        auto res = SpiceSimulationEngine::solve_transient(netlist, opts);
        REQUIRE(res.is_ok());
        const auto* vout = res.value().get_voltage_trace("out");
        REQUIRE(vout != nullptr);
        CHECK(vout->size() == res.value().time_points.size());

        // Compare against analytical curve from t = 0.1 ms to 3.0 ms
        const double tau = 1000.0 * 1e-6;
        for (size_t i = 20; i < res.value().time_points.size(); ++i) {
            const double t = res.value().time_points[i];
            const double exact = 5.0 * (1.0 - std::exp(-t / tau));
            const double rel_err = std::abs((*vout)[i] - exact) / exact;
            CHECK(rel_err < 5e-4); // Strictly < 0.05% error
        }
    }
}

TEST_CASE("PCB-03-001-02: SPICE Newton-Raphson full-wave bridge rectifier & AC sweep & LC tank") {
    // 1) Full-wave 4-diode bridge rectifier fed by 10 Vpk, 1000 Hz AC source
    CircuitNetlist bridge;
    {
        ElectricalComponent vac;
        vac.id = "VAC";
        vac.type = ElectricalComponentType::VoltageSourceAC;
        vac.waveform.amplitude = 10.0;
        vac.waveform.frequency_hz = 1000.0;
        vac.pins = {CircuitPin::make("v_p", "ac_p"), CircuitPin::make("v_n", "ac_n")};
        bridge.add_component(vac);

        auto add_diode = [&](std::string id, std::string anode, std::string cathode) {
            ElectricalComponent d;
            d.id = std::move(id);
            d.type = ElectricalComponentType::Diode;
            d.is_saturation_current = 1e-14;
            d.ideality_factor = 1.0;
            d.thermal_voltage = 0.02585;
            d.pins = {CircuitPin::make("a", std::move(anode)), CircuitPin::make("k", std::move(cathode))};
            bridge.add_component(std::move(d));
        };
        add_diode("D1", "ac_p", "dc_out");
        add_diode("D2", "0", "ac_p");
        add_diode("D3", "ac_n", "dc_out");
        add_diode("D4", "0", "ac_n");

        ElectricalComponent rload;
        rload.id = "R_LOAD";
        rload.type = ElectricalComponentType::Resistor;
        rload.value_primary = 1000.0;
        rload.pins = {CircuitPin::make("1", "dc_out"), CircuitPin::make("2", "0")};
        bridge.add_component(rload);

        ElectricalComponent cfilt;
        cfilt.id = "C_FILT";
        cfilt.type = ElectricalComponentType::Capacitor;
        cfilt.value_primary = 20e-6;
        cfilt.pins = {CircuitPin::make("1", "dc_out"), CircuitPin::make("2", "0")};
        bridge.add_component(cfilt);

        REQUIRE(bridge.build_node_topology().is_ok());
    }

    SpiceTransientOptions bopts;
    bopts.t_stop_s = 2.5e-3; // 2.5 cycles at 1 kHz = 5 rectified half-cycles
    bopts.dt_s = 20e-6;
    bopts.method = SpiceIntegrationMethod::Trapezoidal;
    bopts.use_dc_op_initial = true;

    auto bres = SpiceSimulationEngine::solve_transient(bridge, bopts);
    REQUIRE(bres.is_ok());
    const auto* vdc = bres.value().get_voltage_trace("dc_out");
    REQUIRE(vdc != nullptr);
    double v_steady_min = 1e9;
    double v_steady_max = -1e9;
    for (size_t i = vdc->size() / 2; i < vdc->size(); ++i) {
        v_steady_min = std::min(v_steady_min, (*vdc)[i]);
        v_steady_max = std::max(v_steady_max, (*vdc)[i]);
    }
    CHECK(v_steady_min > 7.0);
    CHECK(v_steady_max > 8.2 && v_steady_max < 9.2);

    // 2) Undamped LC tank oscillator: L = 1 mH, C = 1 uF, Vc(0) = 5.0 V
    CircuitNetlist tank;
    {
        ElectricalComponent l1;
        l1.id = "L1";
        l1.type = ElectricalComponentType::Inductor;
        l1.value_primary = 1e-3;
        l1.pins = {CircuitPin::make("1", "tank"), CircuitPin::make("2", "0")};
        tank.add_component(l1);

        ElectricalComponent c1;
        c1.id = "C1";
        c1.type = ElectricalComponentType::Capacitor;
        c1.value_primary = 1e-6;
        c1.value_secondary = 5.0; // Initial condition 5 V
        c1.pins = {CircuitPin::make("1", "tank"), CircuitPin::make("2", "0")};
        tank.add_component(c1);
        REQUIRE(tank.build_node_topology().is_ok());
    }
    SpiceTransientOptions topts;
    topts.t_stop_s = 4e-4; // ~2 cycles (T = 2*pi*sqrt(LC) ~ 198.7 us)
    topts.dt_s = 0.5e-6;
    topts.method = SpiceIntegrationMethod::Trapezoidal;
    topts.use_dc_op_initial = false;

    auto tres = SpiceSimulationEngine::solve_transient(tank, topts);
    REQUIRE(tres.is_ok());
    const auto* vtank = tres.value().get_voltage_trace("tank");
    REQUIRE(vtank != nullptr);
    const double min_v = *std::min_element(vtank->begin(), vtank->end());
    const double max_v = *std::max_element(vtank->begin(), vtank->end());
    CHECK(std::abs(max_v - 5.0) < 0.05);
    CHECK(std::abs(min_v + 5.0) < 0.05);

    // 3) AC small-signal RC -3 dB cutoff at fc = 1/(2*pi*1k*159.15nF) = 1000 Hz
    auto ac_net = CircuitBenchmarkFactory::create_rc_filter(1.0, 1000.0, 1.0 / (2.0 * 3.14159265358979323846 * 1e6));
    SpiceAcOptions ac_opts;
    ac_opts.f_start_hz = 100.0;
    ac_opts.f_stop_hz = 10000.0;
    ac_opts.points_per_decade = 10;
    ac_opts.input_source_id = "V1";
    ac_opts.output_node = "out";
    auto ac_res = SpiceSimulationEngine::solve_ac(ac_net, ac_opts);
    REQUIRE(ac_res.is_ok());
    const auto& pts = ac_res.value().bode_points;
    REQUIRE(pts.size() >= 21);
    const size_t mid = pts.size() / 2; // 1000 Hz
    CHECK(std::abs(pts[mid].frequency_hz - 1000.0) < 1.0);
    CHECK(std::abs(pts[mid].magnitude_gain_db - (-3.0103)) < 0.05);
}
