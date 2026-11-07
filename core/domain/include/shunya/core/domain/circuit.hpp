#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"

#include <cmath>
#include <complex>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// ELEC-01 & ELEC-02: Schematic Graph, Pin Model & Netlist Generation
// =============================================================================

enum class ElectricalPinPolarity {
    PassiveBiDirectional,
    Positive,
    Negative,
    Anode,
    Cathode,
    Base,
    Collector,
    Emitter,
    Gate,
    Drain,
    Source
};

enum class PinElectricalDomain {
    AnalogElectrical,
    DigitalLogic,
    PowerGround,
    RF
};

[[nodiscard]] std::string_view pin_polarity_to_string(ElectricalPinPolarity pol) noexcept;
[[nodiscard]] std::string_view pin_domain_to_string(PinElectricalDomain dom) noexcept;

struct CircuitPin {
    std::string pin_id;
    std::string component_id;
    std::string name;
    ElectricalPinPolarity polarity = ElectricalPinPolarity::PassiveBiDirectional;
    PinElectricalDomain domain = PinElectricalDomain::AnalogElectrical;
    std::string connected_net = "0"; // "0" is ground

    static CircuitPin make(std::string pid, std::string net,
                           ElectricalPinPolarity pol = ElectricalPinPolarity::PassiveBiDirectional,
                           std::string cid = "", std::string pname = "",
                           PinElectricalDomain dom = PinElectricalDomain::AnalogElectrical) {
        CircuitPin p;
        p.pin_id = std::move(pid);
        p.connected_net = std::move(net);
        p.polarity = pol;
        p.component_id = std::move(cid);
        p.name = std::move(pname);
        p.domain = dom;
        return p;
    }
};

struct CircuitNode {
    std::string id;       // e.g. "0", "1", "in", "out"
    uint32_t index = 0;   // 0 = Ground
    bool is_ground = false;
    std::vector<std::string> connected_pin_ids;
};

// =============================================================================
// ELEC-03 & ELEC-04: Passive Devices & Sources
// =============================================================================

enum class ElectricalComponentType {
    Resistor,
    Capacitor,
    Inductor,
    Transformer,
    VoltageSourceDC,
    VoltageSourceAC,
    VoltageSourcePulse,
    CurrentSourceDC,
    CurrentSourceAC,
    VCVS, // Voltage-Controlled Voltage Source (E)
    VCCS, // Voltage-Controlled Current Source (G)
    CCVS, // Current-Controlled Voltage Source (H)
    CCCS, // Current-Controlled Current Source (F)
    Diode,
    TransistorBJT,
    TransistorMOSFET
};

[[nodiscard]] std::string_view component_type_to_string(ElectricalComponentType type) noexcept;

struct WaveformParams {
    double dc_offset = 0.0;
    double amplitude = 1.0;
    double frequency_hz = 1000.0;
    double phase_deg = 0.0;

    // Pulse parameters
    double v_initial = 0.0;
    double v_pulsed = 5.0;
    double delay_time_s = 0.0;
    double rise_time_s = 1e-9;
    double fall_time_s = 1e-9;
    double pulse_width_s = 0.5e-3;
    double period_s = 1.0e-3;

    [[nodiscard]] double evaluate_sine(double t) const noexcept;
    [[nodiscard]] double evaluate_pulse(double t) const noexcept;
};

struct ElectricalComponent {
    std::string id;
    ElectricalComponentType type = ElectricalComponentType::Resistor;
    std::vector<CircuitPin> pins;

    // Parameters
    double value_primary = 1000.0;    // Resistance (Ohm), Capacitance (F), Inductance (H), Voltage (V), Current (A)
    double value_secondary = 0.0;     // Parasitic resistance, initial condition (IC)
    double value_tertiary = 0.0;      // Coupling ratio (Transformer turns N2/N1), transconductance/gain

    // Source waveform parameters
    WaveformParams waveform;

