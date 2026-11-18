#include "tests/test_framework.hpp"

#include "shunya/cirsim/nonlinear_dc_solver.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <cmath>

using namespace shunya::cirsim;
using namespace shunya::core::domain;

TEST_CASE("PH4-08.02.001: Damped Newton-Raphson Non-Linear Solver with Diode PN Junction Limiting") {
    // Diode circuit: V1 = 5V DC, R1 = 1k, D1 (1N4148 model: Is=1e-14, Vt=25.85mV) to Ground
    // Node 0: Ground (Cathode of D1)
    // Node 1: VCC = 5V
    // Node 2: Diode Anode
    ParsedSchematicTopology topology;

    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_VCC";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_ANODE";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["D1:1"] = 2; // Anode
    topology.pin_to_node_map["D1:2"] = 0; // Cathode

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

    ElectricalComponent d1;
    d1.id = "D1";
    d1.type = ElectricalComponentType::Diode;
    d1.value_primary = 0.7; // nominal forward drop
    d1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, d1};

    NonlinearSolverOptions opts;
    opts.damping_factor = 0.75;
    opts.max_iterations = 100;
    NonlinearDcOperatingPointSolver solver(opts);

    auto res = solver.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.converged);
    REQUIRE(sol.iterations > 1); // Confirms iterative Newton-Raphson convergence

    // In a 5V -> 1k -> Diode circuit:
    // V_D should settle in typical forward conduction range (0.65V to 0.75V)
    double vd = sol.node_voltages[2];
    REQUIRE(vd > 0.65);
    REQUIRE(vd < 0.75);

    // Current should be ~ (5.0 - vd) / 1000 ~= 4.3mA
    double expected_i = (5.0 - vd) / 1000.0;
    REQUIRE(std::abs(sol.branch_currents["R1"] - expected_i) < 1e-5);
}

TEST_CASE("PH4-08.02.002: Companion Dynamic Conductance Linearization for Diode, BJT & NMOS") {
    // 1. Diode linearization test
    double geq_d = 0.0, ieq_d = 0.0;
    NonlinearDcOperatingPointSolver::linearize_diode(0.7, 1e-14, 0.02585, 1.0, geq_d, ieq_d);
    REQUIRE(geq_d > 0.1); // Dynamic conductance at 0.7V forward bias is significant (> 0.1 S)
    REQUIRE(std::abs(ieq_d) > 0.0);

    // 2. BJT NPN linearization test
    double g_be = 0.0, g_m = 0.0, g_ce = 0.0, ib_eq = 0.0, ic_eq = 0.0;
    NonlinearDcOperatingPointSolver::linearize_bjt_npn(0.68, 5.0, 1e-14, 0.02585, 100.0,
                                                      g_be, g_m, g_ce, ib_eq, ic_eq);
    REQUIRE(g_be > 0.001);
    REQUIRE(g_m > 0.01);
    REQUIRE(g_ce > 0.0);

    // 3. NMOS linearization test
    double nmos_gm = 0.0, nmos_gds = 0.0, ids_eq = 0.0;
    // Vgs = 3V, Vds = 5V, Vth = 1V, Kp = 1e-3, lambda = 0.01 -> Saturation mode (Vds > Vgs - Vth = 2V)
    NonlinearDcOperatingPointSolver::linearize_nmos(3.0, 5.0, 1.0, 1.0e-3, 0.01,
                                                   nmos_gm, nmos_gds, ids_eq);
    REQUIRE(nmos_gm > 0.001);
    REQUIRE(nmos_gds > 0.0);
}

TEST_CASE("PH4-08.02.003: Source-Stepping & Gmin-Stepping Convergence Aids in Non-Linear Regimes") {
    // Tough circuit: High voltage 24V step across a steep diode
    ParsedSchematicTopology topology;

    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_HV";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_MID";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["D1:1"] = 2;
    topology.pin_to_node_map["D1:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 24.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 500.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent d1;
    d1.id = "D1";
    d1.type = ElectricalComponentType::Diode;
    d1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, d1};

    NonlinearSolverOptions opts;
    opts.enable_source_stepping = true;
    opts.source_steps = 10;
    opts.enable_gmin_stepping = true;
    NonlinearDcOperatingPointSolver solver(opts);

    auto res = solver.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.converged);
    REQUIRE(sol.node_voltages[2] > 0.65);
    REQUIRE(sol.node_voltages[2] < 0.85);
}

