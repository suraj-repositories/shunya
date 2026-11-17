#pragma once

#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/schematic_symbol_engine.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::cad2d {

enum class WireRouteStyle {
    Orthogonal_HV, // Horizontal then Vertical
    Orthogonal_VH, // Vertical then Horizontal
    Octilinear_45, // 45-degree chamfer / diagonal routing
    Direct         // Straight point-to-point
};

struct SchematicWire {
    std::string wire_id;
    std::string net_name;
    std::vector<Point2D> points;
    std::vector<std::string> entity_ids; // Segments in SketchDocument2D
};

struct JunctionDot {
    std::string junction_id;
    Point2D position;
    std::string net_name;
    double radius_mm{0.8};
    std::vector<std::string> connected_wire_ids;
    std::string dot_entity_id; // Circle entity in SketchDocument2D
};

/**
 * @brief Intelligent 2D schematic wire routing engine supporting Orthogonal (90-deg)
 *        and 45-degree octilinear routing with automated 3-way T-junction dot synthesis.
 */
class SchematicWireRouter {
public:
    explicit SchematicWireRouter(SchematicDraftingEngine* symbol_engine = nullptr);

    /**
     * @brief Computes waypoints between start and target for interactive preview and placement.
     */
    [[nodiscard]] static std::vector<Point2D> calculate_route(
        Point2D start,
        Point2D target,
        WireRouteStyle style = WireRouteStyle::Orthogonal_HV,
        const std::vector<BBox2D>& obstacles = {}
    );

    /**
     * @brief Commits a new schematic wire polyline to the document, resolving net connectivity
     * and automatically creating junction dots at 3-way or higher intersections.
     */
    std::string add_wire(
        SketchDocument2D& doc,
        const std::vector<Point2D>& waypoints,
        std::string preferred_net_name = ""
    );

    /**
     * @brief Removes a wire from document, cleaning up segments and updating junction dots.
     */
    bool remove_wire(SketchDocument2D& doc, std::string_view wire_id);

    /**
     * @brief Re-evaluates all wire intersections and updates junction dots automatically.
     */
    void update_junctions(SketchDocument2D& doc);

    [[nodiscard]] const std::vector<SchematicWire>& wires() const noexcept { return wires_; }
    [[nodiscard]] const std::vector<JunctionDot>& junction_dots() const noexcept { return junctions_; }
    [[nodiscard]] size_t wire_count() const noexcept { return wires_.size(); }
    [[nodiscard]] size_t junction_count() const noexcept { return junctions_.size(); }

    [[nodiscard]] const SchematicWire* find_wire(std::string_view wire_id) const noexcept;
    [[nodiscard]] std::string net_at_point(Point2D pt, double tol = 0.5) const;

    void clear(SketchDocument2D& doc);

private:
    SchematicDraftingEngine* symbol_engine_{nullptr};
    uint64_t next_wire_idx_{1};
    uint64_t next_junction_idx_{1};
    uint64_t next_net_idx_{1};

    std::vector<SchematicWire> wires_;
    std::vector<JunctionDot> junctions_;
};

} // namespace shunya::cad2d
