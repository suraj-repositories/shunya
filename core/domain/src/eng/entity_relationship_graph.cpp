#include "shunya/eng/entity_relationship_graph.hpp"
#include "shunya/eng/engineering_pipeline.hpp"

#include <algorithm>
#include <iomanip>
#include <queue>
#include <sstream>

namespace shunya::eng {

void EntityRelationshipGraph::clear() noexcept {
    nodes_.clear();
    edges_.clear();
    outgoing_adj_.clear();
    incoming_adj_.clear();
}

bool EntityRelationshipGraph::add_node(EntityGraphNode node) {
    const auto id = node.id;
    auto [it, inserted] = nodes_.insert_or_assign(id, std::move(node));
    (void)it;
    return inserted;
}

bool EntityRelationshipGraph::remove_node(const core::base::Uuid& id) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
        return false;
    }
    nodes_.erase(it);

    // Remove connected edges
    auto edge_it = std::remove_if(edges_.begin(), edges_.end(), [&](const EntityGraphEdge& e) {
        return e.source_id == id || e.target_id == id;
    });
    if (edge_it != edges_.end()) {
        edges_.erase(edge_it, edges_.end());
    }
    rebuild_adjacency_indices();
    return true;
}

bool EntityRelationshipGraph::has_node(const core::base::Uuid& id) const noexcept {
    return nodes_.find(id) != nodes_.end();
}

const EntityGraphNode* EntityRelationshipGraph::find_node(const core::base::Uuid& id) const noexcept {
    auto it = nodes_.find(id);
    return it != nodes_.end() ? &it->second : nullptr;
}

bool EntityRelationshipGraph::add_edge(core::base::Uuid source_id,
                                      core::base::Uuid target_id,
                                      RelationKind kind,
                                      std::string label) {
    if (has_edge(source_id, target_id, kind)) {
        return false;
    }
    const size_t edge_idx = edges_.size();
    edges_.push_back(EntityGraphEdge{
        .source_id = source_id,
        .target_id = target_id,
        .kind = kind,
        .label = std::move(label)
    });
    outgoing_adj_[source_id].push_back(edge_idx);
    incoming_adj_[target_id].push_back(edge_idx);
    return true;
}

bool EntityRelationshipGraph::remove_edge(const core::base::Uuid& source_id,
                                         const core::base::Uuid& target_id,
                                         RelationKind kind) {
    auto it = std::find_if(edges_.begin(), edges_.end(), [&](const EntityGraphEdge& e) {
        return e.source_id == source_id && e.target_id == target_id && e.kind == kind;
    });
    if (it == edges_.end()) {
        return false;
    }
    edges_.erase(it);
    rebuild_adjacency_indices();
    return true;
}

bool EntityRelationshipGraph::has_edge(const core::base::Uuid& source_id,
                                      const core::base::Uuid& target_id,
                                      RelationKind kind) const noexcept {
    auto it = outgoing_adj_.find(source_id);
    if (it == outgoing_adj_.end()) return false;
    for (size_t idx : it->second) {
        if (idx < edges_.size()) {
            const auto& e = edges_[idx];
            if (e.target_id == target_id && e.kind == kind) {
                return true;
            }
        }
    }
    return false;
}

void EntityRelationshipGraph::rebuild_adjacency_indices() {
    outgoing_adj_.clear();
    incoming_adj_.clear();
    for (size_t i = 0; i < edges_.size(); ++i) {
        outgoing_adj_[edges_[i].source_id].push_back(i);
        incoming_adj_[edges_[i].target_id].push_back(i);
    }
}

std::vector<EntityGraphEdge> EntityRelationshipGraph::get_outgoing_edges(const core::base::Uuid& id) const {
    std::vector<EntityGraphEdge> res;
    auto it = outgoing_adj_.find(id);
    if (it != outgoing_adj_.end()) {
        res.reserve(it->second.size());
        for (size_t idx : it->second) {
            if (idx < edges_.size()) res.push_back(edges_[idx]);
        }
    }
    return res;
}

std::vector<EntityGraphEdge> EntityRelationshipGraph::get_incoming_edges(const core::base::Uuid& id) const {
    std::vector<EntityGraphEdge> res;
    auto it = incoming_adj_.find(id);
    if (it != incoming_adj_.end()) {
        res.reserve(it->second.size());
        for (size_t idx : it->second) {
            if (idx < edges_.size()) res.push_back(edges_[idx]);
        }
    }
    return res;
}

std::vector<core::base::Uuid> EntityRelationshipGraph::get_direct_dependents(const core::base::Uuid& id) const {
    std::vector<core::base::Uuid> res;
    auto it = outgoing_adj_.find(id);
    if (it != outgoing_adj_.end()) {
        for (size_t idx : it->second) {
            if (idx < edges_.size()) {
                res.push_back(edges_[idx].target_id);
            }
        }
    }
    return res;
}

