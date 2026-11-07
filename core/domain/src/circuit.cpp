#include "shunya/core/domain/circuit.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::core::domain {

namespace {
base::Error make_circuit_error(std::string message, int32_t code = 5001) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Simulation,
        .severity = base::Severity::Error,
        .message = std::move(message),
        .details = ""
    };
}

constexpr double PI = 3.14159265358979323846;
} // namespace

// =============================================================================
// ELEC-01 & ELEC-02: Schematic Graph, Pin Model & Netlist Generation
// =============================================================================

std::string_view pin_polarity_to_string(ElectricalPinPolarity pol) noexcept {
    switch (pol) {
        case ElectricalPinPolarity::PassiveBiDirectional: return "PassiveBiDirectional";
        case ElectricalPinPolarity::Positive: return "Positive";
        case ElectricalPinPolarity::Negative: return "Negative";
        case ElectricalPinPolarity::Anode: return "Anode";
        case ElectricalPinPolarity::Cathode: return "Cathode";
        case ElectricalPinPolarity::Base: return "Base";
        case ElectricalPinPolarity::Collector: return "Collector";
        case ElectricalPinPolarity::Emitter: return "Emitter";
        case ElectricalPinPolarity::Gate: return "Gate";
        case ElectricalPinPolarity::Drain: return "Drain";
        case ElectricalPinPolarity::Source: return "Source";
    }
    return "Unknown";
}

std::string_view pin_domain_to_string(PinElectricalDomain dom) noexcept {
    switch (dom) {
        case PinElectricalDomain::AnalogElectrical: return "AnalogElectrical";
        case PinElectricalDomain::DigitalLogic: return "DigitalLogic";
        case PinElectricalDomain::PowerGround: return "PowerGround";
        case PinElectricalDomain::RF: return "RF";
    }
    return "Unknown";
}

std::string_view component_type_to_string(ElectricalComponentType type) noexcept {
    switch (type) {
        case ElectricalComponentType::Resistor: return "Resistor";
        case ElectricalComponentType::Capacitor: return "Capacitor";
        case ElectricalComponentType::Inductor: return "Inductor";
        case ElectricalComponentType::Transformer: return "Transformer";
        case ElectricalComponentType::VoltageSourceDC: return "VoltageSourceDC";
        case ElectricalComponentType::VoltageSourceAC: return "VoltageSourceAC";
        case ElectricalComponentType::VoltageSourcePulse: return "VoltageSourcePulse";
        case ElectricalComponentType::CurrentSourceDC: return "CurrentSourceDC";
        case ElectricalComponentType::CurrentSourceAC: return "CurrentSourceAC";
        case ElectricalComponentType::VCVS: return "VCVS";
        case ElectricalComponentType::VCCS: return "VCCS";
        case ElectricalComponentType::CCVS: return "CCVS";
        case ElectricalComponentType::CCCS: return "CCCS";
        case ElectricalComponentType::Diode: return "Diode";
        case ElectricalComponentType::TransistorBJT: return "TransistorBJT";
        case ElectricalComponentType::TransistorMOSFET: return "TransistorMOSFET";
    }
    return "Unknown";
}

double WaveformParams::evaluate_sine(double t) const noexcept {
    double phase_rad = phase_deg * PI / 180.0;
    return dc_offset + amplitude * std::sin(2.0 * PI * frequency_hz * t + phase_rad);
}

double WaveformParams::evaluate_pulse(double t) const noexcept {
    if (t < delay_time_s) return v_initial;
    double t_rel = std::fmod(t - delay_time_s, period_s > 1e-15 ? period_s : 1.0);

    if (t_rel < rise_time_s) {
        double frac = rise_time_s > 1e-15 ? (t_rel / rise_time_s) : 1.0;
        return v_initial + frac * (v_pulsed - v_initial);
    }
    t_rel -= rise_time_s;
    if (t_rel < pulse_width_s) {
        return v_pulsed;
    }
    t_rel -= pulse_width_s;
    if (t_rel < fall_time_s) {
        double frac = fall_time_s > 1e-15 ? (t_rel / fall_time_s) : 1.0;
        return v_pulsed + frac * (v_initial - v_pulsed);
    }
    return v_initial;
}

void CircuitNetlist::add_component(ElectricalComponent comp) {
    components_.push_back(std::move(comp));
}

const ElectricalComponent* CircuitNetlist::find_component(std::string_view id) const noexcept {
    for (const auto& comp : components_) {
        if (comp.id == id) return &comp;
    }
    return nullptr;
}

void CircuitNetlist::add_wire(std::string_view from_pin, std::string_view to_pin) {
    wires_.push_back({std::string(from_pin), std::string(to_pin)});
}

base::Result<void> CircuitNetlist::build_node_topology() {
    nodes_.clear();
    index_to_name_.clear();

    // Map connected nets from component pins
    // Ground is always node 0
    CircuitNode ground_node;
    ground_node.id = "0";
    ground_node.index = 0;
    ground_node.is_ground = true;
    nodes_["0"] = ground_node;
    index_to_name_[0] = "0";

    uint32_t next_index = 1;

    for (auto& comp : components_) {
        for (auto& pin : comp.pins) {
            std::string net = pin.connected_net;
            if (net.empty()) net = "0";

            if (net == "0" || net == "GND" || net == "gnd") {
                pin.connected_net = "0";
                nodes_["0"].connected_pin_ids.push_back(pin.pin_id);
            } else {
                auto it = nodes_.find(net);
                if (it == nodes_.end()) {
                    CircuitNode node;
                    node.id = net;
                    node.index = next_index++;
                    node.is_ground = false;
                    node.connected_pin_ids.push_back(pin.pin_id);
                    index_to_name_[node.index] = net;
                    nodes_[net] = std::move(node);
                } else {
                    it->second.connected_pin_ids.push_back(pin.pin_id);
                }
            }
        }
    }

    return base::Result<void>::ok();
}

uint32_t CircuitNetlist::get_node_index(std::string_view node_name) const {
    if (node_name == "0" || node_name == "GND" || node_name == "gnd") return 0;
    auto it = nodes_.find(std::string(node_name));
    if (it != nodes_.end()) {
        return it->second.index;
    }
    return 0;
}

std::string_view CircuitNetlist::get_node_name(uint32_t index) const {
    auto it = index_to_name_.find(index);
    if (it != index_to_name_.end()) {
        return it->second;
    }
    return "0";
}

std::string CircuitNetlist::export_spice_netlist(std::string_view title) const {
    std::ostringstream ss;
    ss << "* " << title << "\n";
    for (const auto& comp : components_) {
        std::string n1 = comp.pins.size() > 0 ? comp.pins[0].connected_net : "0";
        std::string n2 = comp.pins.size() > 1 ? comp.pins[1].connected_net : "0";

        switch (comp.type) {
            case ElectricalComponentType::Resistor:
                ss << "R" << comp.id << " " << n1 << " " << n2 << " " << comp.value_primary << "\n";
                break;
            case ElectricalComponentType::Capacitor:
                ss << "C" << comp.id << " " << n1 << " " << n2 << " " << comp.value_primary << "\n";
                break;
            case ElectricalComponentType::Inductor:
                ss << "L" << comp.id << " " << n1 << " " << n2 << " " << comp.value_primary << "\n";
                break;
            case ElectricalComponentType::VoltageSourceDC:
                ss << "V" << comp.id << " " << n1 << " " << n2 << " DC " << comp.value_primary << "\n";
                break;
            case ElectricalComponentType::CurrentSourceDC:
                ss << "I" << comp.id << " " << n1 << " " << n2 << " DC " << comp.value_primary << "\n";
                break;
            case ElectricalComponentType::Diode:
                ss << "D" << comp.id << " " << n1 << " " << n2 << " DIODE\n";
                break;
            default:
                ss << "X" << comp.id << " " << n1 << " " << n2 << " " << component_type_to_string(comp.type) << "\n";
                break;
        }
    }
    ss << ".end\n";
    return ss.str();
}

