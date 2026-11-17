#pragma once

#include "shunya/cad2d/construction_layer.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::cad2d {

enum class LinePattern {
    Solid,
    Dashed,
    Dotted,
    DashDot,
    Center
};

[[nodiscard]] constexpr std::string_view line_pattern_to_string(LinePattern p) noexcept {
    switch (p) {
        case LinePattern::Solid:   return "Solid";
        case LinePattern::Dashed:  return "Dashed";
        case LinePattern::Dotted:  return "Dotted";
        case LinePattern::DashDot: return "DashDot";
        case LinePattern::Center:  return "Center";
    }
    return "Solid";
}

[[nodiscard]] inline LinePattern string_to_line_pattern(std::string_view s) noexcept {
    if (s == "Dashed") return LinePattern::Dashed;
    if (s == "Dotted") return LinePattern::Dotted;
    if (s == "DashDot") return LinePattern::DashDot;
    if (s == "Center") return LinePattern::Center;
    return LinePattern::Solid;
}

struct LayerStyle {
    std::string color_hex{"#61afef"};
    double line_weight_mm{0.25};
    LinePattern pattern{LinePattern::Solid};
    double opacity{1.0}; // [0.0, 1.0]
};

struct Layer2D {
    std::string name;
    std::string parent_layer;
    bool visible{true};
    bool locked{false};
    LayerStyle style{};
    std::optional<std::string> color_override{std::nullopt};
    std::optional<double> line_weight_override{std::nullopt};
    double opacity{1.0}; // Local opacity factor [0.0, 1.0]
};

/**
 * @brief Hierarchical 2D CAD layer system managing visibility, edit locking, stroke styles,
 *        color overrides, line weights, and composite opacity inheritance.
 */
class LayerManager {
public:
    LayerManager();

    bool add_layer(std::string name, std::string parent_layer = "", LayerStyle style = {});
    bool remove_layer(std::string_view name);
    bool set_visible(std::string_view name, bool visible);
    bool set_locked(std::string_view name, bool locked);
    bool set_style(std::string_view name, const LayerStyle& style);
    bool set_parent(std::string_view name, std::string parent_layer);

    // Color and line weight overrides
    bool set_color_override(std::string_view name, std::optional<std::string> color_hex);
    bool set_line_weight_override(std::string_view name, std::optional<double> weight_mm);
    bool set_opacity(std::string_view name, double opacity);

    // Active layer
    [[nodiscard]] std::string_view active_layer() const noexcept { return active_layer_; }
    bool set_active_layer(std::string_view name);

    [[nodiscard]] const Layer2D* find_layer(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<const Layer2D*> ordered_layers() const;
    [[nodiscard]] size_t layer_count() const noexcept { return layers_.size(); }

    // Hierarchy navigation
    [[nodiscard]] std::vector<std::string> get_children(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> get_ancestors(std::string_view name) const;

    // Layer reordering
    bool move_layer_up(std::string_view name);
    bool move_layer_down(std::string_view name);
    bool reorder_layer(std::string_view name, size_t target_index);

    /**
     * @brief Returns true iff this layer AND all ancestor layers are visible.
     */
    [[nodiscard]] bool is_effectively_visible(std::string_view name) const noexcept;

    /**
     * @brief Returns true iff this layer OR any ancestor layer is locked.
     */
    [[nodiscard]] bool is_effectively_locked(std::string_view name) const noexcept;

    /**
     * @brief Computes cumulative opacity: local opacity * product of ancestor opacities.
     */
    [[nodiscard]] double effective_opacity(std::string_view name) const noexcept;

    /**
     * @brief Resolves effective color: explicit override, or style color, or inherits from ancestor.
     */
    [[nodiscard]] std::string effective_color(std::string_view name) const;

    /**
     * @brief Resolves effective line weight in mm: explicit override or inherited.
     */
    [[nodiscard]] double effective_line_weight(std::string_view name) const noexcept;

    /**
     * @brief Checks whether a SketchEntity on its assigned layer can be edited (visible & unlocked).
     */
    [[nodiscard]] bool can_modify_entity(const SketchEntity& entity) const noexcept;

    /**
     * @brief Reassigns all entities on from_layer to to_layer inside SketchDocument2D; returns count moved.
     */
    size_t reassign_entities(SketchDocument2D& doc, std::string_view from_layer, std::string_view to_layer);

    // Serialization & Deserialization
    [[nodiscard]] std::string to_json() const;
    bool from_json(std::string_view json_str);

private:
    bool would_create_cycle(std::string_view layer, std::string_view proposed_parent) const noexcept;
    Layer2D* find_layer_mut(std::string_view name) noexcept;

    std::vector<Layer2D> layers_;
    std::string active_layer_{"0"};
};

} // namespace shunya::cad2d