    // Non-linear parameters (Diode / BJT / MOSFET)
    double is_saturation_current = 1e-14;  // Is
    double thermal_voltage = 0.02585;     // Vt at 300K
    double ideality_factor = 1.0;          // n
    double bjt_beta_forward = 100.0;       // Beta for BJT
    double mos_vth = 1.0;                  // Threshold voltage Vth
    double mos_kp = 1e-3;                  // Transconductance parameter Kn = mu * Cox * W/L
    double mos_lambda = 0.0;               // Channel length modulation lambda

    // Controlling branch / nodes for dependent sources
    std::string ctrl_node_pos;
    std::string ctrl_node_neg;
};

class CircuitNetlist {
public:
    CircuitNetlist() = default;

    void add_component(ElectricalComponent comp);
    [[nodiscard]] const std::vector<ElectricalComponent>& components() const noexcept { return components_; }
    [[nodiscard]] std::vector<ElectricalComponent>& components() noexcept { return components_; }
    [[nodiscard]] const ElectricalComponent* find_component(std::string_view id) const noexcept;

    void add_wire(std::string_view from_pin, std::string_view to_pin);

    // Compiles schematic graph into mapped electrical nodes (Node 0 is always Ground)
    [[nodiscard]] base::Result<void> build_node_topology();

    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] uint32_t get_node_index(std::string_view node_name) const;
    [[nodiscard]] std::string_view get_node_name(uint32_t index) const;
    [[nodiscard]] const std::unordered_map<std::string, CircuitNode>& nodes() const noexcept { return nodes_; }

    [[nodiscard]] std::string export_spice_netlist(std::string_view title = "Shunya Circuit") const;

    void clear() noexcept;

private:
    std::vector<ElectricalComponent> components_;
    std::vector<std::pair<std::string, std::string>> wires_;
    std::unordered_map<std::string, CircuitNode> nodes_;
    std::unordered_map<uint32_t, std::string> index_to_name_;
};

// =============================================================================
// ELEC-05 & ELEC-13: Ground Checks, Floating Nodes & Circuit Diagnostics
// =============================================================================

enum class CircuitDiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct CircuitDiagnosticIssue {
    CircuitDiagnosticSeverity severity = CircuitDiagnosticSeverity::Error;
    std::string code;
    std::string message;
    std::string target_node_or_component;
    std::string actionable_remediation;
};

class CircuitDiagnostics {
public:
    [[nodiscard]] static std::vector<CircuitDiagnosticIssue> inspect_circuit(
        const CircuitNetlist& netlist);

    [[nodiscard]] static bool has_valid_ground(const CircuitNetlist& netlist) noexcept;
    [[nodiscard]] static std::vector<std::string> find_floating_nodes(const CircuitNetlist& netlist);
    [[nodiscard]] static std::vector<std::string> find_voltage_source_loops(const CircuitNetlist& netlist);
};

// =============================================================================
// ELEC-06: Modified Nodal Analysis (MNA) Linear System Assembly
// =============================================================================

class DenseMatrix {
public:
    DenseMatrix(size_t rows = 0, size_t cols = 0);

    void resize(size_t rows, size_t cols);
    [[nodiscard]] size_t rows() const noexcept { return rows_; }
    [[nodiscard]] size_t cols() const noexcept { return cols_; }

    [[nodiscard]] double& operator()(size_t r, size_t c) noexcept { return data_[r * cols_ + c]; }
    [[nodiscard]] double operator()(size_t r, size_t c) const noexcept { return data_[r * cols_ + c]; }

    void add(size_t r, size_t c, double val) noexcept { data_[r * cols_ + c] += val; }
    void clear() noexcept;

    // LU decomposition with partial pivoting: A * x = b
    [[nodiscard]] base::Result<std::vector<double>> solve_lu(const std::vector<double>& b) const;

    [[nodiscard]] const std::vector<double>& data() const noexcept { return data_; }

private:
    size_t rows_ = 0;
    size_t cols_ = 0;
    std::vector<double> data_;
};