void CircuitNetlist::clear() noexcept {
    components_.clear();
    wires_.clear();
    nodes_.clear();
    index_to_name_.clear();
}

// =============================================================================
// ELEC-05 & ELEC-13: Ground Checks, Floating Nodes & Diagnostics
// =============================================================================

bool CircuitDiagnostics::has_valid_ground(const CircuitNetlist& netlist) noexcept {
    auto it = netlist.nodes().find("0");
    if (it != netlist.nodes().end()) {
        return !it->second.connected_pin_ids.empty();
    }
    return false;
}

std::vector<std::string> CircuitDiagnostics::find_floating_nodes(const CircuitNetlist& netlist) {
    std::vector<std::string> floating;
    for (const auto& [name, node] : netlist.nodes()) {
        if (node.is_ground) continue;
        if (node.connected_pin_ids.size() < 2) {
            floating.push_back(name);
        }
    }
    return floating;
}

std::vector<std::string> CircuitDiagnostics::find_voltage_source_loops(const CircuitNetlist& netlist) {
    std::vector<std::string> loops;
    // Detect two ideal voltage sources in parallel connected to the exact same nodes
    for (size_t i = 0; i < netlist.components().size(); ++i) {
        const auto& c1 = netlist.components()[i];
        if (c1.type != ElectricalComponentType::VoltageSourceDC) continue;
        if (c1.pins.size() < 2) continue;

        for (size_t j = i + 1; j < netlist.components().size(); ++j) {
            const auto& c2 = netlist.components()[j];
            if (c2.type != ElectricalComponentType::VoltageSourceDC) continue;
            if (c2.pins.size() < 2) continue;

            bool same_nodes = (c1.pins[0].connected_net == c2.pins[0].connected_net &&
                               c1.pins[1].connected_net == c2.pins[1].connected_net) ||
                              (c1.pins[0].connected_net == c2.pins[1].connected_net &&
                               c1.pins[1].connected_net == c2.pins[0].connected_net);

            if (same_nodes && std::abs(c1.value_primary - c2.value_primary) > 1e-6) {
                loops.push_back(c1.id + " <-> " + c2.id);
            }
        }
    }
    return loops;
}

std::vector<CircuitDiagnosticIssue> CircuitDiagnostics::inspect_circuit(const CircuitNetlist& netlist) {
    std::vector<CircuitDiagnosticIssue> issues;

    if (!has_valid_ground(netlist)) {
        issues.push_back(CircuitDiagnosticIssue{
            .severity = CircuitDiagnosticSeverity::Error,
            .code = "ELEC_NO_GROUND",
            .message = "Circuit does not contain a reference ground node (Node 0).",
            .target_node_or_component = "Node 0",
            .actionable_remediation = "Connect at least one terminal to ground node '0'."
        });
    }

    auto floating = find_floating_nodes(netlist);
    for (const auto& node_name : floating) {
        issues.push_back(CircuitDiagnosticIssue{
            .severity = CircuitDiagnosticSeverity::Warning,
            .code = "ELEC_FLOATING_NODE",
            .message = "Node '" + node_name + "' has fewer than 2 connections.",
            .target_node_or_component = node_name,
            .actionable_remediation = "Connect node to a closed path or add high-value shunt resistor."
        });
    }

    auto loops = find_voltage_source_loops(netlist);
    for (const auto& loop_pair : loops) {
        issues.push_back(CircuitDiagnosticIssue{
            .severity = CircuitDiagnosticSeverity::Error,
            .code = "ELEC_VOLTAGE_SOURCE_LOOP",
            .message = "Conflicting parallel ideal voltage sources: " + loop_pair,
            .target_node_or_component = loop_pair,
            .actionable_remediation = "Insert series internal resistance or eliminate duplicate parallel source."
        });
    }

    return issues;
}

// =============================================================================
// ELEC-06: Modified Nodal Analysis (MNA) Linear System Assembly
// =============================================================================

DenseMatrix::DenseMatrix(size_t rows, size_t cols)
    : rows_(rows), cols_(cols), data_(rows * cols, 0.0) {}

void DenseMatrix::resize(size_t rows, size_t cols) {
    rows_ = rows;
    cols_ = cols;
    data_.assign(rows * cols, 0.0);
}

void DenseMatrix::clear() noexcept {
    std::fill(data_.begin(), data_.end(), 0.0);
}

base::Result<std::vector<double>> DenseMatrix::solve_lu(const std::vector<double>& b) const {
    if (rows_ != cols_ || rows_ != b.size()) {
        return base::Result<std::vector<double>>::err(make_circuit_error("Matrix dimensions incompatible for LU solve"));
    }

    size_t n = rows_;
    if (n == 0) return base::Result<std::vector<double>>::ok({});

    // Copy A and b
    DenseMatrix A = *this;
    std::vector<double> x = b;
    std::vector<size_t> p(n);
    for (size_t i = 0; i < n; ++i) p[i] = i;

    // Gaussian elimination with partial pivoting
    for (size_t k = 0; k < n; ++k) {
        double max_val = std::abs(A(k, k));
        size_t pivot_row = k;

        for (size_t r = k + 1; r < n; ++r) {
            double v = std::abs(A(r, k));
            if (v > max_val) {
                max_val = v;
                pivot_row = r;
            }
        }

        if (max_val < 1e-13) {
            return base::Result<std::vector<double>>::err(make_circuit_error("Singular matrix encountered during LU decomposition", 5002));
        }

        if (pivot_row != k) {
            // Swap rows in A and RHS
            for (size_t c = 0; c < n; ++c) {
                std::swap(A(k, c), A(pivot_row, c));
            }
            std::swap(x[k], x[pivot_row]);
            std::swap(p[k], p[pivot_row]);
        }

        for (size_t r = k + 1; r < n; ++r) {
            double factor = A(r, k) / A(k, k);
            A(r, k) = factor;
            for (size_t c = k + 1; c < n; ++c) {
                A(r, c) -= factor * A(k, c);
            }
            x[r] -= factor * x[k];
        }
    }

    // Back substitution
    for (int r = static_cast<int>(n) - 1; r >= 0; --r) {
        double sum = x[r];
        for (size_t c = r + 1; c < n; ++c) {
            sum -= A(r, c) * x[c];
        }
        x[r] = sum / A(r, r);
    }

    return base::Result<std::vector<double>>::ok(std::move(x));
}

