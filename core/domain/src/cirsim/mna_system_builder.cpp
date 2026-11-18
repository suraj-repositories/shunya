#include "shunya/cirsim/mna_system_builder.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cirsim {

using namespace core::domain;

core::base::Result<MnaBlockMatrix> MnaSystemBuilder::build_dc_system(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components
) {
    MnaBlockMatrix sys;

    // Number of non-ground nodes N
    size_t n_nodes = 0;
    std::unordered_map<uint32_t, size_t> node_to_mna_row;

    // Sort non-ground indices to ensure deterministic row ordering
    std::vector<uint32_t> sorted_indices;
    for (const auto& [node_idx, info] : topology.nodes) {
        if (!info.is_ground) {
            sorted_indices.push_back(node_idx);
        }
    }
    std::sort(sorted_indices.begin(), sorted_indices.end());

    for (uint32_t node_idx : sorted_indices) {
        node_to_mna_row[node_idx] = n_nodes++;
        sys.node_labels.push_back(topology.nodes.at(node_idx).node_name);
    }
    sys.num_nodes = n_nodes;

    // Count voltage source branches M
    std::vector<const ElectricalComponent*> v_sources;
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourceAC ||
            comp.type == ElectricalComponentType::VoltageSourcePulse) {
            v_sources.push_back(&comp);
            sys.branch_labels.push_back(comp.id);
        }
    }
    sys.num_branches = v_sources.size();

    // Allocate blocks
    sys.G.resize(sys.num_nodes, sys.num_nodes);
    sys.B.resize(sys.num_nodes, sys.num_branches);
    sys.C.resize(sys.num_branches, sys.num_nodes);
    sys.D.resize(sys.num_branches, sys.num_branches);
    sys.i.assign(sys.num_nodes, 0.0);
    sys.e.assign(sys.num_branches, 0.0);

    auto get_node_row = [&](std::string_view comp_id, std::string_view pin_id) -> int {
        std::string key = std::string(comp_id) + ":" + std::string(pin_id);
        auto it = topology.pin_to_node_map.find(key);
        if (it == topology.pin_to_node_map.end()) return -1;
        uint32_t node_idx = it->second;
        if (node_idx == 0) return -1; // Ground
        auto row_it = node_to_mna_row.find(node_idx);
        return (row_it != node_to_mna_row.end()) ? static_cast<int>(row_it->second) : -1;
    };

    // Stamp passive conductances into G
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::Resistor && comp.pins.size() >= 2) {
            const double r = std::max(1e-12, comp.value_primary);
            const double g = 1.0 / r;
            const int p = get_node_row(comp.id, comp.pins[0].pin_id);
            const int q = get_node_row(comp.id, comp.pins[1].pin_id);

            if (p >= 0) sys.G.add(p, p, g);
            if (q >= 0) sys.G.add(q, q, g);
            if (p >= 0 && q >= 0) {
                sys.G.add(p, q, -g);
                sys.G.add(q, p, -g);
            }
        } else if (comp.type == ElectricalComponentType::CurrentSourceDC && comp.pins.size() >= 2) {
            // Injected current flows from pin[1] to pin[0]
            const double is = comp.value_primary;
            const int p = get_node_row(comp.id, comp.pins[0].pin_id); // (+)
            const int q = get_node_row(comp.id, comp.pins[1].pin_id); // (-)
            if (p >= 0) sys.i[p] += is;
            if (q >= 0) sys.i[q] -= is;
        }
    }

    // Stamp voltage sources into B, C, D, e
    for (size_t b = 0; b < v_sources.size(); ++b) {
        const auto* vsrc = v_sources[b];
        if (vsrc->pins.size() >= 2) {
            const int p = get_node_row(vsrc->id, vsrc->pins[0].pin_id); // (+)
            const int q = get_node_row(vsrc->id, vsrc->pins[1].pin_id); // (-)

            if (p >= 0) {
                sys.B.add(p, b, 1.0);
                sys.C.add(b, p, 1.0);
            }
            if (q >= 0) {
                sys.B.add(q, b, -1.0);
                sys.C.add(b, q, -1.0);
            }
            sys.e[b] = vsrc->value_primary; // DC voltage setpoint
        }
    }

    // Assemble unified system A and z
    const size_t total_dim = sys.num_nodes + sys.num_branches;
    sys.A.resize(total_dim, total_dim);
    sys.z.assign(total_dim, 0.0);

    for (size_t r = 0; r < sys.num_nodes; ++r) {
        for (size_t c = 0; c < sys.num_nodes; ++c) {
            sys.A(r, c) = sys.G(r, c);
        }
        for (size_t c = 0; c < sys.num_branches; ++c) {
            sys.A(r, sys.num_nodes + c) = sys.B(r, c);
        }
        sys.z[r] = sys.i[r];
    }

    for (size_t r = 0; r < sys.num_branches; ++r) {
        for (size_t c = 0; c < sys.num_nodes; ++c) {
            sys.A(sys.num_nodes + r, c) = sys.C(r, c);
        }
        for (size_t c = 0; c < sys.num_branches; ++c) {
            sys.A(sys.num_nodes + r, sys.num_nodes + c) = sys.D(r, c);
        }
        sys.z[sys.num_nodes + r] = sys.e[r];
    }

    return sys;
}

core::base::Result<MnaBlockMatrix> MnaSystemBuilder::build_transient_companion_system(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components,
    double dt,
    const std::unordered_map<std::string, double>& cap_voltages_prev,
    const std::unordered_map<std::string, double>& ind_currents_prev
) {
    (void)ind_currents_prev;
    auto res = build_dc_system(topology, components);
    if (res.is_err()) return res;
    auto sys = res.value();

    if (dt <= 0.0) return sys;

    auto get_node_row = [&](std::string_view comp_id, std::string_view pin_id) -> int {
        std::string key = std::string(comp_id) + ":" + std::string(pin_id);
        auto it = topology.pin_to_node_map.find(key);
        if (it == topology.pin_to_node_map.end()) return -1;
        uint32_t node_idx = it->second;
        if (node_idx == 0) return -1; // Ground
        // Find row
        for (size_t i = 0; i < sys.node_labels.size(); ++i) {
            if (sys.node_labels[i] == topology.nodes.at(node_idx).node_name) return static_cast<int>(i);
        }
        return -1;
    };

    // Stamp companion models for dynamic elements (Capacitors G_eq = C/dt, I_eq = G_eq * V_prev)
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
            const double c = comp.value_primary;
            const double g_eq = c / dt;
            double v_prev = 0.0;
            auto v_it = cap_voltages_prev.find(comp.id);
            if (v_it != cap_voltages_prev.end()) v_prev = v_it->second;
            const double i_eq = g_eq * v_prev;

            const int p = get_node_row(comp.id, comp.pins[0].pin_id);
            const int q = get_node_row(comp.id, comp.pins[1].pin_id);

            if (p >= 0) {
                sys.G.add(p, p, g_eq);
                sys.A.add(p, p, g_eq);
                sys.i[p] += i_eq;
                sys.z[p] += i_eq;
            }
            if (q >= 0) {
                sys.G.add(q, q, g_eq);
                sys.A.add(q, q, g_eq);
                sys.i[q] -= i_eq;
                sys.z[q] -= i_eq;
            }
            if (p >= 0 && q >= 0) {
                sys.G.add(p, q, -g_eq);
                sys.G.add(q, p, -g_eq);
                sys.A.add(p, q, -g_eq);
                sys.A.add(q, p, -g_eq);
            }
        }
    }

    return sys;
}

} // namespace shunya::cirsim
