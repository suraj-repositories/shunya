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

enum class NetLabelType {
    Local,            // Normal in-sheet net name
    Global,           // Global sheet power / rail (VCC, GND)
    HierarchicalPort  // Off-page connector / block port
};

[[nodiscard]] constexpr std::string_view net_label_type_to_string(NetLabelType t) noexcept {
    switch (t) {
        case NetLabelType::Local:            return "Local";
        case NetLabelType::Global:           return "Global";
        case NetLabelType::HierarchicalPort: return "HierarchicalPort";
    }
    return "Local";
}

struct NetLabel {
    std::string label_id;
    std::string name;
    Point2D position;
    NetLabelType type{NetLabelType::Local};
    std::vector<std::string> graphic_entity_ids;
};

struct BusDefinition {
    std::string bus_id;
    std::string prefix;
    int start_index{0};
    int end_index{0};
    std::string raw_spec; // e.g. "DATA[0..7]"
    std::vector<std::string> member_nets;
    std::vector<Point2D> waypoints;
    std::vector<std::string> graphic_entity_ids;

    [[nodiscard]] bool contains_net(std::string_view net_name) const noexcept;
};

struct BusTap {
    std::string tap_id;
    std::string bus_id;
    std::string member_net;
    Point2D bus_contact_pos;
    Point2D tap_end_pos; // 45-deg slash or stub
    std::vector<std::string> graphic_entity_ids;
};

/**
 * @brief Schematic Netlabel callout and Hierarchical Bus connector manager.
 *
 * Facilitates wireless net propagation via textual callouts and multi-signal
 * bus distribution with 45-degree tap breakouts.
 */
class SchematicNetlabelBusManager {
public:
    SchematicNetlabelBusManager() = default;

    // Netlabel Callout operations
    std::string place_netlabel(
        SketchDocument2D& doc,
        Point2D anchor,
        std::string net_name,
        NetLabelType type = NetLabelType::Local
    );
    bool remove_netlabel(SketchDocument2D& doc, std::string_view label_id);

    // Hierarchical Bus operations
    std::string place_bus(
        SketchDocument2D& doc,
        const std::vector<Point2D>& waypoints,
        std::string bus_spec // e.g. "DATA[0..7]" or "ADDR[0..15]"
    );
    bool remove_bus(SketchDocument2D& doc, std::string_view bus_id);

    // Bus Tap operations (breakout single net from bus)
    std::string place_bus_tap(
        SketchDocument2D& doc,
        std::string_view bus_id,
        Point2D bus_pos,
        std::string member_net,
        double tap_length_mm = 2.54,
        bool angle_45_deg = true
    );
    bool remove_bus_tap(SketchDocument2D& doc, std::string_view tap_id);

    // Parsing helper
    [[nodiscard]] static std::optional<BusDefinition> parse_bus_spec(std::string_view spec);

    // Queries
    [[nodiscard]] const NetLabel* find_netlabel(std::string_view label_id) const noexcept;
    [[nodiscard]] const BusDefinition* find_bus(std::string_view bus_id) const noexcept;
    [[nodiscard]] const BusTap* find_bus_tap(std::string_view tap_id) const noexcept;

    [[nodiscard]] const std::vector<NetLabel>& all_netlabels() const noexcept { return labels_; }
    [[nodiscard]] const std::vector<BusDefinition>& all_buses() const noexcept { return buses_; }
    [[nodiscard]] const std::vector<BusTap>& all_bus_taps() const noexcept { return bus_taps_; }

    [[nodiscard]] size_t netlabel_count() const noexcept { return labels_.size(); }
    [[nodiscard]] size_t bus_count() const noexcept { return buses_.size(); }
    [[nodiscard]] size_t bus_tap_count() const noexcept { return bus_taps_.size(); }

    [[nodiscard]] std::vector<Point2D> find_all_positions_for_net(std::string_view net_name) const;

    void clear(SketchDocument2D& doc);

private:
    uint64_t next_label_idx_{1};
    uint64_t next_bus_idx_{1};
    uint64_t next_tap_idx_{1};

    std::vector<NetLabel> labels_;
    std::vector<BusDefinition> buses_;
    std::vector<BusTap> bus_taps_;
};

} // namespace shunya::cad2d