base::Result<MnaSystem> MnaAssembler::assemble_dc_system(const CircuitNetlist& netlist) {
    MnaSystem sys;

    // Number of non-ground nodes N
    size_t num_nodes = 0;
    for (const auto& [name, node] : netlist.nodes()) {
        if (!node.is_ground) num_nodes++;
    }

    // Identify branches requiring auxiliary variables:
    // Independent Voltage Sources, VCVS, and Inductors (DC short circuits)
    size_t num_branches = 0;
    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourceAC ||
            comp.type == ElectricalComponentType::VoltageSourcePulse ||
            comp.type == ElectricalComponentType::VCVS ||
            comp.type == ElectricalComponentType::Inductor) {
            sys.vsource_branch_map[comp.id] = num_branches++;
        }
    }

    sys.num_nodes = num_nodes;
    sys.num_branches = num_branches;
    size_t total_size = num_nodes + num_branches;

    sys.A.resize(total_size, total_size);
    sys.z.assign(total_size, 0.0);

    // Helper for 1-based node index to 0-based matrix row/col
    auto stamp_conductance = [&sys](uint32_t n_pos, uint32_t n_neg, double g) {
        if (n_pos > 0) sys.A.add(n_pos - 1, n_pos - 1, g);
        if (n_neg > 0) sys.A.add(n_neg - 1, n_neg - 1, g);
        if (n_pos > 0 && n_neg > 0) {
            sys.A.add(n_pos - 1, n_neg - 1, -g);
            sys.A.add(n_neg - 1, n_pos - 1, -g);
        }
    };

    for (const auto& comp : netlist.components()) {
        if (comp.pins.size() < 2) continue;
        uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
        uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

        switch (comp.type) {
            case ElectricalComponentType::Resistor: {
                double r = std::max(comp.value_primary, 1e-12);
                stamp_conductance(p, q, 1.0 / r);
                break;
            }
            case ElectricalComponentType::Capacitor: {
                // DC open circuit (zero conductance)
                break;
            }
            case ElectricalComponentType::Inductor: {
                // DC short circuit: ideal 0V voltage source branch
                size_t branch_idx = sys.vsource_branch_map[comp.id];
                size_t row = num_nodes + branch_idx;
                if (p > 0) {
                    sys.A.add(p - 1, row, 1.0);
                    sys.A.add(row, p - 1, 1.0);
                }
                if (q > 0) {
                    sys.A.add(q - 1, row, -1.0);
                    sys.A.add(row, q - 1, -1.0);
                }
                sys.z[row] = 0.0; // 0V drop
                break;
            }
            case ElectricalComponentType::VoltageSourceDC: {
                size_t branch_idx = sys.vsource_branch_map[comp.id];
                size_t row = num_nodes + branch_idx;
                if (p > 0) {
                    sys.A.add(p - 1, row, 1.0);
                    sys.A.add(row, p - 1, 1.0);
                }
                if (q > 0) {
                    sys.A.add(q - 1, row, -1.0);
                    sys.A.add(row, q - 1, -1.0);
                }
                sys.z[row] = comp.value_primary;
                break;
            }
            case ElectricalComponentType::CurrentSourceDC: {
                // Injects current from p to q
                double i_val = comp.value_primary;
                if (p > 0) sys.z[p - 1] -= i_val;
                if (q > 0) sys.z[q - 1] += i_val;
                break;
            }
            case ElectricalComponentType::VCVS: {
                size_t branch_idx = sys.vsource_branch_map[comp.id];
                size_t row = num_nodes + branch_idx;
                if (p > 0) {
                    sys.A.add(p - 1, row, 1.0);
                    sys.A.add(row, p - 1, 1.0);
                }
                if (q > 0) {
                    sys.A.add(q - 1, row, -1.0);
                    sys.A.add(row, q - 1, -1.0);
                }
                uint32_t cp = netlist.get_node_index(comp.ctrl_node_pos);
                uint32_t cq = netlist.get_node_index(comp.ctrl_node_neg);
                double gain = comp.value_primary;
                if (cp > 0) sys.A.add(row, cp - 1, -gain);
                if (cq > 0) sys.A.add(row, cq - 1, gain);
                break;
            }
            case ElectricalComponentType::VCCS: {
                uint32_t cp = netlist.get_node_index(comp.ctrl_node_pos);
                uint32_t cq = netlist.get_node_index(comp.ctrl_node_neg);
                double gm = comp.value_primary;
                if (p > 0) {
                    if (cp > 0) sys.A.add(p - 1, cp - 1, gm);
                    if (cq > 0) sys.A.add(p - 1, cq - 1, -gm);
                }
                if (q > 0) {
                    if (cp > 0) sys.A.add(q - 1, cp - 1, -gm);
                    if (cq > 0) sys.A.add(q - 1, cq - 1, gm);
                }
                break;
            }
            default:
                break;
        }
    }

    return base::Result<MnaSystem>::ok(std::move(sys));
}

base::Result<MnaSystem> MnaAssembler::assemble_transient_step(
    const CircuitNetlist& netlist,
    double t, double dt,
    const std::unordered_map<std::string, double>& cap_voltages_prev,
    const std::unordered_map<std::string, double>& ind_currents_prev,
    bool use_trapezoidal) {

    MnaSystem sys;

    size_t num_nodes = 0;
    for (const auto& [name, node] : netlist.nodes()) {
        if (!node.is_ground) num_nodes++;
    }

    // In transient analysis, voltage sources and inductors (when formulated with branches)
    size_t num_branches = 0;
    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourceAC ||
            comp.type == ElectricalComponentType::VoltageSourcePulse ||
            comp.type == ElectricalComponentType::VCVS) {
            sys.vsource_branch_map[comp.id] = num_branches++;
        }
    }

    sys.num_nodes = num_nodes;
    sys.num_branches = num_branches;
    size_t total_size = num_nodes + num_branches;

    sys.A.resize(total_size, total_size);
    sys.z.assign(total_size, 0.0);

    auto stamp_conductance = [&sys](uint32_t n_pos, uint32_t n_neg, double g) {
        if (n_pos > 0) sys.A.add(n_pos - 1, n_pos - 1, g);
        if (n_neg > 0) sys.A.add(n_neg - 1, n_neg - 1, g);
        if (n_pos > 0 && n_neg > 0) {
            sys.A.add(n_pos - 1, n_neg - 1, -g);
            sys.A.add(n_neg - 1, n_pos - 1, -g);
        }
    };

    for (const auto& comp : netlist.components()) {
        if (comp.pins.size() < 2) continue;
        uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
        uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

        switch (comp.type) {
            case ElectricalComponentType::Resistor: {
                double r = std::max(comp.value_primary, 1e-12);
                stamp_conductance(p, q, 1.0 / r);
                break;
            }
            case ElectricalComponentType::Capacitor: {
                double c = std::max(comp.value_primary, 1e-18);
                double g_eq = use_trapezoidal ? (2.0 * c / dt) : (c / dt);
                stamp_conductance(p, q, g_eq);

                double v_prev = 0.0;
                auto it = cap_voltages_prev.find(comp.id);
                if (it != cap_voltages_prev.end()) v_prev = it->second;

                double i_eq = g_eq * v_prev;
                if (p > 0) sys.z[p - 1] += i_eq;
                if (q > 0) sys.z[q - 1] -= i_eq;
                break;
            }
            case ElectricalComponentType::Inductor: {
                double l = std::max(comp.value_primary, 1e-18);
                double g_eq = use_trapezoidal ? (dt / (2.0 * l)) : (dt / l);
                stamp_conductance(p, q, g_eq);

                double i_prev = 0.0;
                auto it = ind_currents_prev.find(comp.id);
                if (it != ind_currents_prev.end()) i_prev = it->second;

                if (p > 0) sys.z[p - 1] -= i_prev;
                if (q > 0) sys.z[q - 1] += i_prev;
                break;
            }
            case ElectricalComponentType::VoltageSourceDC:
            case ElectricalComponentType::VoltageSourceAC:
            case ElectricalComponentType::VoltageSourcePulse: {
                size_t branch_idx = sys.vsource_branch_map[comp.id];
                size_t row = num_nodes + branch_idx;
                if (p > 0) {
                    sys.A.add(p - 1, row, 1.0);
                    sys.A.add(row, p - 1, 1.0);
                }
                if (q > 0) {
                    sys.A.add(q - 1, row, -1.0);
                    sys.A.add(row, q - 1, -1.0);
                }

                double val = comp.value_primary;
                if (comp.type == ElectricalComponentType::VoltageSourceAC) {
                    val = comp.waveform.evaluate_sine(t);
                } else if (comp.type == ElectricalComponentType::VoltageSourcePulse) {
                    val = comp.waveform.evaluate_pulse(t);
                }
                sys.z[row] = val;
                break;
            }
            case ElectricalComponentType::CurrentSourceDC:
            case ElectricalComponentType::CurrentSourceAC: {
                double val = comp.value_primary;
                if (comp.type == ElectricalComponentType::CurrentSourceAC) {
                    val = comp.waveform.evaluate_sine(t);
                }
                if (p > 0) sys.z[p - 1] -= val;
                if (q > 0) sys.z[q - 1] += val;
                break;
            }
            default:
                break;
        }
    }

    return base::Result<MnaSystem>::ok(std::move(sys));
}