std::vector<core::base::Uuid> EntityRelationshipGraph::get_direct_dependencies(const core::base::Uuid& id) const {
    std::vector<core::base::Uuid> res;
    auto it = incoming_adj_.find(id);
    if (it != incoming_adj_.end()) {
        for (size_t idx : it->second) {
            if (idx < edges_.size()) {
                res.push_back(edges_[idx].source_id);
            }
        }
    }
    return res;
}

std::vector<core::base::Uuid> EntityRelationshipGraph::get_transitive_dependents(const core::base::Uuid& id) const {
    std::vector<core::base::Uuid> order;
    std::unordered_set<core::base::Uuid> visited;
    std::queue<core::base::Uuid> q;

    visited.insert(id);
    q.push(id);

    while (!q.empty()) {
        const auto cur = q.front();
        q.pop();

        for (const auto& dep : get_direct_dependents(cur)) {
            if (visited.insert(dep).second) {
                order.push_back(dep);
                q.push(dep);
            }
        }
    }
    return order;
}

std::vector<core::base::Uuid> EntityRelationshipGraph::get_transitive_dependencies(const core::base::Uuid& id) const {
    std::vector<core::base::Uuid> order;
    std::unordered_set<core::base::Uuid> visited;
    std::queue<core::base::Uuid> q;

    visited.insert(id);
    q.push(id);

    while (!q.empty()) {
        const auto cur = q.front();
        q.pop();

        for (const auto& dep : get_direct_dependencies(cur)) {
            if (visited.insert(dep).second) {
                order.push_back(dep);
                q.push(dep);
            }
        }
    }
    return order;
}

std::vector<EntityGraphEdge> EntityRelationshipGraph::find_orphaned_edges() const {
    std::vector<EntityGraphEdge> orphans;
    for (const auto& e : edges_) {
        if (!has_node(e.source_id) || !has_node(e.target_id)) {
            orphans.push_back(e);
        }
    }
    return orphans;
}

bool EntityRelationshipGraph::has_cycles() const {
    std::unordered_map<core::base::Uuid, int> in_degree;
    for (const auto& [id, _] : nodes_) {
        in_degree[id] = 0;
    }
    for (const auto& e : edges_) {
        if (has_node(e.target_id)) {
            in_degree[e.target_id]++;
        }
    }

    std::queue<core::base::Uuid> q;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) q.push(id);
    }

    size_t visited_count = 0;
    while (!q.empty()) {
        const auto cur = q.front();
        q.pop();
        ++visited_count;

        for (const auto& next : get_direct_dependents(cur)) {
            if (has_node(next)) {
                if (--in_degree[next] == 0) {
                    q.push(next);
                }
            }
        }
    }
    return visited_count < nodes_.size();
}

core::base::Result<std::vector<core::base::Uuid>> EntityRelationshipGraph::topological_sort() const {
    std::unordered_map<core::base::Uuid, int> in_degree;
    for (const auto& [id, _] : nodes_) {
        in_degree[id] = 0;
    }
    for (const auto& e : edges_) {
        if (has_node(e.target_id)) {
            in_degree[e.target_id]++;
        }
    }

    std::queue<core::base::Uuid> q;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) q.push(id);
    }

    std::vector<core::base::Uuid> result;
    result.reserve(nodes_.size());

    while (!q.empty()) {
        const auto cur = q.front();
        q.pop();
        result.push_back(cur);

        for (const auto& next : get_direct_dependents(cur)) {
            if (has_node(next)) {
                if (--in_degree[next] == 0) {
                    q.push(next);
                }
            }
        }
    }

    if (result.size() < nodes_.size()) {
        return core::base::Result<std::vector<core::base::Uuid>>::err(core::base::Error{
            .code = 4930,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Cyclic dependency detected in entity-relationship graph",
            .details = "Topological ordering failed due to cycle"
        });
    }
    return core::base::Result<std::vector<core::base::Uuid>>::ok(std::move(result));
}

