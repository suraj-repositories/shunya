#include "tests/test_framework.hpp"

#include "shunya/cirsim/ac_small_signal_solver.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <cmath>
#include <numbers>

using namespace shunya::cirsim;
using namespace shunya::core::domain;

using CirsimAcSolver = shunya::cirsim::AcSmallSignalSolver;
using CirsimComplexMatrix = shunya::cirsim::ComplexDenseMatrix;

TEST_CASE("PH4-08.03.001: Complex Admittance Matrix Assembly for Linear & Decade AC Frequency Sweeps") {
    // 1. Test frequency generation across decades
    AcSweepOptions opts;
    opts.f_start = 10.0;
    opts.f_stop = 100000.0; // 4 decades (10Hz to 100kHz)
    opts.points_per_decade = 5;
    opts.spacing = FrequencySweepSpacing::Decade;

    CirsimAcSolver solver(opts);
    auto freqs = solver.generate_frequencies();
    REQUIRE(freqs.size() == 21); // 4 * 5 + 1
    REQUIRE(std::abs(freqs.front() - 10.0) < 1e-6);
    REQUIRE(std::abs(freqs.back() - 100000.0) < 1e-3);

    // 2. Test admittance matrix assembly at 1kHz for an R-C filter
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
    v1.type = ElectricalComponentType::VoltageSourceAC;
    v1.value_primary = 1.0; // 1V AC
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0; // 1k
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent c1;
    c1.id = "C1";
    c1.type = ElectricalComponentType::Capacitor;
    c1.value_primary = 1.0e-7; // 100nF
    c1.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {v1, r1, c1};

    auto sys_res = solver.assemble_admittance_system(topology, comps, 1000.0);
    REQUIRE(sys_res.is_ok());

    auto [Y, I] = std::move(sys_res.value());
    REQUIRE(Y.rows() == 3); // 2 nodes + 1 vsource branch
    REQUIRE(Y.cols() == 3);
    REQUIRE(I.size() == 3);

    // V1 excitation in branch row
    REQUIRE(std::abs(I[2].real() - 1.0) < 1e-6);
    REQUIRE(std::abs(I[2].imag()) < 1e-6);

    // C1 admittance: j * w * C at 1kHz = j * 2 * pi * 1000 * 1e-7 = j * 6.28318e-4 S
    double expected_wc = 2.0 * std::numbers::pi * 1000.0 * 1.0e-7;
    REQUIRE(std::abs(Y(1, 1).imag() - expected_wc) < 1e-6);
}

TEST_CASE("PH4-08.03.002: Solve Complex Linear Equations Y(w) * V(w) = I(w) via Complex LU Factorization") {
    // Direct test of ComplexDenseMatrix partial-pivoting LU decomposition
    // [ (2 + j)   (1 - j) ] [ x0 ] = [ (5 + 2j) ]
    // [ (1 + 2j)  (3 + 4j)] [ x1 ]   [ (7 + 10j)]
    CirsimComplexMatrix A(2, 2);
    A(0, 0) = ComplexNumber{2.0, 1.0};
    A(0, 1) = ComplexNumber{1.0, -1.0};
    A(1, 0) = ComplexNumber{1.0, 2.0};
    A(1, 1) = ComplexNumber{3.0, 4.0};

    std::vector<ComplexNumber> b = {
        ComplexNumber{5.0, 2.0},
        ComplexNumber{7.0, 10.0}
    };

    auto sol_res = A.solve_lu(b);
    REQUIRE(sol_res.is_ok());

    auto x = sol_res.value();
    REQUIRE(x.size() == 2);

    // Verify A * x == b
    ComplexNumber ax0 = A(0, 0) * x[0] + A(0, 1) * x[1];
    ComplexNumber ax1 = A(1, 0) * x[0] + A(1, 1) * x[1];

    REQUIRE(std::abs(ax0 - b[0]) < 1e-12);
    REQUIRE(std::abs(ax1 - b[1]) < 1e-12);
}