// =============================================================================
// ELEC-07: DC Operating Point Analysis
// =============================================================================

double DCOperatingPointResult::get_voltage(std::string_view node_name) const noexcept {
    if (node_name == "0" || node_name == "GND" || node_name == "gnd") return 0.0;
    auto it = node_voltages.find(std::string(node_name));
    if (it != node_voltages.end()) return it->second;
    return 0.0;
}

double DCOperatingPointResult::get_current(std::string_view comp_id) const noexcept {
    auto it = branch_currents.find(std::string(comp_id));
    if (it != branch_currents.end()) return it->second;
    return 0.0;
}

double DCOperatingPointResult::get_power(std::string_view comp_id) const noexcept {
    auto it = component_power.find(std::string(comp_id));
    if (it != component_power.end()) return it->second;
    return 0.0;
}

base::Result<DCOperatingPointResult> DcOperatingPointSolver::solve(const CircuitNetlist& netlist) {
    auto mna_res = MnaAssembler::assemble_dc_system(netlist);
    if (!mna_res.is_ok()) {
        return base::Result<DCOperatingPointResult>::err(mna_res.error());
    }

    auto base_sys = mna_res.value();
    size_t num_nodes = base_sys.num_nodes;
    size_t total_size = num_nodes + base_sys.num_branches;

    // Check if circuit contains non-linear elements (Diodes, BJTs, MOSFETs)
    bool has_nonlinear = false;
    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::Diode ||
            comp.type == ElectricalComponentType::TransistorBJT ||
            comp.type == ElectricalComponentType::TransistorMOSFET) {
            has_nonlinear = true;
            break;
        }
    }

    std::vector<double> x(total_size, 0.0);

    if (!has_nonlinear) {
        // Pure linear DC operating point
        // Add tiny Gmin shunt to ground for diagonal stability
        for (size_t i = 0; i < num_nodes; ++i) {
            base_sys.A.add(i, i, gmin);
        }

        auto solve_res = base_sys.A.solve_lu(base_sys.z);
        if (!solve_res.is_ok()) {
            return base::Result<DCOperatingPointResult>::err(solve_res.error());
        }
        x = solve_res.value();
    } else {
        // Newton-Raphson iteration loop
        bool converged = false;
        std::vector<double> prev_x(total_size, 0.0);

        for (uint32_t iter = 0; iter < max_iterations; ++iter) {
            MnaSystem sys = base_sys;

            // Stamp Gmin
            for (size_t i = 0; i < num_nodes; ++i) {
                sys.A.add(i, i, gmin);
            }

            // Stamp nonlinear elements at current operating point guess
            for (const auto& comp : netlist.components()) {
                if (comp.pins.size() < 2) continue;
                uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
                uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

                double vp = (p > 0) ? x[p - 1] : 0.0;
                double vq = (q > 0) ? x[q - 1] : 0.0;

                if (comp.type == ElectricalComponentType::Diode) {
                    double vd = vp - vq;
                    double id = 0.0, gd = 0.0;
                    NonLinearDeviceState::evaluate_diode(
                        vd, comp.is_saturation_current, comp.thermal_voltage, comp.ideality_factor, id, gd);

                    // Stamp gd
                    if (p > 0) sys.A.add(p - 1, p - 1, gd);
                    if (q > 0) sys.A.add(q - 1, q - 1, gd);
                    if (p > 0 && q > 0) {
                        sys.A.add(p - 1, q - 1, -gd);
                        sys.A.add(q - 1, p - 1, -gd);
                    }

                    // Stamp companion current source: Ieq = Id - gd * Vd
                    double i_eq = id - gd * vd;
                    if (p > 0) sys.z[p - 1] -= i_eq;
                    if (q > 0) sys.z[q - 1] += i_eq;
                } else if (comp.type == ElectricalComponentType::TransistorMOSFET) {
                    // Pins: 0 = Drain, 1 = Gate, 2 = Source
                    if (comp.pins.size() >= 3) {
                        uint32_t d = p;
                        uint32_t g = q;
                        uint32_t s = netlist.get_node_index(comp.pins[2].connected_net);

                        double vd_val = (d > 0) ? x[d - 1] : 0.0;
                        double vg_val = (g > 0) ? x[g - 1] : 0.0;
                        double vs_val = (s > 0) ? x[s - 1] : 0.0;

                        double vgs = vg_val - vs_val;
                        double vds = vd_val - vs_val;
                        double ids = 0.0, gm = 0.0, gds = 0.0;
                        NonLinearDeviceState::evaluate_mosfet_nmos(
                            vgs, vds, comp.mos_vth, comp.mos_kp, comp.mos_lambda, ids, gm, gds);

                        // Linear companion stamp: Ids = gm * vgs + gds * vds + Ieq
                        double i_eq = ids - gm * vgs - gds * vds;
                        if (d > 0) sys.z[d - 1] -= i_eq;
                        if (s > 0) sys.z[s - 1] += i_eq;

                        // gm stamps: d receives +gm(vg - vs), s receives -gm(vg - vs)
                        if (d > 0) {
                            if (g > 0) sys.A.add(d - 1, g - 1, gm);
                            if (s > 0) sys.A.add(d - 1, s - 1, -gm);
                        }
                        if (s > 0) {
                            if (g > 0) sys.A.add(s - 1, g - 1, -gm);
                            if (s > 0) sys.A.add(s - 1, s - 1, gm);
                        }

                        // gds stamps: across d and s
                        if (d > 0) sys.A.add(d - 1, d - 1, gds);
                        if (s > 0) sys.A.add(s - 1, s - 1, gds);
                        if (d > 0 && s > 0) {
                            sys.A.add(d - 1, s - 1, -gds);
                            sys.A.add(s - 1, d - 1, -gds);
                        }
                    }
                }
            }

            auto solve_res = sys.A.solve_lu(sys.z);
            if (!solve_res.is_ok()) {
                return base::Result<DCOperatingPointResult>::err(solve_res.error());
            }

            auto next_x = solve_res.value();
            double max_err = 0.0;

            for (size_t i = 0; i < total_size; ++i) {
                double diff = std::abs(next_x[i] - x[i]);
                max_err = std::max(max_err, diff);
                // Damping for voltage variables to prevent exponential divergence
                if (i < num_nodes) {
                    double delta = next_x[i] - x[i];
                    double damped_delta = std::clamp(delta, -0.2, 0.2);
                    x[i] += damped_delta;
                } else {
                    x[i] = next_x[i];
                }
            }

            if (max_err < voltage_tolerance) {
                converged = true;
                break;
            }
        }

        if (!converged) {
            return base::Result<DCOperatingPointResult>::err(
                make_circuit_error("Newton-Raphson iteration failed to converge within maximum iteration limit", 5003));
        }
    }

    // Populate DCOperatingPointResult
    DCOperatingPointResult res;
    res.converged = true;

    // Node voltages
    for (const auto& [name, node] : netlist.nodes()) {
        if (node.is_ground) {
            res.node_voltages[name] = 0.0;
        } else {
            res.node_voltages[name] = x[node.index - 1];
        }
    }

    // Branch currents & Component powers
    for (const auto& comp : netlist.components()) {
        if (comp.pins.size() < 2) continue;
        double vp = res.get_voltage(comp.pins[0].connected_net);
        double vq = res.get_voltage(comp.pins[1].connected_net);
        double v_drop = vp - vq;
        double i_comp = 0.0;

        switch (comp.type) {
            case ElectricalComponentType::Resistor:
                i_comp = v_drop / std::max(comp.value_primary, 1e-12);
                break;
            case ElectricalComponentType::Capacitor:
                i_comp = 0.0;
                break;
            case ElectricalComponentType::Inductor:
            case ElectricalComponentType::VoltageSourceDC: {
                size_t branch_idx = base_sys.vsource_branch_map[comp.id];
                i_comp = x[num_nodes + branch_idx];
                break;
            }
            case ElectricalComponentType::CurrentSourceDC:
                i_comp = comp.value_primary;
                break;
            case ElectricalComponentType::Diode: {
                double gd = 0.0;
                NonLinearDeviceState::evaluate_diode(
                    v_drop, comp.is_saturation_current, comp.thermal_voltage, comp.ideality_factor, i_comp, gd);
                break;
            }
            default:
                break;
        }

        res.branch_currents[comp.id] = i_comp;
        res.component_power[comp.id] = std::abs(v_drop * i_comp);
    }

    return base::Result<DCOperatingPointResult>::ok(std::move(res));
}

