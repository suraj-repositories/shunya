#include "shunya/eng/interactive_circuit.hpp"

#include "shunya/chem/li_ion_soc_tables.hpp"
#include "shunya/pcb/spice_simulation_engine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>

namespace shunya::eng {

namespace {

core::base::Error make_cir_error(int32_t code, std::string msg, std::string details = {}) {
    return core::base::Error{
        .code = code,
        .domain = core::base::ErrorDomain::Simulation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = std::move(details)
    };
}

double point_to_segment_dist(
    const core::base::math::Vector2<double>& p,
    const core::base::math::Vector2<double>& a,
    const core::base::math::Vector2<double>& b) noexcept {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double l2 = dx * dx + dy * dy;
    if (l2 <= 1e-12) return std::hypot(p.x - a.x, p.y - a.y);
    const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

struct DisjointSet {
    std::vector<int> parent;
    explicit DisjointSet(int n) : parent(static_cast<size_t>(n)) {
        std::iota(parent.begin(), parent.end(), 0);
    }
    int find(int x) {
        while (parent[static_cast<size_t>(x)] != x) {
            parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
            x = parent[static_cast<size_t>(x)];
        }
        return x;
    }
    void unite(int a, int b) {
        a = find(a);
        b = find(b);
        if (a != b) parent[static_cast<size_t>(b)] = a;
    }
};

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

std::string json_str(std::string_view json, std::string_view key) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*\"([^\"]*)\"");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) return m[1].str();
    return {};
}

double json_num(std::string_view json, std::string_view key, double fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*([-+0-9eE\\.]+)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        try { return std::stod(m[1].str()); } catch (...) { return fallback; }
    }
    return fallback;
}

bool json_bool(std::string_view json, std::string_view key, bool fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        return m[1].str() == "true";
    }
    return fallback;
}

bool is_two_pin_passive_or_semi(CircuitSymbolKind k) noexcept {
    return k != CircuitSymbolKind::Ground;
}

double compute_kcl_residual(
    const InteractiveCircuitDocument& doc,
    const std::unordered_map<std::string, double>& node_v,
    const std::unordered_map<std::string, double>& comp_i) {
    std::unordered_map<std::string, double> node_sum;
    for (const auto& c : doc.components()) {
        if (c.kind == CircuitSymbolKind::Ground || c.terminal_nets.size() < 2) continue;
        auto it = comp_i.find(c.id);
        if (it == comp_i.end()) continue;
        const double curr = it->second; // positive from terminal[0] -> terminal[1]
        const std::string& n0 = c.terminal_nets[0];
        const std::string& n1 = c.terminal_nets[1];
        if (n0 != "0") node_sum[n0] -= curr;
        if (n1 != "0") node_sum[n1] += curr;
    }
    (void)node_v;
    double max_res = 0.0;
    for (const auto& [net, sum] : node_sum) {
        (void)net;
        max_res = std::max(max_res, std::abs(sum));
    }
    return max_res;
}

} // namespace

core::base::Result<CircuitSymbolKind> circuit_symbol_kind_from_string(std::string_view s) {
    if (s == "VoltageSourceDC") return CircuitSymbolKind::VoltageSourceDC;
    if (s == "VoltageSourceAC") return CircuitSymbolKind::VoltageSourceAC;
    if (s == "Resistor") return CircuitSymbolKind::Resistor;
    if (s == "Capacitor") return CircuitSymbolKind::Capacitor;
    if (s == "Inductor") return CircuitSymbolKind::Inductor;
    if (s == "Diode") return CircuitSymbolKind::Diode;
    if (s == "LED") return CircuitSymbolKind::LED;
    if (s == "Switch") return CircuitSymbolKind::Switch;
    if (s == "Battery") return CircuitSymbolKind::Battery;
    if (s == "Ground") return CircuitSymbolKind::Ground;
    if (s == "Voltmeter") return CircuitSymbolKind::Voltmeter;
    if (s == "Ammeter") return CircuitSymbolKind::Ammeter;
    if (s == "HeatingElement") return CircuitSymbolKind::HeatingElement;
    if (s == "ConductorWire") return CircuitSymbolKind::ConductorWire;
    return make_cir_error(4601, "Unknown CircuitSymbolKind: " + std::string(s));
}

std::vector<std::pair<std::string, core::base::math::Vector2<double>>>
PlacedCircuitComponent::terminal_world_positions() const {
    const double rad = static_cast<double>(rotation_deg) * (3.14159265358979323846 / 180.0);
    const double ca = std::cos(rad);
    const double sa = std::sin(rad);
    auto xf = [&](double lx, double ly) -> core::base::math::Vector2<double> {
        return {x_mm + lx * ca - ly * sa, y_mm + lx * sa + ly * ca};
    };

    if (kind == CircuitSymbolKind::Ground) {
        return {{"GND", xf(0.0, -5.0)}};
    }
    if (kind == CircuitSymbolKind::VoltageSourceDC ||
        kind == CircuitSymbolKind::VoltageSourceAC ||
        kind == CircuitSymbolKind::Battery) {
        return {{"+", xf(-10.0, 0.0)}, {"-", xf(10.0, 0.0)}};
    }
    if (kind == CircuitSymbolKind::Diode || kind == CircuitSymbolKind::LED) {
        return {{"A", xf(-10.0, 0.0)}, {"K", xf(10.0, 0.0)}};
    }
    return {{"1", xf(-10.0, 0.0)}, {"2", xf(10.0, 0.0)}};
}

