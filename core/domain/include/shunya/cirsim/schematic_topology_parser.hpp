#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace shunya::cirsim {

using Point2D = core::base::math::Vector2<double>;

struct SchematicPinAnchor {
    std::string component_id;
    std::string pin_id;
    int pin_number{1};
    Point2D location{0.0, 0.0};
    core::domain::ElectricalPinPolarity polarity{core::domain::ElectricalPinPolarity::PassiveBiDirectional};
};

struct SchematicWire {
    std::string wire_id;
    Point2D start{0.0, 0.0};
    Point2D end{0.0, 0.0};
    std::string user_net_label; // e.g. "GND", "VCC", "CLK", or empty
};

struct SchematicJunctionDot {
    Point2D position{0.0, 0.0};
};

struct CircuitNodeInfo {
    uint32_t node_index{0};       // 0 is ground
    std::string node_name;        // "0" or "GND", "NET_1", etc.
    bool is_ground{false};
    std::vector<std::string> connected_component_pins; // "R1:1", "C1:2", etc.
    std::vector<Point2D> junction_points;
};

struct TopologicalDiagnostic {
    enum class Severity { Info, Warning, Error };
    Severity severity{Severity::Info};
    std::string code;
    std::string message;
    std::vector<std::string> affected_elements;
};

struct ParsedSchematicTopology {
    std::unordered_map<uint32_t, CircuitNodeInfo> nodes;
    std::unordered_map<std::string, uint32_t> pin_to_node_map; // "R1:1" -> node_index
    std::vector<TopologicalDiagnostic> diagnostics;
    std::vector<std::vector<std::string>> detected_loops;
    bool has_ground_reference{false};
    size_t floating_node_count{0};
};

/**
 * @brief High-speed Disjoint Set Union (DSU) graph parser for electronic schematics.
 * Fulfills roadmap tasks PH4-08.01.001 and PH4-08.01.002.
 */
class SchematicTopologyParser {
public:
    explicit SchematicTopologyParser(double snap_tolerance = 1.0e-3);

    void add_component_pin(const SchematicPinAnchor& pin);
    void add_wire(const SchematicWire& wire);
    void add_junction(const SchematicJunctionDot& junction);
    void clear() noexcept;

    /**
     * @brief Parses connectivity graph and compiles mapped electrical nodes.
     */
    [[nodiscard]] ParsedSchematicTopology parse();

    /**
     * @brief Detects closed topological loops / cycles in component-node graph.
     */
    [[nodiscard]] static std::vector<std::vector<std::string>> detect_topological_loops(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components
    );

    /**
     * @brief Identifies floating or under-connected nodes (pins <= 1).
     */
    [[nodiscard]] static std::vector<uint32_t> find_floating_nodes(
        const ParsedSchematicTopology& topology
    );

private:
    double snap_tolerance_{1.0e-3};
    std::vector<SchematicPinAnchor> pins_;
    std::vector<SchematicWire> wires_;
    std::vector<SchematicJunctionDot> junctions_;
};

} // namespace shunya::cirsim