// =============================================================================
// ELEC-08: Transient Analysis
// =============================================================================

const std::vector<double>* TransientResult::get_voltage_trace(std::string_view node_name) const noexcept {
    auto it = node_voltages.find(std::string(node_name));
    if (it != node_voltages.end()) return &it->second;
    return nullptr;
}

const std::vector<double>* TransientResult::get_current_trace(std::string_view comp_id) const noexcept {
    auto it = branch_currents.find(std::string(comp_id));
    if (it != branch_currents.end()) return &it->second;
    return nullptr;
}

base::Result<TransientResult> TransientSolver::solve(const CircuitNetlist& netlist) {
    TransientResult res;

    // Check DC initial operating point if requested
    std::unordered_map<std::string, double> cap_voltages;
    std::unordered_map<std::string, double> ind_currents;

    if (use_dc_op_initial_state) {
        DcOperatingPointSolver dc_solver;
        auto dc_res = dc_solver.solve(netlist);
        if (dc_res.is_ok()) {
            for (const auto& comp : netlist.components()) {
                if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
                    double vp = dc_res.value().get_voltage(comp.pins[0].connected_net);
                    double vq = dc_res.value().get_voltage(comp.pins[1].connected_net);
                    cap_voltages[comp.id] = vp - vq;
                } else if (comp.type == ElectricalComponentType::Inductor) {
                    ind_currents[comp.id] = dc_res.value().get_current(comp.id);
                }
            }
        }
    }

    // Initialize initial condition overrides if specified
    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::Capacitor && comp.value_secondary != 0.0) {
            cap_voltages[comp.id] = comp.value_secondary;
        } else if (comp.type == ElectricalComponentType::Inductor && comp.value_secondary != 0.0) {
            ind_currents[comp.id] = comp.value_secondary;
        }
    }

    uint32_t num_steps = static_cast<uint32_t>(std::ceil(t_stop / time_step));
    bool use_trap = (method == TransientIntegrationMethod::Trapezoidal);

    res.time_points.reserve(num_steps + 1);

    // Initial state t = 0
    res.time_points.push_back(0.0);
    for (const auto& [name, node] : netlist.nodes()) {
        double v_init = 0.0;
        if (!node.is_ground) {
            for (const auto& comp : netlist.components()) {
                if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
                    if (comp.pins[0].connected_net == name) {
                        v_init = cap_voltages[comp.id];
                    }
                }
            }
        }
        res.node_voltages[name].push_back(v_init);
    }

    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::Capacitor) {
            res.branch_currents[comp.id].push_back(0.0);
        } else if (comp.type == ElectricalComponentType::Inductor) {
            res.branch_currents[comp.id].push_back(ind_currents[comp.id]);
        }
    }

    for (uint32_t step = 1; step <= num_steps; ++step) {
        double t = step * time_step;
        res.time_points.push_back(t);

        auto mna_res = MnaAssembler::assemble_transient_step(
            netlist, t, time_step, cap_voltages, ind_currents, use_trap);

        if (!mna_res.is_ok()) {
            return base::Result<TransientResult>::err(mna_res.error());
        }

        auto sys = mna_res.value();
        // Add tiny Gmin
        for (size_t i = 0; i < sys.num_nodes; ++i) {
            sys.A.add(i, i, 1e-12);
        }

        auto solve_res = sys.A.solve_lu(sys.z);
        if (!solve_res.is_ok()) {
            return base::Result<TransientResult>::err(solve_res.error());
        }

        auto x = solve_res.value();

        // Record node voltages
        for (const auto& [name, node] : netlist.nodes()) {
            double v = node.is_ground ? 0.0 : x[node.index - 1];
            res.node_voltages[name].push_back(v);
        }

        // Update state variables (Capacitor voltages and Inductor currents)
        for (const auto& comp : netlist.components()) {
            if (comp.pins.size() < 2) continue;
            uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
            uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

            double vp = (p > 0) ? x[p - 1] : 0.0;
            double vq = (q > 0) ? x[q - 1] : 0.0;
            double v_drop = vp - vq;

            if (comp.type == ElectricalComponentType::Capacitor) {
                if (use_trap) {
                    double v_old = cap_voltages[comp.id];
                    double c = comp.value_primary;
                    double g_eq = 2.0 * c / time_step;
                    double i_cap = g_eq * (v_drop - v_old);
                    res.branch_currents[comp.id].push_back(i_cap);
                }
                cap_voltages[comp.id] = v_drop;
            } else if (comp.type == ElectricalComponentType::Inductor) {
                double l = comp.value_primary;
                double i_old = ind_currents[comp.id];
                double di = (v_drop / l) * time_step;
                ind_currents[comp.id] = i_old + di;
                res.branch_currents[comp.id].push_back(ind_currents[comp.id]);
            } else if (comp.type == ElectricalComponentType::Resistor) {
                res.branch_currents[comp.id].push_back(v_drop / std::max(comp.value_primary, 1e-12));
            } else if (comp.type == ElectricalComponentType::VoltageSourceDC ||
                       comp.type == ElectricalComponentType::VoltageSourceAC ||
                       comp.type == ElectricalComponentType::VoltageSourcePulse) {
                size_t branch_idx = sys.vsource_branch_map[comp.id];
                res.branch_currents[comp.id].push_back(x[sys.num_nodes + branch_idx]);
            }
        }
    }

    res.success = true;
    return base::Result<TransientResult>::ok(std::move(res));
}

