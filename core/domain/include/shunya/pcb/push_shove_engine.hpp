#pragma once

#include "shunya/core/base/math.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

struct PcbTrackSegment {
    std::string net_name;
    int layer_index{0};
    double width_mm{0.25};
    core::base::math::Vector2<double> start_mm;
    core::base::math::Vector2<double> end_mm;
    bool locked{false};
};

struct PushShoveOptions {
    double min_clearance_mm{0.15};
    int max_iterations{24};
};

/**
 * @brief Spring-repulsion local push-and-shove solver enforcing minimum track-to-track clearances.
 */
class PushShoveEngine {
public:
    /**
     * @brief Displaces unlocked tracks perpendicular to the intruder track until clearance is restored.
     */
    [[nodiscard]] static std::vector<PcbTrackSegment> shove_parallel_tracks(
        const PcbTrackSegment& intruder,
        std::vector<PcbTrackSegment> neighbors,
        const PushShoveOptions& opts = {});
};

} // namespace shunya::pcb
