#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <set>

namespace shunya::cirsim {

namespace {

struct DSU {
    std::vector<int> parent;
    std::vector<int> rank;

    explicit DSU(size_t n) : parent(n), rank(n, 0) {
        for (size_t i = 0; i < n; ++i) parent[i] = static_cast<int>(i);
    }

    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]);
    }

    void unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i != root_j) {
            if (rank[root_i] < rank[root_j]) std::swap(root_i, root_j);
            parent[root_j] = root_i;
            if (rank[root_i] == rank[root_j]) rank[root_i]++;
        }
    }
};

bool points_close(const Point2D& a, const Point2D& b, double tol) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return (dx * dx + dy * dy) <= (tol * tol);
}

} // namespace

SchematicTopologyParser::SchematicTopologyParser(double snap_tolerance)
    : snap_tolerance_(snap_tolerance) {}

void SchematicTopologyParser::add_component_pin(const SchematicPinAnchor& pin) {
    pins_.push_back(pin);
}

void SchematicTopologyParser::add_wire(const SchematicWire& wire) {
    wires_.push_back(wire);
}

void SchematicTopologyParser::add_junction(const SchematicJunctionDot& junction) {
    junctions_.push_back(junction);
}

void SchematicTopologyParser::clear() noexcept {
    pins_.clear();
    wires_.clear();
    junctions_.clear();
}