// =============================================================================
// ELEC-09 & ELEC-10: Non-linear Diode & Transistor Newton-Raphson Models
// =============================================================================

void NonLinearDeviceState::evaluate_diode(
    double v_diode, double is, double vt, double n,
    double& out_current, double& out_conductance) noexcept {

    double v_scaled = v_diode / (n * vt);
    // Limit exponential argument to prevent numerical float overflow
    if (v_scaled > 40.0) v_scaled = 40.0;

    if (v_scaled < -10.0) {
        // Severe reverse bias
        out_current = -is;
        out_conductance = 1e-12;
    } else {
        double exp_val = std::exp(v_scaled);
        out_current = is * (exp_val - 1.0);
        out_conductance = (is / (n * vt)) * exp_val;
        out_conductance = std::max(out_conductance, 1e-12);
    }
}

void NonLinearDeviceState::evaluate_bjt_npn(
    double v_be, double v_ce, double is, double vt, double beta,
    double& out_ib, double& out_ic, double& out_gbe, double& out_gm, double& out_gce) noexcept {

    double ib = 0.0, gbe = 0.0;
    evaluate_diode(v_be, is / beta, vt, 1.0, ib, gbe);

    if (v_ce > 0.2) {
        // Forward active region
        out_ib = ib;
        out_ic = beta * ib;
        out_gbe = gbe;
        out_gm = beta * gbe;
        out_gce = 1e-5; // Early effect parameter 1 / Va
    } else {
        // Saturation region
        out_ib = ib;
        out_ic = std::max(0.0, (v_ce / 0.2) * (beta * ib));
        out_gbe = gbe;
        out_gm = 0.5 * beta * gbe;
        out_gce = (beta * ib) / 0.2;
    }
}

void NonLinearDeviceState::evaluate_mosfet_nmos(
    double v_gs, double v_ds, double vth, double kp, double lambda,
    double& out_ids, double& out_gm, double& out_gds) noexcept {

    double v_gst = v_gs - vth;

    if (v_gst <= 0.0) {
        // Cutoff
        out_ids = 0.0;
        out_gm = 0.0;
        out_gds = 1e-12;
    } else if (v_ds < v_gst) {
        // Linear / Triode region
        out_ids = kp * ((v_gst * v_ds) - 0.5 * v_ds * v_ds) * (1.0 + lambda * v_ds);
        out_gm = kp * v_ds * (1.0 + lambda * v_ds);
        out_gds = kp * (v_gst - v_ds) * (1.0 + lambda * v_ds) + lambda * out_ids;
    } else {
        // Saturation region
        double v_gst2 = v_gst * v_gst;
        out_ids = 0.5 * kp * v_gst2 * (1.0 + lambda * v_ds);
        out_gm = kp * v_gst * (1.0 + lambda * v_ds);
        out_gds = lambda * 0.5 * kp * v_gst2;
    }
    out_gds = std::max(out_gds, 1e-12);
}

// =============================================================================
// ELEC-11: AC Small-Signal Frequency Response Analysis
// =============================================================================

ComplexDenseMatrix::ComplexDenseMatrix(size_t rows, size_t cols)
    : rows_(rows), cols_(cols), data_(rows * cols, {0.0, 0.0}) {}

void ComplexDenseMatrix::resize(size_t rows, size_t cols) {
    rows_ = rows;
    cols_ = cols;
    data_.assign(rows * cols, {0.0, 0.0});
}

void ComplexDenseMatrix::clear() noexcept {
    std::fill(data_.begin(), data_.end(), std::complex<double>{0.0, 0.0});
}

base::Result<std::vector<std::complex<double>>> ComplexDenseMatrix::solve_lu(
    const std::vector<std::complex<double>>& b) const {

    size_t n = rows_;
    if (n == 0) return base::Result<std::vector<std::complex<double>>>::ok({});

    ComplexDenseMatrix A = *this;
    std::vector<std::complex<double>> x = b;

    for (size_t k = 0; k < n; ++k) {
        double max_val = std::abs(A(k, k));
        size_t pivot_row = k;

        for (size_t r = k + 1; r < n; ++r) {
            double v = std::abs(A(r, k));
            if (v > max_val) {
                max_val = v;
                pivot_row = r;
            }
        }

        if (max_val < 1e-13) {
            return base::Result<std::vector<std::complex<double>>>::err(
                make_circuit_error("Singular complex matrix in AC solve", 5004));
        }

        if (pivot_row != k) {
            for (size_t c = 0; c < n; ++c) {
                std::swap(A(k, c), A(pivot_row, c));
            }
            std::swap(x[k], x[pivot_row]);
        }

        for (size_t r = k + 1; r < n; ++r) {
            std::complex<double> factor = A(r, k) / A(k, k);
            A(r, k) = factor;
            for (size_t c = k + 1; c < n; ++c) {
                A(r, c) -= factor * A(k, c);
            }
            x[r] -= factor * x[k];
        }
    }

    // Back substitution
    for (int r = static_cast<int>(n) - 1; r >= 0; --r) {
        std::complex<double> sum = x[r];
        for (size_t c = r + 1; c < n; ++c) {
            sum -= A(r, c) * x[c];
        }
        x[r] = sum / A(r, r);
    }

    return base::Result<std::vector<std::complex<double>>>::ok(std::move(x));
}