std::string EntityRelationshipGraph::to_dot() const {
    std::ostringstream os;
    os << "digraph ShunyaEntityGraph {\n";
    os << "  rankdir=LR;\n";
    os << "  node [shape=box, style=filled, fontname=\"Helvetica\", fontsize=10];\n";
    os << "  edge [fontname=\"Helvetica\", fontsize=8];\n";

    for (const auto& [id, n] : nodes_) {
        std::string color = "#e0e0e0";
        switch (n.kind) {
        case EntityKind::Element:        color = "#ffdd99"; break;
        case EntityKind::Material:       color = "#ffccaa"; break;
        case EntityKind::Mixture:        color = "#ffe0cc"; break;
        case EntityKind::PhysicalObject: color = "#aaccff"; break;
        case EntityKind::Component:      color = "#bbeeaa"; break;
        case EntityKind::Battery:        color = "#ffffaa"; break;
        case EntityKind::Circuit:        color = "#eebbff"; break;
        case EntityKind::Assembly:       color = "#ccffee"; break;
        }
        os << "  \"" << id.to_string() << "\" [label=\""
           << entity_kind_to_string(n.kind) << "\\n" << n.name << "\", fillcolor=\"" << color << "\"];\n";
    }

    for (const auto& e : edges_) {
        os << "  \"" << e.source_id.to_string() << "\" -> \"" << e.target_id.to_string() << "\"";
        if (!e.label.empty() || e.kind != RelationKind::AssignedTo) {
            os << " [label=\"" << relation_kind_to_string(e.kind);
            if (!e.label.empty()) os << " (" << e.label << ")";
            os << "\"]";
        }
        os << ";\n";
    }
    os << "}\n";
    return os.str();
}

std::string EntityRelationshipGraph::to_json() const {
    std::ostringstream os;
    os << "{\n";
    os << "  \"nodes\": [\n";
    size_t i = 0;
    for (const auto& [id, n] : nodes_) {
        os << "    {\"id\": \"" << id.to_string() << "\", \"kind\": \""
           << entity_kind_to_string(n.kind) << "\", \"name\": \"" << n.name << "\"}";
        if (++i < nodes_.size()) os << ",";
        os << "\n";
    }
    os << "  ],\n";
    os << "  \"edges\": [\n";
    for (size_t j = 0; j < edges_.size(); ++j) {
        const auto& e = edges_[j];
        os << "    {\"source\": \"" << e.source_id.to_string() << "\", \"target\": \""
           << e.target_id.to_string() << "\", \"kind\": \""
           << relation_kind_to_string(e.kind) << "\", \"label\": \"" << e.label << "\"}";
        if (j + 1 < edges_.size()) os << ",";
        os << "\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os.str();
}

void EntityRelationshipGraph::rebuild_from_workspace(const EngineeringWorkspaceState& state) {
    clear();

    // 1. Materials
    for (const auto& [mat_id, mat] : state.material_registry().materials()) {
        add_node(EntityGraphNode{
            .id = mat_id,
            .kind = EntityKind::Material,
            .name = mat.name(),
            .metadata = mat.description()
        });
    }

    // 2. Mixtures
    for (const auto& mix : state.mixture_experiments()) {
        add_node(EntityGraphNode{
            .id = mix.id,
            .kind = EntityKind::Mixture,
            .name = mix.name,
            .metadata = std::string(mat::formation_scenario_to_string(mix.scenario))
        });
        // Connect constituent materials if known
        for (const auto& inp : mix.inputs) {
            const auto* m = state.material_registry().get_material(inp.symbol_or_id);
            if (m) {
                add_edge(m->id(), mix.id, RelationKind::ConstituentOf, inp.symbol_or_id);
            }
        }
    }

    // 3. Physical Objects
    for (const auto& obj : state.object_manager().objects()) {
        add_node(EntityGraphNode{
            .id = obj.id,
            .kind = EntityKind::PhysicalObject,
            .name = obj.name,
            .metadata = obj.category
        });
        // Edges from materials to object
        for (const auto& reg : obj.regions) {
            if (has_node(reg.material_id)) {
                add_edge(reg.material_id, obj.id, RelationKind::AssignedTo, reg.region_id);
            }
        }
    }

    // 4. Batteries
    for (const auto& bat : state.battery_specs()) {
        add_node(EntityGraphNode{
            .id = bat.id,
            .kind = EntityKind::Battery,
            .name = bat.name,
            .metadata = bat.chemistry_label
        });
    }

    // 5. Components
    for (const auto& comp : state.component_library().components()) {
        add_node(EntityGraphNode{
            .id = comp.id,
            .kind = EntityKind::Component,
            .name = comp.name,
            .metadata = std::string(component_kind_to_string(comp.kind))
        });
        if (has_node(comp.source_object_id)) {
            add_edge(comp.source_object_id, comp.id, RelationKind::GeneratedFrom, "Geometry");
        }
    }

    // 6. Circuits
    for (const auto& circ : state.circuits()) {
        // Deterministic circuit UUID based on name
        const core::base::Uuid circ_id = core::base::Uuid::generate_random();
        add_node(EntityGraphNode{
            .id = circ_id,
            .kind = EntityKind::Circuit,
            .name = circ.name(),
            .metadata = std::to_string(circ.components().size()) + " components"
        });

        for (const auto& inst : circ.components()) {
            if (inst.generated_component_id && has_node(*inst.generated_component_id)) {
                add_edge(*inst.generated_component_id, circ_id, RelationKind::ReferencedBy, inst.id);
            }
        }
    }
}

} // namespace shunya::eng
