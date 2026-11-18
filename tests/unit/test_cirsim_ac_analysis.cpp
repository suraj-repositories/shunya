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

TEST_CASE("PH4-08.03.004: Bode Plot Calculation (Gain Margin, Phase Margin, 3dB Cutoff Frequency)") {
    // RC filter with cutoff at exactly 1000 Hz
    AcNodeResponse resp;
    const double fc = 1000.0;
    for (int exp = 1; exp <= 6; ++exp) {
        for (int step = 0; step < 10; ++step) {
            double f = std::pow(10.0, exp + step * 0.1);
            resp.frequencies.push_back(f);
            // Analytical 1st order lowpass: H = 1 / (1 + j*(f/fc))
            double norm_f = f / fc;
            double mag = 1.0 / std::sqrt(1.0 + norm_f * norm_f);
            double mag_db = 20.0 * std::log10(mag);
            double phase_deg = -std::atan(norm_f) * (180.0 / std::numbers::pi);

            resp.magnitude_linear.push_back(mag);
            resp.magnitude_db.push_back(mag_db);
            resp.phase_degrees.push_back(phase_deg);
        }
    }

    auto metrics = CirsimAcSolver::compute_bode_metrics(resp);

    REQUIRE(metrics.has_cutoff);
    // Interpolated cutoff frequency should be ~1000 Hz (+/- 1.5%)
    REQUIRE(std::abs(metrics.cutoff_frequency_hz - 1000.0) < 15.0);
    REQUIRE(std::abs(metrics.passband_gain_db - 0.0) < 0.05);
}

TEST_CASE("PH4-08.03.005: Impedance Matching & Resonant Frequency Detection for RLC Circuits") {
    // 1. Series RLC circuit: R = 10 Ohm, L = 100 uH, C = 10 nF
    // Resonant frequency: f0 = 1 / (2 * pi * sqrt(L * C))
    // f0 = 1 / (2 * pi * sqrt(1e-4 * 1e-8)) = 1 / (2 * pi * 1e-6) ~= 159,155 Hz (159.155 kHz)
    // At f0, Z_in = R = 10 Ohm (purely resistive)
    const double r = 10.0;
    const double l = 1.0e-4;
    const double c = 1.0e-8;
    const double f0_expected = 1.0 / (2.0 * std::numbers::pi * std::sqrt(l * c));

    std::vector<double> freqs;
    std::vector<ComplexNumber> z_in;

    for (int i = 0; i < 200; ++i) {
        double f = 50000.0 + i * 1500.0; // 50kHz to 350kHz
        freqs.push_back(f);
        double w = 2.0 * std::numbers::pi * f;
        double x_l = w * l;
        double x_c = 1.0 / (w * c);
        z_in.push_back(ComplexNumber{r, x_l - x_c});
    }

    auto res_metrics = CirsimAcSolver::analyze_rlc_resonance(freqs, z_in);
    REQUIRE(res_metrics.resonance_detected);
    REQUIRE(std::abs(res_metrics.resonant_frequency_hz - f0_expected) < 500.0);
    REQUIRE(std::abs(res_metrics.impedance_at_resonance_ohms - 10.0) < 0.1);

    // 2. Impedance Matching Calculation
    ComplexNumber z_load{75.0, 25.0}; // 75 + j25 Ohm
    auto match = CirsimAcSolver::calculate_impedance_matching(f0_expected, z_load, 50.0);

    // Conjugate matched load should be 75 - j25 Ohm
    REQUIRE(std::abs(match.z_matched_load.real() - 75.0) < 1e-9);
    REQUIRE(std::abs(match.z_matched_load.imag() - (-25.0)) < 1e-9);

    // Return loss > 0 dB, VSWR > 1.0
    REQUIRE(match.return_loss_db > 5.0);
    REQUIRE(match.vswr > 1.0);
    REQUIRE(match.vswr < 3.0);
}

TEST_CASE("PH4-08.03.006: Connect AC Simulation Results to Desktop Interactive Scientific Plotter") {
    AcNodeResponse resp;
    resp.frequencies = {10.0, 100.0, 1000.0, 10000.0, 100000.0};
    resp.magnitude_db = {0.0, -0.05, -3.01, -20.0, -40.0};
    resp.phase_degrees = {0.0, -5.7, -45.0, -84.3, -89.4};

    auto plot_series = CirsimAcSolver::export_for_scientific_plot(resp, "V_OUT", true);

    REQUIRE(plot_series.node_name == "V_OUT");
    REQUIRE(plot_series.magnitude_series.size() == 5);
    REQUIRE(plot_series.phase_series.size() == 5);

    // Log10 frequency axis: log10(10) = 1, log10(100) = 2, ..., log10(100000) = 5
    REQUIRE(std::abs(plot_series.magnitude_series[0].x - 1.0) < 1e-9);
    REQUIRE(std::abs(plot_series.magnitude_series[2].x - 3.0) < 1e-9); // 1kHz = 3
    REQUIRE(std::abs(plot_series.magnitude_series[4].x - 5.0) < 1e-9); // 100kHz = 5

    // Magnitude at 1kHz should be -3.01 dB
    REQUIRE(std::abs(plot_series.magnitude_series[2].y - (-3.01)) < 1e-9);

    // Range validity for ScientificPlot.qml
    REQUIRE(plot_series.magnitude_range.is_valid());
    REQUIRE(plot_series.phase_range.is_valid());
    REQUIRE(plot_series.magnitude_range.x_min == 1.0);
    REQUIRE(plot_series.magnitude_range.x_max == 5.0);
}