TEST_CASE("PH4-08.03.003: Compute Node Voltage Magnitude (dB) and Phase Angle Across Frequency Decades") {
    // Benchmark: 1st-order RC Low-Pass Filter
    // R = 1591.55 Ohm, C = 100 nF -> fc = 1 / (2 * pi * R * C) = 1000 Hz (1 kHz)
    // Transfer function H(jw) = 1 / (1 + j * (f / fc))
    // At f = fc (1kHz):
    //   |H(fc)| = 1 / sqrt(2) = 0.707106 (-3.01 dB)
    //   arg(H(fc)) = -45.0 degrees
    // At f << fc (10Hz):
    //   |H| ~= 1.0 (0.0 dB), phase ~= 0.0 deg
    // At f >> fc (100kHz):
    //   |H| ~= 0.01 (-40.0 dB), phase ~= -90.0 deg
    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    CircuitNodeInfo n1; n1.node_index = 1; n1.node_name = "NET_IN";
    CircuitNodeInfo n2; n2.node_index = 2; n2.node_name = "NET_OUT";
    topology.nodes[0] = gnd;
    topology.nodes[1] = n1;
    topology.nodes[2] = n2;

    topology.pin_to_node_map["V_AC:1"] = 1;
    topology.pin_to_node_map["V_AC:2"] = 0;
    topology.pin_to_node_map["R_FILT:1"] = 1;
    topology.pin_to_node_map["R_FILT:2"] = 2;
    topology.pin_to_node_map["C_FILT:1"] = 2;
    topology.pin_to_node_map["C_FILT:2"] = 0;

    ElectricalComponent vac;
    vac.id = "V_AC";
    vac.type = ElectricalComponentType::VoltageSourceAC;
    vac.value_primary = 1.0; // 1V peak AC
    vac.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent rfilt;
    rfilt.id = "R_FILT";
    rfilt.type = ElectricalComponentType::Resistor;
    rfilt.value_primary = 1.0 / (2.0 * std::numbers::pi * 1000.0 * 1.0e-7); // ~1591.55 Ohm
    rfilt.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent cfilt;
    cfilt.id = "C_FILT";
    cfilt.type = ElectricalComponentType::Capacitor;
    cfilt.value_primary = 1.0e-7; // 100 nF
    cfilt.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    std::vector<ElectricalComponent> comps = {vac, rfilt, cfilt};

    AcSweepOptions opts;
    opts.f_start = 10.0;
    opts.f_stop = 100000.0;
    opts.points_per_decade = 10;
    opts.ac_input_amplitude = 1.0;
    opts.input_source_id = "V_AC";
    opts.spacing = FrequencySweepSpacing::Decade;

    CirsimAcSolver solver(opts);
    auto res = solver.solve_frequency_sweep(topology, comps);
    REQUIRE(res.is_ok());

    auto sol = res.value();
    REQUIRE(sol.success);
    REQUIRE(sol.node_responses.count(2) > 0);

    const auto& v_out = sol.node_responses.at(2);

    // 1. Verify low-frequency passband (10Hz)
    REQUIRE(std::abs(v_out.magnitude_linear[0] - 1.0) < 0.005);
    REQUIRE(std::abs(v_out.magnitude_db[0] - 0.0) < 0.05); // ~0 dB
    REQUIRE(std::abs(v_out.phase_degrees[0] - 0.0) < 1.0); // ~0 deg

    // 2. Find response near 1kHz cutoff
    size_t cutoff_idx = 0;
    double min_f_diff = 1e9;
    for (size_t i = 0; i < v_out.frequencies.size(); ++i) {
        double diff = std::abs(v_out.frequencies[i] - 1000.0);
        if (diff < min_f_diff) {
            min_f_diff = diff;
            cutoff_idx = i;
        }
    }

    // Cutoff frequency magnitude: ~ -3 dB (1/sqrt(2) = 0.7071)
    REQUIRE(std::abs(v_out.magnitude_db[cutoff_idx] - (-3.01)) < 0.1);
    // Cutoff frequency phase: ~ -45 degrees
    REQUIRE(std::abs(v_out.phase_degrees[cutoff_idx] - (-45.0)) < 1.0);

    // 3. High-frequency stopband (100kHz = 2 decades above cutoff)
    // Roll-off of 1st-order filter is -20 dB / decade -> should be ~ -40 dB at 100kHz
    const auto& last_db = v_out.magnitude_db.back();
    const auto& last_phase = v_out.phase_degrees.back();
    REQUIRE(std::abs(last_db - (-40.0)) < 0.5);
    REQUIRE(std::abs(last_phase - (-90.0)) < 1.0);
}