base::Result<AcAnalysisResult> AcSmallSignalSolver::solve(
    const CircuitNetlist& netlist,
    const DCOperatingPointResult&) {

    AcAnalysisResult res;

    size_t num_nodes = 0;
    for (const auto& [name, node] : netlist.nodes()) {
        if (!node.is_ground) num_nodes++;
    }

    // Find input voltage source
    size_t num_branches = 0;
    std::unordered_map<std::string, size_t> branch_map;
    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourceAC) {
            branch_map[comp.id] = num_branches++;
        }
    }

    size_t total_size = num_nodes + num_branches;

    double dec = std::log10(f_stop_hz / f_start_hz);
    uint32_t total_points = static_cast<uint32_t>(dec * points_per_decade) + 1;

    res.frequencies.reserve(total_points);
    res.bode_points.reserve(total_points);

    for (uint32_t pt = 0; pt < total_points; ++pt) {
        double f = f_start_hz * std::pow(10.0, (dec * pt) / std::max(1u, total_points - 1));
        double omega = 2.0 * PI * f;

        ComplexDenseMatrix Y(total_size, total_size);
        std::vector<std::complex<double>> I_vec(total_size, {0.0, 0.0});

        auto stamp_admittance = [&Y](uint32_t p, uint32_t q, std::complex<double> y) {
            if (p > 0) Y.add(p - 1, p - 1, y);
            if (q > 0) Y.add(q - 1, q - 1, y);
            if (p > 0 && q > 0) {
                Y.add(p - 1, q - 1, -y);
                Y.add(q - 1, p - 1, -y);
            }
        };

        for (const auto& comp : netlist.components()) {
            if (comp.pins.size() < 2) continue;
            uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
            uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

            if (comp.type == ElectricalComponentType::Resistor) {
                stamp_admittance(p, q, {1.0 / std::max(comp.value_primary, 1e-12), 0.0});
            } else if (comp.type == ElectricalComponentType::Capacitor) {
                std::complex<double> jwc{0.0, omega * comp.value_primary};
                stamp_admittance(p, q, jwc);
            } else if (comp.type == ElectricalComponentType::Inductor) {
                std::complex<double> jwl{0.0, omega * comp.value_primary};
                stamp_admittance(p, q, 1.0 / jwl);
            } else if (comp.type == ElectricalComponentType::VoltageSourceDC ||
                       comp.type == ElectricalComponentType::VoltageSourceAC) {
                size_t branch_idx = branch_map[comp.id];
                size_t row = num_nodes + branch_idx;
                if (p > 0) {
                    Y.add(p - 1, row, {1.0, 0.0});
                    Y.add(row, p - 1, {1.0, 0.0});
                }
                if (q > 0) {
                    Y.add(q - 1, row, {-1.0, 0.0});
                    Y.add(row, q - 1, {-1.0, 0.0});
                }

                if (comp.id == input_source_id) {
                    I_vec[row] = {1.0, 0.0}; // 1V AC stimulus
                }
            }
        }

        // Tiny Gmin
        for (size_t i = 0; i < num_nodes; ++i) {
            Y.add(i, i, {1e-12, 0.0});
        }

        auto solve_res = Y.solve_lu(I_vec);
        if (!solve_res.is_ok()) {
            return base::Result<AcAnalysisResult>::err(solve_res.error());
        }

        auto x = solve_res.value();
        uint32_t out_idx = netlist.get_node_index(output_node);
        std::complex<double> v_out = (out_idx > 0) ? x[out_idx - 1] : std::complex<double>{0.0, 0.0};

        double mag = std::abs(v_out);
        double mag_db = (mag > 1e-12) ? (20.0 * std::log10(mag)) : -240.0;
        double phase = std::arg(v_out) * 180.0 / PI;

        res.frequencies.push_back(f);
        res.bode_points.push_back(AcFrequencyPoint{
            .frequency_hz = f,
            .magnitude_gain_db = mag_db,
            .phase_deg = phase,
            .complex_gain = v_out
        });
    }

    res.success = true;
    return base::Result<AcAnalysisResult>::ok(std::move(res));
}

// =============================================================================
// ELEC-12: Circuit Probes & Waveform Computations
// =============================================================================

std::vector<double> ProbeMeasurementEngine::evaluate_probe_trace(
    const CircuitProbe& probe,
    const TransientResult& transient) {

    std::vector<double> trace;
    size_t count = transient.time_points.size();
    if (count == 0) return trace;
    trace.resize(count, 0.0);

    if (probe.type == ProbeType::NodeVoltage) {
        auto p_trace = transient.get_voltage_trace(probe.node_pos);
        if (p_trace) trace = *p_trace;
    } else if (probe.type == ProbeType::DifferentialVoltage) {
        auto p_trace = transient.get_voltage_trace(probe.node_pos);
        auto n_trace = transient.get_voltage_trace(probe.node_neg);
        for (size_t i = 0; i < count; ++i) {
            double vp = p_trace ? (*p_trace)[i] : 0.0;
            double vn = n_trace ? (*n_trace)[i] : 0.0;
            trace[i] = vp - vn;
        }
    } else if (probe.type == ProbeType::BranchCurrent) {
        auto c_trace = transient.get_current_trace(probe.component_id);
        if (c_trace) trace = *c_trace;
    }

    return trace;
}

double ProbeMeasurementEngine::compute_average_value(const std::vector<double>& trace) noexcept {
    if (trace.empty()) return 0.0;
    double sum = 0.0;
    for (double v : trace) sum += v;
    return sum / static_cast<double>(trace.size());
}

double ProbeMeasurementEngine::compute_rms_value(const std::vector<double>& trace) noexcept {
    if (trace.empty()) return 0.0;
    double sum_sq = 0.0;
    for (double v : trace) sum_sq += v * v;
    return std::sqrt(sum_sq / static_cast<double>(trace.size()));
}

double ProbeMeasurementEngine::compute_peak_to_peak(const std::vector<double>& trace) noexcept {
    if (trace.empty()) return 0.0;
    double min_v = trace.front();
    double max_v = trace.front();
    for (double v : trace) {
        min_v = std::min(min_v, v);
        max_v = std::max(max_v, v);
    }
    return max_v - min_v;
}

double ProbeMeasurementEngine::compute_total_energy(
    const std::vector<double>& time_trace,
    const std::vector<double>& power_trace) noexcept {

    if (time_trace.size() < 2 || time_trace.size() != power_trace.size()) return 0.0;
    double energy = 0.0;
    // Trapezoidal numerical integration of P(t) dt
    for (size_t i = 0; i < time_trace.size() - 1; ++i) {
        double dt = time_trace[i + 1] - time_trace[i];
        double p_avg = 0.5 * (power_trace[i] + power_trace[i + 1]);
        energy += p_avg * dt;
    }
    return energy;
}

// =============================================================================
// ELEC-14: Comprehensive Circuit Validation Benchmarks
// =============================================================================

CircuitNetlist CircuitBenchmarkFactory::create_voltage_divider(double vin, double r1, double r2) {
    CircuitNetlist netlist;

    // V1 from in to 0
    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = vin;
    v1.pins = {
        CircuitPin::make("V1_p", "in", ElectricalPinPolarity::Positive, "V1", "+"),
        CircuitPin::make("V1_n", "0", ElectricalPinPolarity::Negative, "V1", "-")
    };
    netlist.add_component(v1);

    // R1 from in to out
    ElectricalComponent r1_comp;
    r1_comp.id = "R1";
    r1_comp.type = ElectricalComponentType::Resistor;
    r1_comp.value_primary = r1;
    r1_comp.pins = {
        CircuitPin::make("R1_p", "in", ElectricalPinPolarity::PassiveBiDirectional, "R1", "1"),
        CircuitPin::make("R1_n", "out", ElectricalPinPolarity::PassiveBiDirectional, "R1", "2")
    };
    netlist.add_component(r1_comp);

    // R2 from out to 0
    ElectricalComponent r2_comp;
    r2_comp.id = "R2";
    r2_comp.type = ElectricalComponentType::Resistor;
    r2_comp.value_primary = r2;
    r2_comp.pins = {
        CircuitPin::make("R2_p", "out", ElectricalPinPolarity::PassiveBiDirectional, "R2", "1"),
        CircuitPin::make("R2_n", "0", ElectricalPinPolarity::PassiveBiDirectional, "R2", "2")
    };
    netlist.add_component(r2_comp);

    (void)netlist.build_node_topology();
    return netlist;
}