struct MnaSystem {
    DenseMatrix A;             // Complete MNA matrix [G B; C D]
    std::vector<double> z;     // RHS vector [j; e]
    size_t num_nodes = 0;      // N (excluding ground node 0)
    size_t num_branches = 0;   // M (voltage source and inductor currents)
    std::unordered_map<std::string, size_t> vsource_branch_map;
};

class MnaAssembler {
public:
    [[nodiscard]] static base::Result<MnaSystem> assemble_dc_system(const CircuitNetlist& netlist);

    [[nodiscard]] static base::Result<MnaSystem> assemble_transient_step(
        const CircuitNetlist& netlist,
        double t, double dt,
        const std::unordered_map<std::string, double>& cap_voltages_prev,
        const std::unordered_map<std::string, double>& ind_currents_prev,
        bool use_trapezoidal = true);
};

// =============================================================================
// ELEC-07: DC Operating Point Analysis
// =============================================================================

struct DCOperatingPointResult {
    bool converged = false;
    uint32_t iterations = 0;
    std::unordered_map<std::string, double> node_voltages;
    std::unordered_map<std::string, double> branch_currents;
    std::unordered_map<std::string, double> component_power;
    std::string error_message;

    [[nodiscard]] double get_voltage(std::string_view node_name) const noexcept;
    [[nodiscard]] double get_current(std::string_view comp_id) const noexcept;
    [[nodiscard]] double get_power(std::string_view comp_id) const noexcept;
};

class DcOperatingPointSolver {
public:
    DcOperatingPointSolver() = default;

    uint32_t max_iterations = 200;
    double voltage_tolerance = 1e-6; // 1 uV
    double current_tolerance = 1e-9; // 1 nA
    double gmin = 1e-12;             // 1 pS shunt conductance for stability

    [[nodiscard]] base::Result<DCOperatingPointResult> solve(const CircuitNetlist& netlist);
};

// =============================================================================
// ELEC-08: Transient Analysis
// =============================================================================

enum class TransientIntegrationMethod {
    BackwardEuler,
    Trapezoidal
};

struct TransientResult {
    bool success = false;
    std::vector<double> time_points;
    std::unordered_map<std::string, std::vector<double>> node_voltages; // [node_name] -> values
    std::unordered_map<std::string, std::vector<double>> branch_currents; // [comp_id] -> values
    std::string error_message;

    [[nodiscard]] const std::vector<double>* get_voltage_trace(std::string_view node_name) const noexcept;
    [[nodiscard]] const std::vector<double>* get_current_trace(std::string_view comp_id) const noexcept;
};

class TransientSolver {
public:
    TransientSolver() = default;

    double t_stop = 1e-3;
    double time_step = 1e-6;
    TransientIntegrationMethod method = TransientIntegrationMethod::BackwardEuler;
    bool use_dc_op_initial_state = true;
    uint32_t max_newton_iterations = 50;

    [[nodiscard]] base::Result<TransientResult> solve(const CircuitNetlist& netlist);
};

// =============================================================================
// ELEC-09 & ELEC-10: Non-linear Diode & Transistor Newton-Raphson Models
// =============================================================================

struct NonLinearDeviceState {
    // Diode
    static void evaluate_diode(
        double v_diode, double is, double vt, double n,
        double& out_current, double& out_conductance) noexcept;

    // BJT (NPN active / saturation approximation)
    static void evaluate_bjt_npn(
        double v_be, double v_ce, double is, double vt, double beta,
        double& out_ib, double& out_ic, double& out_gbe, double& out_gm, double& out_gce) noexcept;

    // MOSFET (NMOS Level 1 Shichman-Hodges)
    static void evaluate_mosfet_nmos(
        double v_gs, double v_ds, double vth, double kp, double lambda,
        double& out_ids, double& out_gm, double& out_gds) noexcept;
};

// =============================================================================
// ELEC-11: AC Small-Signal Frequency Response Analysis
// =============================================================================

struct ComplexDenseMatrix {
    ComplexDenseMatrix(size_t rows = 0, size_t cols = 0);

    void resize(size_t rows, size_t cols);
    [[nodiscard]] size_t rows() const noexcept { return rows_; }
    [[nodiscard]] size_t cols() const noexcept { return cols_; }

