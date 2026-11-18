#include "tests/test_framework.hpp"

#include "shunya/cirsim/ac_small_signal_solver.hpp"
#include "shunya/cirsim/kcl_kvl_validator.hpp"
#include "shunya/cirsim/mna_system_builder.hpp"
#include "shunya/cirsim/nonlinear_dc_solver.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"
#include "shunya/cirsim/transient_simulation_engine.hpp"

#include <cmath>
#include <numbers>
#include <vector>

using namespace shunya::cirsim;
using namespace shunya::core::domain;

// =============================================================================
// PH4-13.01.003: Dedicated Circuit Solver Verification
// =============================================================================

TEST_CASE("PH4-13.01.003: DC Operating Points (Linear MNA & Nonlinear Diode)") {
    // 1. Linear Resistive Voltage Divider
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_IN";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_MID";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["R2:1"] = 2;
    topology.pin_to_node_map["R2:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 12.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 3000.0; // 3k
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent r2;
    r2.id = "R2";
    r2.type = ElectricalComponentType::Resistor;
    r2.value_primary = 1000.0; // 1k -> V_mid = 12 * 1 / (3+1) = 3V
    r2.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, r2};

    NonlinearSolverOptions opts;
    opts.damping_factor = 0.8;
    NonlinearDcOperatingPointSolver solver(opts);
    auto dc_res = solver.solve(topology, comps);
    REQUIRE(dc_res.is_ok());

    auto sol = dc_res.value();
    CHECK(sol.converged);
    CHECK(std::abs(sol.node_voltages[1] - 12.0) < 1e-4);
    CHECK(std::abs(sol.node_voltages[2] - 3.0) < 1e-4);

    // KCL/KVL Conservation Check
    std::vector<double> voltages = {sol.node_voltages[1], sol.node_voltages[2]};
    std::vector<double> currents = {-0.003}; // 3 mA total branch current
    auto val_rep = KclKvlValidator::verify_conservation(topology, comps, voltages, currents);
    CHECK(val_rep.kcl_conserved);

    // 2. Nonlinear Diode Circuit (5V -> 1k -> Diode -> GND)
    ParsedSchematicTopology d_top;
    d_top.nodes[0] = gnd;
    d_top.nodes[1] = n1;
    d_top.nodes[2] = n2;
    d_top.pin_to_node_map["V1:1"] = 1;
    d_top.pin_to_node_map["V1:2"] = 0;
    d_top.pin_to_node_map["R1:1"] = 1;
    d_top.pin_to_node_map["R1:2"] = 2;
    d_top.pin_to_node_map["D1:1"] = 2; // Anode
    d_top.pin_to_node_map["D1:2"] = 0; // Cathode

    v1.value_primary = 5.0;
    r1.value_primary = 1000.0;

    ElectricalComponent d1;
    d1.id = "D1";
    d1.type = ElectricalComponentType::Diode;
    d1.value_primary = 0.7; // 0.7V forward threshold
    d1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> d_comps = {v1, r1, d1};
    auto d_res = solver.solve(d_top, d_comps);
    REQUIRE(d_res.is_ok());
    auto d_sol = d_res.value();
    CHECK(d_sol.converged);
    CHECK(d_sol.node_voltages[2] > 0.60);
    CHECK(d_sol.node_voltages[2] < 0.85);
}

TEST_CASE("PH4-13.01.003: AC Frequency Small-Signal Frequency Response") {
    // RC Low-pass filter: R = 1k, C = 159.155 nF (fc = 1000 Hz)
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo in_node; in_node.node_index = 1; in_node.node_name = "VIN";
    CircuitNodeInfo out_node; out_node.node_index = 2; out_node.node_name = "VOUT";
    topology.nodes[0] = gnd;
    topology.nodes[1] = in_node;
    topology.nodes[2] = out_node;

    topology.pin_to_node_map["VIN:1"] = 1;
    topology.pin_to_node_map["VIN:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["C1:1"] = 2;
    topology.pin_to_node_map["C1:2"] = 0;

    ElectricalComponent v_ac;
    v_ac.id = "VIN";
    v_ac.type = ElectricalComponentType::VoltageSourceAC;
    v_ac.value_primary = 1.0; // 1V AC amplitude
    v_ac.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0 / (2.0 * std::numbers::pi * 1000.0 * 1000.0); // ~159.155 nF
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v_ac, r1, c1};

    AcSweepOptions opts;
    opts.f_start = 10.0;
    opts.f_stop = 100000.0;
    opts.points_per_decade = 20;
    opts.input_source_id = "VIN";

    shunya::cirsim::AcSmallSignalSolver ac_solver(opts);

    auto sweep_res = ac_solver.solve_frequency_sweep(topology, comps);
    REQUIRE(sweep_res.is_ok());

    auto sim_res = sweep_res.value();
    REQUIRE(sim_res.node_responses.count(2) > 0);

    const auto& vout_bode = sim_res.node_responses.at(2);
    auto metrics = shunya::cirsim::AcSmallSignalSolver::compute_bode_metrics(vout_bode);

    // Passband gain should be ~0 dB
    CHECK(std::abs(metrics.passband_gain_db) < 0.2);
    // Cutoff frequency should be approximately 1000 Hz
    CHECK(metrics.has_cutoff);
    CHECK(std::abs(metrics.cutoff_frequency_hz - 1000.0) < 50.0);
}

TEST_CASE("PH4-13.01.003: Transient Numerical Convergence & Integration Methods") {
    // RC circuit transient charging: V = 10V, R = 1k, C = 10 uF (tau = 10 ms)
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo in_n; in_n.node_index = 1; in_n.node_name = "VCC";
    CircuitNodeInfo out_n; out_n.node_index = 2; out_n.node_name = "VCAP";
    topology.nodes[0] = gnd;
    topology.nodes[1] = in_n;
    topology.nodes[2] = out_n;

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
    c1.value_primary = 10.0e-6; // 10 uF
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, c1};

    TransientOptions opts;
    opts.t_start = 0.0;
    opts.t_stop = 0.05; // 50 ms = 5 tau
    opts.dt_initial = 1.0e-4; // 100 us
    opts.method = TransientMethod::Trapezoidal;

    TransientSimulationEngine engine(opts);
    auto trans_res = engine.solve(topology, comps);
    REQUIRE(trans_res.is_ok());

    auto result = trans_res.value();
    CHECK(result.success);
    CHECK(result.time_points.size() > 50);

    REQUIRE(result.node_voltages.count(2) > 0);
    const auto& vcap_trace = result.node_voltages.at(2);
    REQUIRE(!vcap_trace.empty());

    // At t = 1 tau (10 ms), Vc = 10 * (1 - e^-1) = 6.321V
    double v_at_1tau = 0.0;
    for (size_t i = 0; i < result.time_points.size(); ++i) {
        if (result.time_points[i] >= 0.010) {
            v_at_1tau = vcap_trace[i];
            break;
        }
    }
    CHECK(std::abs(v_at_1tau - 6.321) < 0.25);

    // At t = 5 tau (50 ms), capacitor should be near 10V
    CHECK(vcap_trace.back() > 9.8);
    CHECK(vcap_trace.back() <= 10.01);
}
