#pragma once

#include "shunya/cad2d/curve_2d.hpp"

#include <optional>
#include <vector>

namespace shunya::cad2d {

struct PolarGuideResult {
    bool active{false};
    Point2D origin{};
    Point2D snapped_point{};
    Vec2d direction{1.0, 0.0};
    double snapped_angle_deg{0.0};
    double angular_error_deg{0.0};
};

/**
 * @brief Dynamic polar angle tracking engine emitting snapping guide rays (0, 30, 45, 60, 90 deg, etc.).
 */
class PolarTrackingEngine {
public:
    bool enabled{true};
    double snap_threshold_deg{2.5};
    std::vector<double> guide_angles_deg{
        0.0, 30.0, 45.0, 60.0, 90.0, 120.0, 135.0, 150.0,
        180.0, 210.0, 225.0, 240.0, 270.0, 300.0, 315.0, 330.0
    };

    void set_uniform_step_deg(double step_deg);

    /**
     * @brief Evaluates cursor_world relative to anchor origin and projects onto closest guide ray within threshold.
     */
    [[nodiscard]] PolarGuideResult evaluate(
        const Point2D& anchor,
        const Point2D& cursor_world
    ) const;
};

} // namespace shunya::cad2d