    [[nodiscard]] std::complex<double>& operator()(size_t r, size_t c) noexcept { return data_[r * cols_ + c]; }
    [[nodiscard]] std::complex<double> operator()(size_t r, size_t c) const noexcept { return data_[r * cols_ + c]; }

    void add(size_t r, size_t c, std::complex<double> val) noexcept { data_[r * cols_ + c] += val; }
    void clear() noexcept;

    [[nodiscard]] base::Result<std::vector<std::complex<double>>> solve_lu(
        const std::vector<std::complex<double>>& b) const;

private:
    size_t rows_ = 0;
    size_t cols_ = 0;
    std::vector<std::complex<double>> data_;
};

struct AcFrequencyPoint {
    double frequency_hz = 0.0;
    double magnitude_gain_db = 0.0;
    double phase_deg = 0.0;
    std::complex<double> complex_gain{0.0, 0.0};
};

struct AcAnalysisResult {
    bool success = false;
    std::vector<double> frequencies;
    std::vector<AcFrequencyPoint> bode_points;
    std::unordered_map<std::string, std::vector<std::complex<double>>> node_voltages;
    std::string error_message;
};

class AcSmallSignalSolver {
public:
    AcSmallSignalSolver() = default;

    double f_start_hz = 1.0;
    double f_stop_hz = 1e6;
    uint32_t points_per_decade = 20;
    std::string input_source_id = "V1";
    std::string output_node = "out";

    [[nodiscard]] base::Result<AcAnalysisResult> solve(
        const CircuitNetlist& netlist,
        const DCOperatingPointResult& op_point);
};

// =============================================================================
// ELEC-12: Circuit Probes & Waveform Computations
// =============================================================================

enum class ProbeType {
    NodeVoltage,
    DifferentialVoltage,
    BranchCurrent,
    ComponentPower
};

struct CircuitProbe {
    std::string id = "probe_1";
    ProbeType type = ProbeType::NodeVoltage;
    std::string node_pos = "out";
    std::string node_neg = "0";
    std::string component_id;
    std::string display_name = "V(out)";
    std::string unit = "V";
};

class ProbeMeasurementEngine {
public:
    [[nodiscard]] static std::vector<double> evaluate_probe_trace(
        const CircuitProbe& probe,
        const TransientResult& transient);

    [[nodiscard]] static double compute_average_value(const std::vector<double>& trace) noexcept;
    [[nodiscard]] static double compute_rms_value(const std::vector<double>& trace) noexcept;
    [[nodiscard]] static double compute_peak_to_peak(const std::vector<double>& trace) noexcept;
    [[nodiscard]] static double compute_total_energy(
        const std::vector<double>& time_trace,
        const std::vector<double>& power_trace) noexcept;
};

// =============================================================================
// ELEC-14: Comprehensive Circuit Validation Benchmarks
// =============================================================================

class CircuitBenchmarkFactory {
public:
    // Benchmark 1: Resistive Voltage Divider (Vin=10V, R1=1k, R2=1k -> Vout=5V)
    [[nodiscard]] static CircuitNetlist create_voltage_divider(double vin, double r1, double r2);

    // Benchmark 2: RC Low-Pass Filter step response
    [[nodiscard]] static CircuitNetlist create_rc_filter(double v_step, double r, double c);

    // Benchmark 3: RL High-Pass Filter step response
    [[nodiscard]] static CircuitNetlist create_rl_filter(double v_step, double r, double l);

    // Benchmark 4: Half-Wave Diode Rectifier
    [[nodiscard]] static CircuitNetlist create_diode_rectifier(double v_ac, double freq, double r_load, double c_filter);

    // Benchmark 5: Common-Emitter BJT Amplifier / Inverter
    [[nodiscard]] static CircuitNetlist create_bjt_inverter(double vcc, double rb, double rc);

    // Benchmark 6: NMOS Inverter
    [[nodiscard]] static CircuitNetlist create_nmos_inverter(double vdd, double r_load, double v_in);
};

} // namespace shunya::core::domain
