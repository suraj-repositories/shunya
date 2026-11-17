#pragma once

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace shunya::cad2d {

enum class MarqueeMode {
    Window,   // Must be completely inside boundary
    Crossing  // Intersecting or inside boundary
};

enum class SelectionModifier {
    Replace,     // Clear current selection and set new
    ShiftToggle, // Toggle individual item in/out
    Add,         // Union with existing
    Remove       // Exclude from existing
};

enum class GizmoHandle {
    None,
    MoveBody,
    RotateHandle,
    ScaleTopLeft,
    ScaleTop,
    ScaleTopRight,
    ScaleRight,
    ScaleBottomRight,
    ScaleBottom,
    ScaleBottomLeft,
    ScaleLeft
};

[[nodiscard]] constexpr std::string_view gizmo_handle_to_string(GizmoHandle h) noexcept {
    switch (h) {
        case GizmoHandle::None:             return "None";
        case GizmoHandle::MoveBody:         return "MoveBody";
        case GizmoHandle::RotateHandle:     return "RotateHandle";
        case GizmoHandle::ScaleTopLeft:     return "ScaleTopLeft";
        case GizmoHandle::ScaleTop:         return "ScaleTop";
        case GizmoHandle::ScaleTopRight:    return "ScaleTopRight";
        case GizmoHandle::ScaleRight:       return "ScaleRight";
        case GizmoHandle::ScaleBottomRight: return "ScaleBottomRight";
        case GizmoHandle::ScaleBottom:      return "ScaleBottom";
        case GizmoHandle::ScaleBottomLeft:  return "ScaleBottomLeft";
        case GizmoHandle::ScaleLeft:        return "ScaleLeft";
    }
    return "None";
}

/**
 * @brief Selection and Bounding Box Transform Gizmo engine for 2D CAD.
 *
 * Supports Marquee (Window & Crossing), Polygon Lasso, Shift-click multi-selection,
 * and interactive bounding-box manipulation (Translation, Uniform/Non-uniform Scale, Rotation).
 */
class SelectionGizmo2D {
public:
    SelectionGizmo2D() = default;

    // Selection management
    void clear_selection() noexcept;
    void add_to_selection(std::string_view entity_id);
    void remove_from_selection(std::string_view entity_id);
    void toggle_selection(std::string_view entity_id);
    void set_selection(const std::vector<std::string>& entity_ids);
    [[nodiscard]] bool is_selected(std::string_view entity_id) const noexcept;
    [[nodiscard]] const std::vector<std::string>& selected_ids() const noexcept { return selected_ids_; }
    [[nodiscard]] size_t selection_count() const noexcept { return selected_ids_.size(); }

    // Hit-testing entities in document
    [[nodiscard]] std::string hit_test_entity(
        const SketchDocument2D& doc,
        const Point2D& pt,
        double tolerance = 5.0
    ) const;

    // Shift-click / direct click handler
    void apply_click(
        const SketchDocument2D& doc,
        const Point2D& pt,
        SelectionModifier modifier = SelectionModifier::Replace,
        double tolerance = 5.0
    );

    // Marquee rectangle selection (Auto-determines Window if p1.x >= p0.x, Crossing if p1.x < p0.x)
    std::vector<std::string> select_marquee(
        const SketchDocument2D& doc,
        const Point2D& corner1,
        const Point2D& corner2,
        MarqueeMode mode,
        SelectionModifier modifier = SelectionModifier::Replace
    );

    // Lasso polygon selection
    std::vector<std::string> select_lasso(
        const SketchDocument2D& doc,
        const std::vector<Point2D>& polygon,
        MarqueeMode mode,
        SelectionModifier modifier = SelectionModifier::Replace
    );

    // Bounding box & Gizmo geometry
    BBox2D update_bounds(const SketchDocument2D& doc);
    [[nodiscard]] const BBox2D& bounds() const noexcept { return current_bbox_; }
    [[nodiscard]] Point2D pivot_point() const noexcept;
    [[nodiscard]] Point2D rotate_handle_pos() const noexcept;
    [[nodiscard]] Point2D handle_pos(GizmoHandle handle) const noexcept;

    // Gizmo hit testing
    [[nodiscard]] GizmoHandle hit_test_gizmo(const Point2D& cursor, double handle_radius = 8.0) const noexcept;

    // Interactive transformation lifecycle
    bool begin_transform(GizmoHandle handle, const Point2D& cursor);
    [[nodiscard]] Transform2D update_transform(
        const Point2D& cursor,
        bool maintain_aspect = false,
        double angle_snap_rad = 0.0
    );
    bool apply_transform(SketchDocument2D& doc, const Transform2D& xf);
    void end_transform() noexcept;

    [[nodiscard]] bool is_transforming() const noexcept { return active_handle_ != GizmoHandle::None; }
    [[nodiscard]] GizmoHandle active_handle() const noexcept { return active_handle_; }

    void set_rotation_arm_length(double len) noexcept { rotation_arm_length_ = len; }
    [[nodiscard]] double rotation_arm_length() const noexcept { return rotation_arm_length_; }

private:
    void apply_selection_modifier(const std::vector<std::string>& matched_ids, SelectionModifier modifier);

    std::vector<std::string> selected_ids_;
    std::unordered_set<std::string> selected_set_;
    BBox2D current_bbox_{};

    // Transform session state
    GizmoHandle active_handle_{GizmoHandle::None};
    Point2D transform_start_cursor_{};
    Point2D transform_pivot_{};
    BBox2D initial_bbox_{};
    double rotation_arm_length_{25.0};
};

} // namespace shunya::cad2d
