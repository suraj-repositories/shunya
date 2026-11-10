#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/pcb/push_shove_engine.hpp"

#include <vector>

namespace shunya::pcb {

/**
 * @brief Interactive 45-degree octilinear PCB trace router with clearance validation.
 */
class InteractiveTraceRouter {
public:
    /**
     * @brief Constructs a strictly octilinear (0/45/90 deg) 2-segment path between start and end.
     */
    [[nodiscard]] static std::vector<PcbTrackSegment> route_octilinear_pair(
        std::string net_name,
        int layer_index,
        double width_mm,
        const core::base::math::Vector2<double>& start_mm,
        const core::base::math::Vector2<double>& end_mm,
        bool miter_first = true);

    /**
     * @brief Verifies that segment orientation is a multiple of 45 degrees.
     */
    [[nodiscard]] static bool is_45_deg_aligned(const PcbTrackSegment& seg, double tol = 1e-4) noexcept;

    /**
     * @brief Minimum Euclidean distance between two segments (centerline minus half-widths).
     */
    [[nodiscard]] static double edge_clearance_mm(const PcbTrackSegment& a,
                                                  const PcbTrackSegment& b) noexcept;
};

} // namespace shunya::pcb