void InteractiveCircuitDocument::add_component(PlacedCircuitComponent comp) {
    if (comp.terminal_nets.empty()) {
        const size_t n = (comp.kind == CircuitSymbolKind::Ground) ? 1u : 2u;
        comp.terminal_nets.assign(n, "");
    }
    components_.push_back(std::move(comp));
}

bool InteractiveCircuitDocument::move_component(std::string_view id, double x_mm, double y_mm) {
    if (auto* c = find_component_mut(id)) {
        c->x_mm = x_mm;
        c->y_mm = y_mm;
        return true;
    }
    return false;
}

bool InteractiveCircuitDocument::rotate_component(std::string_view id, int delta_deg) {
    if (auto* c = find_component_mut(id)) {
        c->rotation_deg = ((c->rotation_deg + delta_deg) % 360 + 360) % 360;
        return true;
    }
    return false;
}

bool InteractiveCircuitDocument::remove_component(std::string_view id) {
    auto it = std::remove_if(components_.begin(), components_.end(), [&](const auto& c) {
        return c.id == id;
    });
    if (it == components_.end()) return false;
    components_.erase(it, components_.end());
    return true;
}

PlacedCircuitComponent* InteractiveCircuitDocument::find_component_mut(std::string_view id) noexcept {
    for (auto& c : components_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const PlacedCircuitComponent* InteractiveCircuitDocument::find_component(std::string_view id) const noexcept {
    for (const auto& c : components_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

void InteractiveCircuitDocument::add_wire(CircuitWire wire) {
    wires_.push_back(std::move(wire));
}

bool InteractiveCircuitDocument::remove_wire(std::string_view id) {
    auto it = std::remove_if(wires_.begin(), wires_.end(), [&](const auto& w) {
        return w.id == id;
    });
    if (it == wires_.end()) return false;
    wires_.erase(it, wires_.end());
    return true;
}

void InteractiveCircuitDocument::add_junction(CircuitJunction junc) {
    junctions_.push_back(std::move(junc));
}

bool InteractiveCircuitDocument::remove_junction(std::string_view id) {
    auto it = std::remove_if(junctions_.begin(), junctions_.end(), [&](const auto& j) {
        return j.id == id;
    });
    if (it == junctions_.end()) return false;
    junctions_.erase(it, junctions_.end());
    return true;
}

void InteractiveCircuitDocument::clear() {
    components_.clear();
    wires_.clear();
    junctions_.clear();
}

void InteractiveCircuitDocument::sync_from_component_library(const ComponentLibraryManager& lib) {
    for (auto& pc : components_) {
        if (!pc.generated_component_id.has_value()) continue;
        const auto* gen = lib.get_component(*pc.generated_component_id);
        if (!gen) continue;
        pc.primary_value = gen->derived.primary_value;
        if (gen->kind == ComponentKind::Battery) {
            pc.secondary_value = gen->config.internal_resistance_ohm;
            pc.tertiary_value = gen->config.capacity_ah;
        }
        pc.max_current_rating_a = gen->derived.max_current_a;
        pc.max_power_rating_w = gen->derived.max_power_w;
    }
}

void InteractiveCircuitDocument::resolve_connectivity(double snap_tol_mm) {
    struct Anchor {
        core::base::math::Vector2<double> pt;
        bool is_ground{false};
        int comp_idx{-1};
        int term_idx{-1};
        int wire_idx{-1};
        int junc_idx{-1};
    };

    std::vector<Anchor> anchors;
    for (size_t ci = 0; ci < components_.size(); ++ci) {
        auto terms = components_[ci].terminal_world_positions();
        components_[ci].terminal_nets.assign(terms.size(), "");
        for (size_t ti = 0; ti < terms.size(); ++ti) {
            anchors.push_back({
                terms[ti].second,
                components_[ci].kind == CircuitSymbolKind::Ground,
                static_cast<int>(ci), static_cast<int>(ti), -1, -1
            });
        }
    }

    std::vector<std::vector<int>> wire_anchor_indices(wires_.size());
    for (size_t wi = 0; wi < wires_.size(); ++wi) {
        for (const auto& pt : wires_[wi].points_mm) {
            const int idx = static_cast<int>(anchors.size());
            anchors.push_back({pt, false, -1, -1, static_cast<int>(wi), -1});
            wire_anchor_indices[wi].push_back(idx);
        }
    }

    for (size_t ji = 0; ji < junctions_.size(); ++ji) {
        anchors.push_back({junctions_[ji].pos_mm, false, -1, -1, -1, static_cast<int>(ji)});
    }

    const int M = static_cast<int>(anchors.size());
    if (M == 0) return;
    DisjointSet dsu(M);

    // Connect all vertices of the same wire
    for (size_t wi = 0; wi < wires_.size(); ++wi) {
        for (size_t k = 1; k < wire_anchor_indices[wi].size(); ++k) {
            dsu.unite(wire_anchor_indices[wi][k - 1], wire_anchor_indices[wi][k]);
        }
    }

    // Connect spatially coincident anchors
    for (int i = 0; i < M; ++i) {
        for (int j = i + 1; j < M; ++j) {
            const double dist = std::hypot(anchors[i].pt.x - anchors[j].pt.x,
                                           anchors[i].pt.y - anchors[j].pt.y);
            if (dist <= snap_tol_mm) {
                dsu.unite(i, j);
            }
        }
    }

    // Connect junctions (and terminals) sitting on any wire segment
    for (int i = 0; i < M; ++i) {
        if (anchors[i].junc_idx < 0 && anchors[i].comp_idx < 0) continue;
        for (size_t wi = 0; wi < wires_.size(); ++wi) {
            const auto& pts = wires_[wi].points_mm;
            for (size_t s = 1; s < pts.size(); ++s) {
                if (point_to_segment_dist(anchors[i].pt, pts[s - 1], pts[s]) <= snap_tol_mm) {
                    dsu.unite(i, wire_anchor_indices[wi][0]);
                    break;
                }
            }
        }
    }

    std::unordered_map<int, bool> root_is_gnd;
    for (int i = 0; i < M; ++i) {
        if (anchors[i].is_ground) {
            root_is_gnd[dsu.find(i)] = true;
        }
    }

    std::unordered_map<int, std::string> root_net_name;
    int next_net_id = 1;
    for (int i = 0; i < M; ++i) {
        const int r = dsu.find(i);
        if (root_net_name.find(r) == root_net_name.end()) {
            if (root_is_gnd[r]) root_net_name[r] = "0";
            else root_net_name[r] = "N" + std::to_string(next_net_id++);
        }
    }

    for (int i = 0; i < M; ++i) {
        const std::string& net = root_net_name[dsu.find(i)];
        if (anchors[i].comp_idx >= 0) {
            components_[static_cast<size_t>(anchors[i].comp_idx)]
                .terminal_nets[static_cast<size_t>(anchors[i].term_idx)] = net;
        } else if (anchors[i].wire_idx >= 0) {
            wires_[static_cast<size_t>(anchors[i].wire_idx)].net_name = net;
        } else if (anchors[i].junc_idx >= 0) {
            junctions_[static_cast<size_t>(anchors[i].junc_idx)].net_name = net;
        }
    }
}

core::base::Result<core::domain::CircuitNetlist> InteractiveCircuitDocument::compile_netlist() const {
    using namespace core::domain;

    CircuitNetlist netlist;
    bool has_ground = false;
    for (const auto& c : components_) {
        if (c.kind == CircuitSymbolKind::Ground) {
            has_ground = true;
            continue;
        }
        if (c.terminal_nets.size() < 2 || c.terminal_nets[0].empty() || c.terminal_nets[1].empty()) {
            return make_cir_error(4610, "Component '" + c.id + "' has unconnected terminal(s).");
        }
        const std::string& n0 = c.terminal_nets[0];
        const std::string& n1 = c.terminal_nets[1];
        if (n0 == "0" || n1 == "0") has_ground = true;

        ElectricalComponent ec;
        ec.id = c.id;

        switch (c.kind) {
        case CircuitSymbolKind::VoltageSourceDC:
            ec.type = ElectricalComponentType::VoltageSourceDC;
            ec.value_primary = c.primary_value;
            ec.pins = {CircuitPin::make("+", n0), CircuitPin::make("-", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::VoltageSourceAC:
            ec.type = ElectricalComponentType::VoltageSourceAC;
            ec.value_primary = c.primary_value;
            ec.waveform.amplitude = (c.tertiary_value > 0.0) ? c.tertiary_value : c.primary_value;
            ec.waveform.frequency_hz = (c.secondary_value > 0.0) ? c.secondary_value : 50.0;
            ec.pins = {CircuitPin::make("+", n0), CircuitPin::make("-", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Battery: {
            // Model as ideal OCV source + internal series resistance R0
            const double r0 = std::max(1e-4, c.secondary_value > 0.0 ? c.secondary_value : 0.05);
            const std::string internal_net = c.id + "_int";

            ElectricalComponent v_ocv;
            v_ocv.id = c.id;
            v_ocv.type = ElectricalComponentType::VoltageSourceDC;
            v_ocv.value_primary = c.primary_value;
            v_ocv.pins = {CircuitPin::make("+", internal_net), CircuitPin::make("-", n1)};
            netlist.add_component(std::move(v_ocv));

            ElectricalComponent r_int;
            r_int.id = c.id + "_R0";
            r_int.type = ElectricalComponentType::Resistor;
            r_int.value_primary = r0;
            r_int.pins = {CircuitPin::make("1", internal_net), CircuitPin::make("2", n0)};
            netlist.add_component(std::move(r_int));
            break;
        }
        case CircuitSymbolKind::Resistor:
        case CircuitSymbolKind::HeatingElement:
        case CircuitSymbolKind::ConductorWire:
            if (c.primary_value <= 0.0) {
                return make_cir_error(4611, "Resistance of '" + c.id + "' must be positive.");
            }
            ec.type = ElectricalComponentType::Resistor;
            ec.value_primary = c.primary_value;
            ec.pins = {CircuitPin::make("1", n0), CircuitPin::make("2", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Switch:
            ec.type = ElectricalComponentType::Resistor;
            ec.value_primary = c.switch_closed ? 1e-3 : 1e9;
            ec.pins = {CircuitPin::make("1", n0), CircuitPin::make("2", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Capacitor:
            if (c.primary_value <= 0.0) {
                return make_cir_error(4612, "Capacitance of '" + c.id + "' must be positive.");
            }
            ec.type = ElectricalComponentType::Capacitor;
            ec.value_primary = c.primary_value;
            ec.value_secondary = c.secondary_value; // IC
            ec.pins = {CircuitPin::make("1", n0), CircuitPin::make("2", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Inductor:
            if (c.primary_value <= 0.0) {
                return make_cir_error(4613, "Inductance of '" + c.id + "' must be positive.");
            }
            ec.type = ElectricalComponentType::Inductor;
            ec.value_primary = c.primary_value;
            ec.pins = {CircuitPin::make("1", n0), CircuitPin::make("2", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Diode:
            ec.type = ElectricalComponentType::Diode;
            ec.is_saturation_current = 1e-14;
            ec.ideality_factor = 1.0;
            ec.pins = {CircuitPin::make("A", n0), CircuitPin::make("K", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::LED:
            ec.type = ElectricalComponentType::Diode;
            ec.is_saturation_current = 1e-19; // Yields ~1.9 - 2.1 V forward drop
            ec.ideality_factor = 1.8;
            ec.pins = {CircuitPin::make("A", n0), CircuitPin::make("K", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Voltmeter:
            ec.type = ElectricalComponentType::Resistor;
            ec.value_primary = 1e12;
            ec.pins = {CircuitPin::make("+", n0), CircuitPin::make("-", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Ammeter:
            ec.type = ElectricalComponentType::VoltageSourceDC;
            ec.value_primary = 0.0;
            ec.pins = {CircuitPin::make("+", n0), CircuitPin::make("-", n1)};
            netlist.add_component(std::move(ec));
            break;
        case CircuitSymbolKind::Ground:
            break;
        }
    }

    if (!has_ground) {
        return make_cir_error(4614, "Circuit has no Ground reference node ('0'). Add a Ground symbol.");
    }
    auto topo_res = netlist.build_node_topology();
    if (topo_res.is_err()) return topo_res.error();
    return netlist;
}

std::vector<core::domain::CircuitDiagnosticIssue> InteractiveCircuitDocument::validate() const {
    using namespace core::domain;
    std::vector<CircuitDiagnosticIssue> issues;

    if (components_.empty()) {
        issues.push_back({CircuitDiagnosticSeverity::Error, "EMPTY_CIRCUIT",
                          "Circuit contains no components.", "", "Add components and wire them to run simulation."});
        return issues;
    }

    // Count net connections to spot floating pins
    std::unordered_map<std::string, int> net_degree;
    bool has_gnd = false;
    bool has_source = false;

    for (const auto& c : components_) {
        if (c.kind == CircuitSymbolKind::Ground) {
            has_gnd = true;
            if (!c.terminal_nets.empty() && !c.terminal_nets[0].empty()) {
                net_degree[c.terminal_nets[0]]++;
            }
            continue;
        }
        if (c.kind == CircuitSymbolKind::VoltageSourceDC ||
            c.kind == CircuitSymbolKind::VoltageSourceAC ||
            c.kind == CircuitSymbolKind::Battery) {
            has_source = true;
        }
        if (is_two_pin_passive_or_semi(c.kind) && c.terminal_nets.size() >= 2) {
            if (c.terminal_nets[0].empty() || c.terminal_nets[1].empty()) {
                issues.push_back({CircuitDiagnosticSeverity::Error, "UNCONNECTED_PIN",
                                  "Component '" + c.id + "' has unconnected terminal(s).", c.id,
                                  "Connect both terminals with wires."});
            } else {
                net_degree[c.terminal_nets[0]]++;
                net_degree[c.terminal_nets[1]]++;
                if (c.terminal_nets[0] == c.terminal_nets[1]) {
                    issues.push_back({CircuitDiagnosticSeverity::Warning, "SHORTED_COMPONENT",
                                      "Both terminals of '" + c.id + "' are on the same net (" + c.terminal_nets[0] + ").",
                                      c.id, "Connect terminals to distinct circuit nodes."});
                }
            }
        }
    }

    if (!has_gnd) {
        issues.push_back({CircuitDiagnosticSeverity::Error, "NO_GROUND",
                          "Circuit is missing a Ground reference.", "",
                          "Place a Ground component and connect it to the negative return net."});
    }
    if (!has_source) {
        issues.push_back({CircuitDiagnosticSeverity::Warning, "NO_SOURCE",
                          "Circuit contains no Voltage Source or Battery.", "",
                          "Add a DC/AC Source or Battery to energize the circuit."});
    }
    for (const auto& [net, deg] : net_degree) {
        if (net != "0" && deg < 2) {
            issues.push_back({CircuitDiagnosticSeverity::Error, "FLOATING_NODE",
                              "Net '" + net + "' is connected to only one terminal (open/dangling node).",
                              net, "Complete the closed loop back to Ground or another component."});
        }
    }

    auto nl_res = compile_netlist();
    if (nl_res.is_ok()) {
        auto core_issues = CircuitDiagnostics::inspect_circuit(nl_res.value());
        issues.insert(issues.end(), core_issues.begin(), core_issues.end());
    }
    return issues;
}

InteractiveCircuitDocument InteractiveCircuitDocument::duplicate(std::string new_name) const {
    InteractiveCircuitDocument copy = *this;
    copy.id_ = core::base::Uuid::generate_random();
    copy.name_ = std::move(new_name);
    return copy;
}

std::string InteractiveCircuitDocument::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name_) << "\",\n";
    ss << "  \"components\": [\n";
    for (size_t i = 0; i < components_.size(); ++i) {
        const auto& c = components_[i];
        ss << "    {\"id\": \"" << escape_json(c.id)
           << "\", \"label\": \"" << escape_json(c.label)
           << "\", \"kind\": \"" << circuit_symbol_kind_to_string(c.kind)
           << "\", \"gen_id\": \"" << (c.generated_component_id ? c.generated_component_id->to_string() : "")
           << "\", \"x_mm\": " << c.x_mm << ", \"y_mm\": " << c.y_mm
           << ", \"rotation_deg\": " << c.rotation_deg
           << ", \"primary_value\": " << std::setprecision(10) << c.primary_value
           << ", \"secondary_value\": " << std::setprecision(10) << c.secondary_value
           << ", \"tertiary_value\": " << std::setprecision(10) << c.tertiary_value
           << ", \"switch_closed\": " << (c.switch_closed ? "true" : "false")
           << ", \"n0\": \"" << (c.terminal_nets.size() > 0 ? escape_json(c.terminal_nets[0]) : "")
           << "\", \"n1\": \"" << (c.terminal_nets.size() > 1 ? escape_json(c.terminal_nets[1]) : "")
           << "\"}";
        if (i + 1 < components_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"wires\": [\n";
    for (size_t i = 0; i < wires_.size(); ++i) {
        const auto& w = wires_[i];
        ss << "    {\"id\": \"" << escape_json(w.id) << "\", \"net\": \"" << escape_json(w.net_name) << "\", \"pts\": \"";
        for (size_t k = 0; k < w.points_mm.size(); ++k) {
            if (k > 0) ss << ";";
            ss << w.points_mm[k].x << "," << w.points_mm[k].y;
        }
        ss << "\"}";
        if (i + 1 < wires_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"junctions\": [\n";
    for (size_t i = 0; i < junctions_.size(); ++i) {
        const auto& j = junctions_[i];
        ss << "    {\"id\": \"" << escape_json(j.id) << "\", \"net\": \"" << escape_json(j.net_name)
           << "\", \"x\": " << j.pos_mm.x << ", \"y\": " << j.pos_mm.y << "}";
        if (i + 1 < junctions_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<InteractiveCircuitDocument> InteractiveCircuitDocument::from_json(std::string_view src) {
    if (src.empty()) return make_cir_error(4620, "Empty InteractiveCircuitDocument JSON");
    InteractiveCircuitDocument doc;
    if (auto id_opt = core::base::Uuid::from_string(json_str(src, "id"))) doc.id_ = *id_opt;
    std::string nm = json_str(src, "name");
    if (!nm.empty()) doc.name_ = std::move(nm);

    auto parse_array = [&](std::string_view key, auto fn) {
        const size_t pos = src.find("\"" + std::string(key) + "\"");
        if (pos == std::string_view::npos) return;
        std::string_view sub = src.substr(pos);
        const size_t end_arr = sub.find(']');
        if (end_arr != std::string_view::npos) sub = sub.substr(0, end_arr + 1);
        std::regex item_re(R"(\{[^\{\}]*\})");
        auto begin = std::regex_iterator(sub.begin(), sub.end(), item_re);
        auto end = std::regex_iterator<std::string_view::const_iterator>();
        for (auto it = begin; it != end; ++it) fn(it->str());
    };

    parse_array("components", [&](const std::string& item) {
        PlacedCircuitComponent c;
        c.id = json_str(item, "id");
        c.label = json_str(item, "label");
        if (auto k = circuit_symbol_kind_from_string(json_str(item, "kind")); k.is_ok()) c.kind = k.value();
        std::string gid = json_str(item, "gen_id");
        if (!gid.empty()) c.generated_component_id = core::base::Uuid::from_string(gid);
        c.x_mm = json_num(item, "x_mm", 0.0);
        c.y_mm = json_num(item, "y_mm", 0.0);
        c.rotation_deg = static_cast<int>(json_num(item, "rotation_deg", 0.0));
        c.primary_value = json_num(item, "primary_value", 1000.0);
        c.secondary_value = json_num(item, "secondary_value", 0.0);
        c.tertiary_value = json_num(item, "tertiary_value", 0.0);
        c.switch_closed = json_bool(item, "switch_closed", true);
        std::string n0 = json_str(item, "n0");
        std::string n1 = json_str(item, "n1");
        if (c.kind == CircuitSymbolKind::Ground) c.terminal_nets = {n0.empty() ? "0" : n0};
        else c.terminal_nets = {n0, n1};
        if (!c.id.empty()) doc.components_.push_back(std::move(c));
    });

    parse_array("wires", [&](const std::string& item) {
        CircuitWire w;
        w.id = json_str(item, "id");
        w.net_name = json_str(item, "net");
        std::string pts_s = json_str(item, "pts");
        std::stringstream ss(pts_s);
        std::string tok;
        while (std::getline(ss, tok, ';')) {
            auto comma = tok.find(',');
            if (comma != std::string::npos) {
                try {
                    w.points_mm.push_back({std::stod(tok.substr(0, comma)), std::stod(tok.substr(comma + 1))});
                } catch (...) {}
            }
        }
        if (!w.id.empty()) doc.wires_.push_back(std::move(w));
    });

    parse_array("junctions", [&](const std::string& item) {
        CircuitJunction j;
        j.id = json_str(item, "id");
        j.net_name = json_str(item, "net");
        j.pos_mm = {json_num(item, "x", 0.0), json_num(item, "y", 0.0)};
        if (!j.id.empty()) doc.junctions_.push_back(std::move(j));
    });

    return doc;
}

core::base::Result<CircuitDcSummary> CircuitSimulationRunner::run_dc(const InteractiveCircuitDocument& doc) {
    auto issues = doc.validate();
    for (const auto& iss : issues) {
        if (iss.severity == core::domain::CircuitDiagnosticSeverity::Error) {
            return make_cir_error(4630, "Circuit validation failed: " + iss.message, iss.actionable_remediation);
        }
    }

    auto nl_res = doc.compile_netlist();
    if (nl_res.is_err()) return nl_res.error();
    const auto& netlist = nl_res.value();

    auto op_res = pcb::SpiceSimulationEngine::solve_dc(netlist);
    if (op_res.is_err()) return op_res.error();
    const auto& op = op_res.value();

    CircuitDcSummary summary;
    summary.converged = op.converged;
    summary.node_voltages = op.node_voltages;

    std::unordered_map<std::string, double> comp_i;
    for (const auto& c : doc.components()) {
        if (c.kind == CircuitSymbolKind::Ground || c.terminal_nets.size() < 2) continue;
        const double vp = op.get_voltage(c.terminal_nets[0]);
        const double vq = op.get_voltage(c.terminal_nets[1]);
        const double dv = vp - vq;

        CircuitComponentMetrics m;
        m.component_id = c.id;
        m.kind = c.kind;
        m.voltage_v = dv;
        m.temperature_k = c.temperature_k;

        switch (c.kind) {
        case CircuitSymbolKind::VoltageSourceDC:
        case CircuitSymbolKind::VoltageSourceAC:
        case CircuitSymbolKind::Ammeter:
            m.current_a = op.get_current(c.id);
            m.power_w = dv * m.current_a;
            break;
        case CircuitSymbolKind::Battery: {
            const double r0 = std::max(1e-4, c.secondary_value > 0.0 ? c.secondary_value : 0.05);
            const double v_int = op.get_voltage(c.id + "_int");
            // Current leaving '+' terminal of battery
            m.current_a = (v_int - vp) / r0;
            m.effective_resistance_ohm = r0;
            m.power_w = dv * m.current_a;
            break;
        }
        case CircuitSymbolKind::Resistor:
        case CircuitSymbolKind::HeatingElement:
        case CircuitSymbolKind::ConductorWire:
            m.effective_resistance_ohm = c.primary_value;
            m.current_a = dv / std::max(1e-9, c.primary_value);
            m.power_w = m.current_a * m.current_a * c.primary_value;
            break;
        case CircuitSymbolKind::Switch:
            m.effective_resistance_ohm = c.switch_closed ? 1e-3 : 1e9;
            m.current_a = dv / m.effective_resistance_ohm;
            m.power_w = m.current_a * m.current_a * m.effective_resistance_ohm;
            break;
        case CircuitSymbolKind::Voltmeter:
            m.effective_resistance_ohm = 1e12;
            m.current_a = dv / 1e12;
            m.power_w = 0.0;
            break;
        case CircuitSymbolKind::Diode:
        case CircuitSymbolKind::LED: {
            const double is = (c.kind == CircuitSymbolKind::LED) ? 1e-19 : 1e-14;
            const double n = (c.kind == CircuitSymbolKind::LED) ? 1.8 : 1.0;
            double id = 0.0, gd = 0.0;
            core::domain::NonLinearDeviceState::evaluate_diode(dv, is, 0.02585, n, id, gd);
            m.current_a = id;
            m.power_w = dv * id;
            m.led_lit = (c.kind == CircuitSymbolKind::LED && id >= 1e-3);
            break;
        }
        case CircuitSymbolKind::Capacitor:
        case CircuitSymbolKind::Inductor:
        case CircuitSymbolKind::Ground:
            break;
        }

        if (std::abs(m.current_a) > c.max_current_rating_a || std::abs(m.power_w) > c.max_power_rating_w) {
            m.over_limit = true;
            summary.warnings.push_back("Component '" + c.id + "' exceeds rated current/power limit.");
        }
        comp_i[c.id] = m.current_a;
        summary.component_metrics.push_back(std::move(m));
    }

    summary.kcl_max_residual_a = compute_kcl_residual(doc, summary.node_voltages, comp_i);
    return summary;
}

core::base::Result<CircuitTransientSummary> CircuitSimulationRunner::run_transient(
    const InteractiveCircuitDocument& doc,
    double duration_s,
    double dt_s,
    const ComponentLibraryManager* lib,
    double ambient_temp_k) {
    if (duration_s <= 0.0 || dt_s <= 0.0) {
        return make_cir_error(4640, "Transient simulation duration_s and dt_s must be positive.");
    }
    auto issues = doc.validate();
    for (const auto& iss : issues) {
        if (iss.severity == core::domain::CircuitDiagnosticSeverity::Error) {
            return make_cir_error(4641, "Circuit validation failed: " + iss.message, iss.actionable_remediation);
        }
    }

    bool has_reactive_or_ac = false;
    bool has_battery = false;
    for (const auto& c : doc.components()) {
        if (c.kind == CircuitSymbolKind::Capacitor ||
            c.kind == CircuitSymbolKind::Inductor ||
            c.kind == CircuitSymbolKind::VoltageSourceAC) {
            has_reactive_or_ac = true;
        }
        if (c.kind == CircuitSymbolKind::Battery) has_battery = true;
    }

    CircuitTransientSummary out;

    // Case A: Dynamic reactive / AC SPICE transient (Capacitor charging/discharging, Inductor RL, AC rectification)
    if (has_reactive_or_ac && !has_battery) {
        auto nl_res = doc.compile_netlist();
        if (nl_res.is_err()) return nl_res.error();
        const auto& netlist = nl_res.value();

        pcb::SpiceTransientOptions opts;
        opts.t_stop_s = duration_s;
        opts.dt_s = dt_s;
        opts.method = pcb::SpiceIntegrationMethod::BackwardEuler;
        opts.use_dc_op_initial = false; // start uncharged unless explicit .IC

        auto tr_res = pcb::SpiceSimulationEngine::solve_transient(netlist, opts);
        if (tr_res.is_err()) return tr_res.error();
        const auto& tr = tr_res.value();

        out.success = tr.success;
        out.time_s = tr.time_points;
        out.node_voltages = tr.node_voltages;

        const size_t n_pts = out.time_s.size();
        for (const auto& c : doc.components()) {
            if (c.kind == CircuitSymbolKind::Ground || c.terminal_nets.size() < 2) continue;
            std::vector<double> v_trace(n_pts, 0.0), i_trace(n_pts, 0.0), p_trace(n_pts, 0.0), t_trace(n_pts, ambient_temp_k);
            const auto* vp_tr = tr.get_voltage_trace(c.terminal_nets[0]);
            const auto* vq_tr = tr.get_voltage_trace(c.terminal_nets[1]);
            const auto* br_tr = tr.get_current_trace(c.id);

            double c_th = 20.0, g_th = 0.2, temp = ambient_temp_k;
            if (lib && c.generated_component_id) {
                if (const auto* gen = lib->get_component(*c.generated_component_id)) {
                    c_th = std::max(1.0, gen->derived.thermal_mass_j_k);
                    g_th = std::max(0.01, gen->derived.thermal_conductance_w_k);
                }
            }

            for (size_t k = 0; k < n_pts; ++k) {
                const double vp = vp_tr ? (*vp_tr)[k] : 0.0;
                const double vq = vq_tr ? (*vq_tr)[k] : 0.0;
                const double dv = vp - vq;
                v_trace[k] = dv;
                double curr = 0.0;
                if (br_tr) curr = (*br_tr)[k];
                else if (c.kind == CircuitSymbolKind::Resistor || c.kind == CircuitSymbolKind::HeatingElement || c.kind == CircuitSymbolKind::ConductorWire) {
                    curr = dv / std::max(1e-9, c.primary_value);
                } else if (c.kind == CircuitSymbolKind::Switch) {
                    curr = dv / (c.switch_closed ? 1e-3 : 1e9);
                } else if (c.kind == CircuitSymbolKind::Capacitor && k > 0) {
                    curr = c.primary_value * (v_trace[k] - v_trace[k - 1]) / dt_s;
                }
                i_trace[k] = curr;
                p_trace[k] = dv * curr;
                if (k > 0 && (c.kind == CircuitSymbolKind::Resistor || c.kind == CircuitSymbolKind::HeatingElement)) {
                    const double q_cool = g_th * (temp - ambient_temp_k);
                    temp = std::max(100.0, temp + ((std::abs(p_trace[k]) - q_cool) / c_th) * dt_s);
                }
                t_trace[k] = temp;
            }
            out.component_voltages_v[c.id] = std::move(v_trace);
            out.component_currents_a[c.id] = std::move(i_trace);
            out.component_powers_w[c.id] = std::move(p_trace);
            out.component_temperatures_k[c.id] = std::move(t_trace);
        }
        return out;
    }

    // Case B: Coupled Battery / Electro-Thermal / DC-Stepped Transient
    InteractiveCircuitDocument sim_doc = doc;
    struct BatState {
        std::string comp_id;
        double soc{1.0};
        double cap_ah{2.5};
        double r0_25c{0.05};
        double v_scale{1.0};
        double temp_k{298.15};
        double c_th{42.0};
        double g_th{0.35};
    };
    std::vector<BatState> bats;
    std::unordered_map<std::string, double> comp_temp;
    for (const auto& c : sim_doc.components()) {
        comp_temp[c.id] = ambient_temp_k;
        if (c.kind == CircuitSymbolKind::Battery) {
            BatState bs;
            bs.comp_id = c.id;
            bs.cap_ah = (c.tertiary_value > 0.0) ? c.tertiary_value : 2.5;
            bs.r0_25c = (c.secondary_value > 0.0) ? c.secondary_value : 0.05;
            bs.v_scale = (c.primary_value > 0.0) ? (c.primary_value / 3.7) : 1.0;
            bs.temp_k = ambient_temp_k;
            if (lib && c.generated_component_id) {
                if (const auto* gen = lib->get_component(*c.generated_component_id)) {
                    bs.soc = gen->config.initial_soc;
                    bs.c_th = std::max(1.0, gen->derived.thermal_mass_j_k);
                    bs.g_th = std::max(0.05, gen->derived.thermal_conductance_w_k);
                }
            }
            bats.push_back(bs);
        }
    }

    const int n_steps = std::max(1, static_cast<int>(std::round(duration_s / dt_s)));
    for (int step = 0; step <= n_steps; ++step) {
        const double t = static_cast<double>(step) * dt_s;
        for (const auto& bs : bats) {
            if (auto* bc = sim_doc.find_component_mut(bs.comp_id)) {
                bc->primary_value = chem::LiIonSocTables::lookup_ocv_v(bs.soc) * bs.v_scale;
                bc->secondary_value = bs.r0_25c * std::exp(std::clamp(1800.0 * ((1.0 / bs.temp_k) - (1.0 / 298.15)), -5.0, 5.0));
            }
        }

        auto dc_res = run_dc(sim_doc);
        if (dc_res.is_err()) return dc_res.error();
        const auto& dc = dc_res.value();

        out.time_s.push_back(t);
        out.kcl_max_residual_a = std::max(out.kcl_max_residual_a, dc.kcl_max_residual_a);
        for (const auto& [net, v] : dc.node_voltages) {
            out.node_voltages[net].push_back(v);
        }

        for (const auto& m : dc.component_metrics) {
            out.component_voltages_v[m.component_id].push_back(m.voltage_v);
            out.component_currents_a[m.component_id].push_back(m.current_a);
            out.component_powers_w[m.component_id].push_back(m.power_w);

            // Electro-thermal Joule heating step
            if (step < n_steps) {
                double c_th = 25.0, g_th = 0.25;
                if (lib) {
                    if (const auto* pc = sim_doc.find_component(m.component_id); pc && pc->generated_component_id) {
                        if (const auto* gen = lib->get_component(*pc->generated_component_id)) {
                            c_th = std::max(1.0, gen->derived.thermal_mass_j_k);
                            g_th = std::max(0.02, gen->derived.thermal_conductance_w_k);
                        }
                    }
                }
                double& tk = comp_temp[m.component_id];
                const double q_gen = (m.kind == CircuitSymbolKind::Battery)
                    ? (m.current_a * m.current_a * std::max(1e-4, m.effective_resistance_ohm))
                    : std::abs(m.power_w);
                const double q_cool = g_th * (tk - ambient_temp_k);
                tk = std::max(100.0, tk + ((q_gen - q_cool) / c_th) * dt_s);
            }
            out.component_temperatures_k[m.component_id].push_back(comp_temp[m.component_id]);

            // Update battery SOC & thermal sync
            for (auto& bs : bats) {
                if (bs.comp_id == m.component_id) {
                    bs.temp_k = comp_temp[m.component_id];
                    if (step == 0 || step <= n_steps) {
                        out.battery_soc.push_back(bs.soc);
                    }
                    if (step < n_steps) {
                        bs.soc = std::clamp(bs.soc - (m.current_a * dt_s) / (bs.cap_ah * 3600.0), 0.0, 1.0);
                    }
                }
            }
        }
    }

    out.success = true;
    return out;
}

} // namespace shunya::eng
