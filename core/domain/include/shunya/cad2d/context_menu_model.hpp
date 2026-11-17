#pragma once

#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/group_manager.hpp"
#include "shunya/cad2d/layer_manager.hpp"
#include "shunya/cad2d/selection_gizmo.hpp"
#include "shunya/cad2d/sketch_commands.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::cad2d {

enum class ContextMenuContext {
    EmptyCanvas,
    SingleEntity,
    MultiEntity,
    Group
};

struct MenuItem2D {
    std::string id;
    std::string label;
    std::string shortcut;
    std::string icon;
    bool enabled{true};
    std::optional<bool> checked{std::nullopt};
    bool is_separator{false};
    std::vector<MenuItem2D> subitems;

    [[nodiscard]] static MenuItem2D separator() {
        MenuItem2D item;
        item.is_separator = true;
        return item;
    }
};

/**
 * @brief Dynamic right-click contextual menu generator and dispatcher for the 2D CAD environment.
 *
 * Evaluates current canvas hit-test, active selection, layers, and grouping to build
 * tailored menus (Properties, Transformations, Layer Assignment, Conversion, Clipboard).
 */
class ContextMenuModel2D {
public:
    ContextMenuModel2D() = default;

    /**
     * @brief Generates menu items tree based on the click position and current editor state.
     */
    [[nodiscard]] std::vector<MenuItem2D> build_menu(
        const SketchDocument2D& doc,
        const LayerManager& layer_mgr,
        const GroupManager& group_mgr,
        const SelectionGizmo2D& selection_gizmo,
        Point2D click_pos,
        bool has_clipboard_data = false
    ) const;

    /**
     * @brief Dispatches and executes a contextual action by its action ID.
     */
    bool execute_action(
        std::string_view action_id,
        SketchDocument2D& doc,
        LayerManager& layer_mgr,
        GroupManager& group_mgr,
        SelectionGizmo2D& selection_gizmo,
        SketchCommandHistory2D* history = nullptr
    );

    [[nodiscard]] ContextMenuContext determine_context(
        const SketchDocument2D& doc,
        const GroupManager& group_mgr,
        const SelectionGizmo2D& selection_gizmo,
        Point2D click_pos
    ) const;
};

} // namespace shunya::cad2d
