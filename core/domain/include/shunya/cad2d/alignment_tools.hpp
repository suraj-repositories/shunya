#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/core/base/result.hpp"

#include <string>
#include <vector>

namespace shunya::cad2d {

enum class AlignmentMode {
    AlignLeft,
    AlignCenterHorizontal,
    AlignRight,
    AlignTop,
    AlignMiddleVertical,
    AlignBottom
};

enum class DistributionMode {
    DistributeHorizontally,
    DistributeVertically,
    DistributeCentersHorizontally,
    DistributeCentersVertically
};

/**
 * @brief Helper to translate arbitrary Curve2D primitives by (dx, dy).
 */
class CurveTransformHelper {
public:
    static bool translate_curve(Curve2D& curve, const Vec2d& delta);
};

/**
 * @brief Multi-object alignment and distribution engine for 2D drafting entities.
 */
class AlignmentEngine {
public:
    /**
     * @brief Aligns selected entities along specified boundary or center axis.
     * @param doc Target sketch document containing the entities.
     * @param entity_ids IDs of entities to align (minimum 2).
     * @param mode Alignment type (Left, Center, Right, Top, Middle, Bottom).
     * @param anchor_entity_id Optional reference entity to stay fixed; if empty, uses collective bounding box.
     * @return Number of entities translated.
     */
    static core::base::Result<size_t> align_entities(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        AlignmentMode mode,
        const std::string& anchor_entity_id = ""
    );

    /**
     * @brief Evenly distributes selected entities along horizontal or vertical axes.
     * @param doc Target sketch document containing the entities.
     * @param entity_ids IDs of entities to distribute (minimum 3).
     * @param mode Distribution mode (equidistant gaps or equidistant centers).
     * @return Number of entities distributed.
     */
    static core::base::Result<size_t> distribute_entities(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        DistributionMode mode
    );
};

} // namespace shunya::cad2d