ParsedSchematicTopology SchematicTopologyParser::parse() {
    ParsedSchematicTopology result;

    // Collect all spatial geometric vertices (pins, wire endpoints, junctions)
    struct VertexRef {
        Point2D pt;
        int pin_idx{-1};
        int wire_idx{-1};
        bool is_wire_end{false};
        std::string explicit_label;
    };

    std::vector<VertexRef> vertices;
    for (size_t i = 0; i < pins_.size(); ++i) {
        vertices.push_back(VertexRef{pins_[i].location, static_cast<int>(i), -1, false, ""});
    }
    for (size_t i = 0; i < wires_.size(); ++i) {
        vertices.push_back(VertexRef{wires_[i].start, -1, static_cast<int>(i), false, wires_[i].user_net_label});
        vertices.push_back(VertexRef{wires_[i].end, -1, static_cast<int>(i), true, wires_[i].user_net_label});
    }
    for (size_t i = 0; i < junctions_.size(); ++i) {
        vertices.push_back(VertexRef{junctions_[i].position, -1, -1, false, ""});
    }

    if (vertices.empty()) {
        result.diagnostics.push_back(TopologicalDiagnostic{
            TopologicalDiagnostic::Severity::Warning,
            "SCHEM_EMPTY",
            "Schematic topology contains no pins, wires, or junctions.",
            {}
        });
        return result;
    }

    DSU dsu(vertices.size());

    // 1. Unite wire start and end (each wire segment connects its endpoints)
    for (size_t w = 0; w < wires_.size(); ++w) {
        const int start_v = static_cast<int>(pins_.size() + 2 * w);
        const int end_v = start_v + 1;
        dsu.unite(start_v, end_v);
    }

    // 2. Unite vertices that are spatially coincident within snap_tolerance
    for (size_t i = 0; i < vertices.size(); ++i) {
        for (size_t j = i + 1; j < vertices.size(); ++j) {
            if (points_close(vertices[i].pt, vertices[j].pt, snap_tolerance_)) {
                dsu.unite(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }

    // 3. Cluster identification and net naming
    std::unordered_map<int, std::vector<size_t>> clusters;
    for (size_t i = 0; i < vertices.size(); ++i) {
        clusters[dsu.find(static_cast<int>(i))].push_back(i);
    }

    // Determine which cluster is ground ("0", "GND", "GROUND")
    int ground_root = -1;
    for (const auto& [root, members] : clusters) {
        for (size_t idx : members) {
            const auto& lbl = vertices[idx].explicit_label;
            if (lbl == "0" || lbl == "GND" || lbl == "GROUND" || lbl == "gnd") {
                ground_root = root;
                break;
            }
            if (vertices[idx].pin_idx >= 0) {
                const auto& p = pins_[vertices[idx].pin_idx];
                if (p.pin_id == "GND" || p.component_id.find("GND") != std::string::npos) {
                    ground_root = root;
                    break;
                }
            }
        }
        if (ground_root >= 0) break;
    }

    // Assign node indices (0 for ground, 1..N for others)
    std::unordered_map<int, uint32_t> root_to_node_index;
    uint32_t next_index = 1;

    if (ground_root >= 0) {
        root_to_node_index[ground_root] = 0;
        result.has_ground_reference = true;
    }

    for (const auto& [root, members] : clusters) {
        if (root == ground_root) continue;
        root_to_node_index[root] = next_index++;
    }

    // Build CircuitNodeInfo for each cluster
    for (const auto& [root, members] : clusters) {
        const uint32_t node_idx = root_to_node_index[root];
        CircuitNodeInfo info;
        info.node_index = node_idx;
        info.is_ground = (node_idx == 0);

        std::string preferred_name;
        for (size_t idx : members) {
            if (!vertices[idx].explicit_label.empty() && preferred_name.empty()) {
                preferred_name = vertices[idx].explicit_label;
            }
            if (vertices[idx].pin_idx >= 0) {
                const auto& p = pins_[vertices[idx].pin_idx];
                const std::string pin_key = p.component_id + ":" + p.pin_id;
                info.connected_component_pins.push_back(pin_key);
                result.pin_to_node_map[pin_key] = node_idx;
            }
            info.junction_points.push_back(vertices[idx].pt);
        }

        if (info.is_ground) {
            info.node_name = "0";
        } else if (!preferred_name.empty()) {
            info.node_name = preferred_name;
        } else {
            info.node_name = "NET_" + std::to_string(node_idx);
        }

        result.nodes[node_idx] = info;
    }

    // Diagnostics checks
    if (!result.has_ground_reference) {
        result.diagnostics.push_back(TopologicalDiagnostic{
            TopologicalDiagnostic::Severity::Error,
            "SCHEM_NO_GROUND",
            "Circuit lacks an authoritative ground reference node (Node 0 / GND).",
            {}
        });
    }

    auto floating = find_floating_nodes(result);
    result.floating_node_count = floating.size();
    for (uint32_t node_idx : floating) {
        const auto& n = result.nodes[node_idx];
        result.diagnostics.push_back(TopologicalDiagnostic{
            TopologicalDiagnostic::Severity::Warning,
            "SCHEM_FLOATING_NODE",
            "Node '" + n.node_name + "' has fewer than 2 pin connections (floating/stub).",
            n.connected_component_pins
        });
    }

    return result;
}

std::vector<uint32_t> SchematicTopologyParser::find_floating_nodes(
    const ParsedSchematicTopology& topology
) {
    std::vector<uint32_t> floating;
    for (const auto& [idx, node] : topology.nodes) {
        if (!node.is_ground && node.connected_component_pins.size() <= 1) {
            floating.push_back(idx);
        }
    }
    return floating;
}

std::vector<std::vector<std::string>> SchematicTopologyParser::detect_topological_loops(
    const ParsedSchematicTopology& topology,
    const std::vector<core::domain::ElectricalComponent>& components
) {
    std::vector<std::vector<std::string>> loops;

    // Build bipartite node-component graph
    std::unordered_map<uint32_t, std::vector<std::string>> node_to_comps;
    std::unordered_map<std::string, std::vector<uint32_t>> comp_to_nodes;

    for (const auto& comp : components) {
        for (const auto& pin : comp.pins) {
            const std::string pin_key = comp.id + ":" + pin.pin_id;
            auto it = topology.pin_to_node_map.find(pin_key);
            if (it != topology.pin_to_node_map.end()) {
                node_to_comps[it->second].push_back(comp.id);
                comp_to_nodes[comp.id].push_back(it->second);
            }
        }
    }

    // Simple cycle detection using DFS on node graph
    std::unordered_set<uint32_t> visited;
    std::vector<uint32_t> path;

    auto dfs = [&](auto& self, uint32_t u, uint32_t parent) -> void {
        visited.insert(u);
        path.push_back(u);

        for (const auto& cid : node_to_comps[u]) {
            for (uint32_t v : comp_to_nodes[cid]) {
                if (v == u || v == parent) continue;
                if (visited.count(v)) {
                    // Cycle detected! Collect components along path
                    std::vector<std::string> loop_comps;
                    loop_comps.push_back(cid);
                    loops.push_back(loop_comps);
                } else {
                    self(self, v, u);
                }
            }
        }
        path.pop_back();
    };

    for (const auto& [idx, _] : topology.nodes) {
        if (!visited.count(idx)) {
            dfs(dfs, idx, 999999);
        }
    }

    return loops;
}

} // namespace shunya::cirsim
