#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/core/base/result.hpp"

#include <memory>
#include <string>
#include <vector>

namespace shunya::cad2d {

enum class TrimExtendMode {
    Trim,
    Extend,
    Eraser
};

struct TrimResult {
    std::string original_entity_id;
    std::vector<std::shared_ptr<Curve2D>> remaining_curves;
};

/**
 * @brief Interactive Eraser and Trim/Extend tool for 2D curve segmentation and cleanup.
 */
class TrimExtendEngine {
public:
    /**
     * @brief Trims target_entity at intersection points with boundary curves, removing the segment clicked at pick_pt.
     */
    [[nodiscard]] static core::base::Result<TrimResult> trim_curve(
        SketchDocument2D& doc,
        const std::string& target_entity_id,
        const Point2D& pick_pt,
        const std::vector<std::string>& boundary_entity_ids = {}
    );

    /**
     * @brief Extends target_entity along its end tangent to the nearest boundary curve.
     */
    [[nodiscard]] static core::base::Result<Point2D> extend_curve(
        SketchDocument2D& doc,
        const std::string& target_entity_id,
        const Point2D& pick_near_end,
        const std::vector<std::string>& boundary_entity_ids = {}
    );

    /**
     * @brief Erases an entity by ID from doc.
     */
    static bool erase_entity(SketchDocument2D& doc, const std::string& entity_id);

    /**
     * @brief Stroke eraser: removes all entities in doc intersecting the stroke polyline.
     */
    static size_t erase_along_stroke(SketchDocument2D& doc, const std::vector<Point2D>& stroke_pts, double radius = 0.5);

    /**
     * @brief Analytical intersections between curve A and curve B.
     */
    [[nodiscard]] static std::vector<Point2D> find_intersections(const Curve2D& a, const Curve2D& b);
};

/**
 * @brief Interactive controller state machine for Trim, Extend, and Eraser workflows.
 */
class TrimExtendTool {
public:
    TrimExtendMode mode{TrimExtendMode::Trim};

    void set_mode(TrimExtendMode m) noexcept { mode = m; }

    /**
     * @brief Handles user click action in world coordinates.
     */
    bool on_click(SketchDocument2D& doc, const Point2D& world_pt);
};

} // namespace shunya::cad2d
