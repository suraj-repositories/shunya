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

struct LayerStyle {
    std::string color_hex{"#61afef"};
    double line_weight_mm{0.25};
    LinePattern pattern{LinePattern::Solid};
};

struct Layer2D {
    std::string name;
    std::string parent_layer;
    bool visible{true};
    bool locked{false};
    LayerStyle style{};
};

/**
 * @brief Hierarchical 2D CAD layer system managing visibility, edit locking, stroke styles, and inheritance.
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

    [[nodiscard]] const Layer2D* find_layer(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<const Layer2D*> ordered_layers() const;
    [[nodiscard]] size_t layer_count() const noexcept { return layers_.size(); }

    /**
     * @brief Returns true iff this layer AND all ancestor layers are visible.
     */
    [[nodiscard]] bool is_effectively_visible(std::string_view name) const noexcept;

    /**
     * @brief Returns true iff this layer OR any ancestor layer is locked.
     */
    [[nodiscard]] bool is_effectively_locked(std::string_view name) const noexcept;

    /**
     * @brief Checks whether a SketchEntity on its assigned layer can be edited (visible & unlocked).
     */
    [[nodiscard]] bool can_modify_entity(const SketchEntity& entity) const noexcept;

    /**
     * @brief Reassigns all entities on from_layer to to_layer inside SketchDocument2D; returns count moved.
     */
    size_t reassign_entities(SketchDocument2D& doc, std::string_view from_layer, std::string_view to_layer);

private:
    bool would_create_cycle(std::string_view layer, std::string_view proposed_parent) const noexcept;

    std::vector<Layer2D> layers_;
};

} // namespace shunya::cad2d