TEST_CASE("PH4-08.02.004: Short-Circuit & Over-Current Detection Algorithms with Node Diagnostics") {
    // Circuit with dead short across 12V voltage source (0.001 Ohm resistor)
    ParsedSchematicTopology topology;

    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_PWR";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;

    topology.pin_to_node_map["V_DEAD:1"] = 1;
    topology.pin_to_node_map["V_DEAD:2"] = 0;
    topology.pin_to_node_map["R_SHORT:1"] = 1;
    topology.pin_to_node_map["R_SHORT:2"] = 0;

    ElectricalComponent v_dead;
    v_dead.id = "V_DEAD";
    v_dead.type = ElectricalComponentType::VoltageSourceDC;
    v_dead.value_primary = 12.0;
    v_dead.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r_short;
    r_short.id = "R_SHORT";
    r_short.type = ElectricalComponentType::Resistor;
    r_short.value_primary = 0.05; // 0.05 Ohm -> 240A short circuit
    r_short.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v_dead, r_short};

    NonlinearSolverOptions opts;
    opts.max_current_limit = 50.0; // 50A threshold
    NonlinearDcOperatingPointSolver solver(opts);

    auto res = solver.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.converged);
    REQUIRE(!sol.diagnostics.empty());

    bool found_short = false;
    for (const auto& diag : sol.diagnostics) {
        if (diag.code == "CIRSIM_SHORT_CIRCUIT" && diag.component_id == "V_DEAD") {
            found_short = true;
            REQUIRE(diag.severity == CircuitDiagnosticReport::Severity::CriticalError);
            REQUIRE(diag.current_level > 200.0);
        }
    }
    REQUIRE(found_short);
}

TEST_CASE("PH4-08.02.005: Total Circuit Power Dissipation Calculation & Component Stress Analysis") {
    // Circuit: 10V DC, R1 = 100 Ohm (dissipates 10V^2 / 100 = 1W -> 400% stress on 0.25W resistor)
    ParsedSchematicTopology topology;

    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_10V";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R_HOT:1"] = 1;
    topology.pin_to_node_map["R_HOT:2"] = 0;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 10.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r_hot;
    r_hot.id = "R_HOT";
    r_hot.type = ElectricalComponentType::Resistor;
    r_hot.value_primary = 100.0; // 10V / 100 = 0.1A -> 1W
    r_hot.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r_hot};

    NonlinearDcOperatingPointSolver solver;
    auto res = solver.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.converged);

    // Tellegen power balance
    REQUIRE(std::abs(sol.total_power_delivered_watts - 1.0) < 1e-3);
    REQUIRE(std::abs(sol.total_power_dissipated_watts - 1.0) < 1e-3);

    // Component stress
    REQUIRE(!sol.component_stresses.empty());
    bool found_r_hot = false;
    for (const auto& st : sol.component_stresses) {
        if (st.component_id == "R_HOT") {
            found_r_hot = true;
            REQUIRE(std::abs(st.power_dissipated_watts - 1.0) < 1e-3);
            REQUIRE(st.stress_ratio > 3.9); // 1.0W / 0.25W = 4.0
            REQUIRE(st.is_over_stressed);
        }
    }
    REQUIRE(found_r_hot);
}

TEST_CASE("PH4-08.02.006: Benchmark DC Operating Point Solving Against Standard SPICE3f5 Reference Netlists") {
    // Benchmark Netlist: Standard SPICE3f5 Diode Clipper
    // Vin = 10V, R1 = 2.2k, D1 = 1N4148 (Is = 1e-14, Vt = 25.85mV, n = 1)
    // Analytical SPICE solution:
    // Id = (10 - Vd) / 2200
    // Id = Is * (exp(Vd / Vt) - 1)
    // SPICE3f5 operating point: Vd = 0.6953 V, Id = 4.229 mA
    ParsedSchematicTopology topology;

    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_IN";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_OUT";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V_IN:1"] = 1;
    topology.pin_to_node_map["V_IN:2"] = 0;
    topology.pin_to_node_map["R_LIMIT:1"] = 1;
    topology.pin_to_node_map["R_LIMIT:2"] = 2;
    topology.pin_to_node_map["D_CLAMP:1"] = 2;
    topology.pin_to_node_map["D_CLAMP:2"] = 0;

    ElectricalComponent vin;
    vin.id = "V_IN";
    vin.type = ElectricalComponentType::VoltageSourceDC;
    vin.value_primary = 10.0;
    vin.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent rlim;
    rlim.id = "R_LIMIT";
    rlim.type = ElectricalComponentType::Resistor;
    rlim.value_primary = 2200.0;
    rlim.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent dclamp;
    dclamp.id = "D_CLAMP";
    dclamp.type = ElectricalComponentType::Diode;
    dclamp.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {vin, rlim, dclamp};

    NonlinearDcOperatingPointSolver solver;
    auto res = solver.solve(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.converged);

    const double vd_solved = sol.node_voltages[2];
    const double id_solved = sol.branch_currents["D_CLAMP"];

    // Compare with SPICE3f5 analytical target (0.6953 V +/- 0.02V, 4.23mA +/- 0.05mA)
    REQUIRE(std::abs(vd_solved - 0.695) < 0.025);
    REQUIRE(std::abs(id_solved - 0.00423) < 0.0001);
}
