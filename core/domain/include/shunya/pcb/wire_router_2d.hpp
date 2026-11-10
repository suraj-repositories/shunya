#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/pcb/schematic_sheet.hpp"

#include <vector>

namespace shunya::pcb {

struct OrthogonalRouteResult {
    std::vector<core::base::math::Vector2<double>> waypoints_mm;
    std::vector<WireSegment2D> segments;
    std::vector<core::base::math::Vector2<double>> inserted_junctions;
};

/**
 * @brief A* 90-degree orthogonal schematic wire router avoiding component obstacles with T-junction detection.
 */
class OrthogonalWireRouter {
public:
    /**
     * @brief Routes a strictly orthogonal path between start and goal on the given grid pitch.
     */
    [[nodiscard]] static core::base::Result<OrthogonalRouteResult> route(
        const core::base::math::Vector2<double>& start_mm,
        const core::base::math::Vector2<double>& goal_mm,
        const std::vector<SchematicComponent>& obstacles,
        double grid_pitch_mm = 2.54,
        double margin_mm = 1.27);

    /**
     * @brief Detects and returns T-junction and 4-way intersection points across schematic wire segments.
     */
    [[nodiscard]] static std::vector<core::base::math::Vector2<double>> detect_junctions(
        const std::vector<WireSegment2D>& segments);
};

} // namespace shunya::pcb
