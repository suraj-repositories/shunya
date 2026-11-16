#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace shunya::eng {

// Forward declaration
class EngineeringWorkspaceState;

enum class EntityKind : uint8_t {
    Element,
    Material,
    Mixture,
    PhysicalObject,
    Component,
    Battery,
    Circuit,
    Assembly
};

[[nodiscard]] constexpr std::string_view entity_kind_to_string(EntityKind k) noexcept {
    switch (k) {
    case EntityKind::Element:        return "Element";
    case EntityKind::Material:       return "Material";
    case EntityKind::Mixture:        return "Mixture";
    case EntityKind::PhysicalObject: return "PhysicalObject";
    case EntityKind::Component:      return "Component";
    case EntityKind::Battery:        return "Battery";
    case EntityKind::Circuit:        return "Circuit";
    case EntityKind::Assembly:       return "Assembly";
    }
    return "Unknown";
}

enum class RelationKind : uint8_t {
    Contains,
    ConstituentOf,
    AssignedTo,
    GeneratedFrom,
    ReferencedBy,
    ElectricallyConnectedTo,
    Powers
};

[[nodiscard]] constexpr std::string_view relation_kind_to_string(RelationKind r) noexcept {
    switch (r) {
    case RelationKind::Contains:                return "Contains";
    case RelationKind::ConstituentOf:           return "ConstituentOf";
    case RelationKind::AssignedTo:              return "AssignedTo";
    case RelationKind::GeneratedFrom:           return "GeneratedFrom";
    case RelationKind::ReferencedBy:            return "ReferencedBy";
    case RelationKind::ElectricallyConnectedTo: return "ElectricallyConnectedTo";
    case RelationKind::Powers:                  return "Powers";
    }
    return "Unknown";
}

struct EntityGraphNode {
    core::base::Uuid id;
    EntityKind kind{EntityKind::Material};
    std::string name;
    std::string metadata;
};

struct EntityGraphEdge {
    core::base::Uuid source_id;
    core::base::Uuid target_id;
    RelationKind kind{RelationKind::AssignedTo};
    std::string label;

    bool operator==(const EntityGraphEdge& other) const noexcept {
        return source_id == other.source_id &&
               target_id == other.target_id &&
               kind == other.kind;
    }
};

/**
 * @brief Multi-domain Entity-Relationship Graph (PH4-01.03.005)
 *
 * Models and manages directed acyclic dependency relationships connecting
 * Materials, Physical Objects, Electronic Components, Schematics, and Assemblies.
 * Provides transitive dependency invalidation, orphaned edge detection,
 * topological ordering, and Graphviz DOT visualization.
 */
class EntityRelationshipGraph {
public:
    EntityRelationshipGraph() = default;

    void clear() noexcept;

    // Node management
    bool add_node(EntityGraphNode node);
    bool remove_node(const core::base::Uuid& id);
    [[nodiscard]] bool has_node(const core::base::Uuid& id) const noexcept;
    [[nodiscard]] const EntityGraphNode* find_node(const core::base::Uuid& id) const noexcept;
    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] const std::unordered_map<core::base::Uuid, EntityGraphNode>& nodes() const noexcept { return nodes_; }

    // Edge management (directed: source -> target)
    bool add_edge(core::base::Uuid source_id,
                  core::base::Uuid target_id,
                  RelationKind kind,
                  std::string label = "");
    bool remove_edge(const core::base::Uuid& source_id,
                     const core::base::Uuid& target_id,
                     RelationKind kind);
    [[nodiscard]] bool has_edge(const core::base::Uuid& source_id,
                                const core::base::Uuid& target_id,
                                RelationKind kind) const noexcept;
    [[nodiscard]] size_t edge_count() const noexcept { return edges_.size(); }
    [[nodiscard]] const std::vector<EntityGraphEdge>& edges() const noexcept { return edges_; }

    // Neighbor queries
    [[nodiscard]] std::vector<EntityGraphEdge> get_outgoing_edges(const core::base::Uuid& id) const;
    [[nodiscard]] std::vector<EntityGraphEdge> get_incoming_edges(const core::base::Uuid& id) const;
    [[nodiscard]] std::vector<core::base::Uuid> get_direct_dependents(const core::base::Uuid& id) const;
    [[nodiscard]] std::vector<core::base::Uuid> get_direct_dependencies(const core::base::Uuid& id) const;

    // Transitive queries
    [[nodiscard]] std::vector<core::base::Uuid> get_transitive_dependents(const core::base::Uuid& id) const;
    [[nodiscard]] std::vector<core::base::Uuid> get_transitive_dependencies(const core::base::Uuid& id) const;

    // Integrity and validation
    [[nodiscard]] std::vector<EntityGraphEdge> find_orphaned_edges() const;
    [[nodiscard]] bool has_cycles() const;
    [[nodiscard]] core::base::Result<std::vector<core::base::Uuid>> topological_sort() const;

    // Export formats
    [[nodiscard]] std::string to_dot() const;
    [[nodiscard]] std::string to_json() const;

    // Rebuild from entire workspace state
    void rebuild_from_workspace(const EngineeringWorkspaceState& state);

private:
    std::unordered_map<core::base::Uuid, EntityGraphNode> nodes_;
    std::vector<EntityGraphEdge> edges_;
    std::unordered_map<core::base::Uuid, std::vector<size_t>> outgoing_adj_;
    std::unordered_map<core::base::Uuid, std::vector<size_t>> incoming_adj_;

    void rebuild_adjacency_indices();
};

} // namespace shunya::eng