CircuitNetlist CircuitBenchmarkFactory::create_rc_filter(double v_step, double r, double c) {
    CircuitNetlist netlist;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = v_step;
    v1.pins = {
        CircuitPin::make("V1_p", "in"),
        CircuitPin::make("V1_n", "0")
    };
    netlist.add_component(v1);

    ElectricalComponent r_comp;
    r_comp.id = "R1";
    r_comp.type = ElectricalComponentType::Resistor;
    r_comp.value_primary = r;
    r_comp.pins = {
        CircuitPin::make("R1_p", "in"),
        CircuitPin::make("R1_n", "out")
    };
    netlist.add_component(r_comp);

    ElectricalComponent c_comp;
    c_comp.id = "C1";
    c_comp.type = ElectricalComponentType::Capacitor;
    c_comp.value_primary = c;
    c_comp.value_secondary = 0.0; // Initial condition Vc(0) = 0
    c_comp.pins = {
        CircuitPin::make("C1_p", "out"),
        CircuitPin::make("C1_n", "0")
    };
    netlist.add_component(c_comp);

    (void)netlist.build_node_topology();
    return netlist;
}

CircuitNetlist CircuitBenchmarkFactory::create_rl_filter(double v_step, double r, double l) {
    CircuitNetlist netlist;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = v_step;
    v1.pins = {
        CircuitPin::make("V1_p", "in"),
        CircuitPin::make("V1_n", "0")
    };
    netlist.add_component(v1);

    ElectricalComponent r_comp;
    r_comp.id = "R1";
    r_comp.type = ElectricalComponentType::Resistor;
    r_comp.value_primary = r;
    r_comp.pins = {
        CircuitPin::make("R1_p", "in"),
        CircuitPin::make("R1_n", "mid")
    };
    netlist.add_component(r_comp);

    ElectricalComponent l_comp;
    l_comp.id = "L1";
    l_comp.type = ElectricalComponentType::Inductor;
    l_comp.value_primary = l;
    l_comp.value_secondary = 0.0; // Initial condition Il(0) = 0
    l_comp.pins = {
        CircuitPin::make("L1_p", "mid"),
        CircuitPin::make("L1_n", "0")
    };
    netlist.add_component(l_comp);

    (void)netlist.build_node_topology();
    return netlist;
}

CircuitNetlist CircuitBenchmarkFactory::create_diode_rectifier(double v_ac, double freq, double r_load, double c_filter) {
    CircuitNetlist netlist;

    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceAC;
    v1.value_primary = v_ac;
    v1.waveform.amplitude = v_ac;
    v1.waveform.frequency_hz = freq;
    v1.pins = {
        CircuitPin::make("V1_p", "ac_in"),
        CircuitPin::make("V1_n", "0")
    };
    netlist.add_component(v1);

    ElectricalComponent d1;
    d1.id = "D1";
    d1.type = ElectricalComponentType::Diode;
    d1.pins = {
        CircuitPin::make("D1_a", "ac_in", ElectricalPinPolarity::Anode),
        CircuitPin::make("D1_k", "rect_out", ElectricalPinPolarity::Cathode)
    };
    netlist.add_component(d1);

    ElectricalComponent rl;
    rl.id = "RL";
    rl.type = ElectricalComponentType::Resistor;
    rl.value_primary = r_load;
    rl.pins = {
        CircuitPin::make("RL_p", "rect_out"),
        CircuitPin::make("RL_n", "0")
    };
    netlist.add_component(rl);

    if (c_filter > 0.0) {
        ElectricalComponent cf;
        cf.id = "CF";
        cf.type = ElectricalComponentType::Capacitor;
        cf.value_primary = c_filter;
        cf.pins = {
            CircuitPin::make("CF_p", "rect_out"),
            CircuitPin::make("CF_n", "0")
        };
        netlist.add_component(cf);
    }

    (void)netlist.build_node_topology();
    return netlist;
}

CircuitNetlist CircuitBenchmarkFactory::create_bjt_inverter(double vcc, double rb, double rc) {
    CircuitNetlist netlist;

    ElectricalComponent v_vcc;
    v_vcc.id = "VCC";
    v_vcc.type = ElectricalComponentType::VoltageSourceDC;
    v_vcc.value_primary = vcc;
    v_vcc.pins = {
        CircuitPin::make("VCC_p", "vcc"),
        CircuitPin::make("VCC_n", "0")
    };
    netlist.add_component(v_vcc);

    ElectricalComponent r_c;
    r_c.id = "RC";
    r_c.type = ElectricalComponentType::Resistor;
    r_c.value_primary = rc;
    r_c.pins = {
        CircuitPin::make("RC_p", "vcc"),
        CircuitPin::make("RC_n", "vout")
    };
    netlist.add_component(r_c);

    ElectricalComponent r_b;
    r_b.id = "RB";
    r_b.type = ElectricalComponentType::Resistor;
    r_b.value_primary = rb;
    r_b.pins = {
        CircuitPin::make("RB_p", "vin"),
        CircuitPin::make("RB_n", "base")
    };
    netlist.add_component(r_b);

    (void)netlist.build_node_topology();
    return netlist;
}

CircuitNetlist CircuitBenchmarkFactory::create_nmos_inverter(double vdd, double r_load, double v_in) {
    CircuitNetlist netlist;

    // VDD supply
    ElectricalComponent v_vdd;
    v_vdd.id = "VDD";
    v_vdd.type = ElectricalComponentType::VoltageSourceDC;
    v_vdd.value_primary = vdd;
    v_vdd.pins = {
        CircuitPin::make("VDD_p", "vdd"),
        CircuitPin::make("VDD_n", "0")
    };
    netlist.add_component(v_vdd);

    // VIN input voltage
    ElectricalComponent v_in_comp;
    v_in_comp.id = "VIN";
    v_in_comp.type = ElectricalComponentType::VoltageSourceDC;
    v_in_comp.value_primary = v_in;
    v_in_comp.pins = {
        CircuitPin::make("VIN_p", "vin"),
        CircuitPin::make("VIN_n", "0")
    };
    netlist.add_component(v_in_comp);

    // Load resistor
    ElectricalComponent r_l;
    r_l.id = "RL";
    r_l.type = ElectricalComponentType::Resistor;
    r_l.value_primary = r_load;
    r_l.pins = {
        CircuitPin::make("RL_p", "vdd"),
        CircuitPin::make("RL_n", "vout")
    };
    netlist.add_component(r_l);

    // NMOS transistor (Drain=vout, Gate=vin, Source=0)
    ElectricalComponent nmos;
    nmos.id = "M1";
    nmos.type = ElectricalComponentType::TransistorMOSFET;
    nmos.mos_vth = 1.0;
    nmos.mos_kp = 2e-3;
    nmos.pins = {
        CircuitPin::make("M1_d", "vout", ElectricalPinPolarity::Drain),
        CircuitPin::make("M1_g", "vin", ElectricalPinPolarity::Gate),
        CircuitPin::make("M1_s", "0", ElectricalPinPolarity::Source)
    };
    netlist.add_component(nmos);

    (void)netlist.build_node_topology();
    return netlist;
}

} // namespace shunya::core::domain
