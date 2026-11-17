#pragma once

#include "shunya/cad2d/curve_2d.hpp"

#include <optional>
#include <vector>
#include <string>

namespace shunya::cad2d {

enum class PolarIncrementPreset {
    Deg15,
    Deg30,
    Deg45,
    Deg90,
    Custom
};

struct PolarGuideResult {
    bool active{false};
    Point2D origin{};
    Point2D snapped_point{};
    Vec2d direction{1.0, 0.0};
    double distance{0.0};
    double snapped_angle_deg{0.0};
    double angular_error_deg{0.0};
    std::string hud_tooltip;
};

/**
 * @brief Dynamic polar angle tracking engine with customizable angular increments (15°, 30°, 45°, 90°),
 * custom auxiliary angles, relative tracking, and visual guide ray generation.
 */
class PolarTrackingEngine {
public:
    bool enabled{true};
    double snap_threshold_deg{2.5};
    PolarIncrementPreset current_preset{PolarIncrementPreset::Deg30};
    std::vector<double> guide_angles_deg;
    std::vector<double> additional_custom_angles_deg;
    bool track_relative_to_segment{false};
    double reference_orientation_deg{0.0};

    PolarTrackingEngine();

    void set_preset(PolarIncrementPreset preset);
    void set_uniform_step_deg(double step_deg);
    void add_custom_angle(double angle_deg);
    void clear_custom_angles() noexcept;

    /**
     * @brief Sets an alignment reference angle (e.g. tangent direction of preceding segment).
     */
    void set_reference_orientation(double angle_deg) noexcept {
        reference_orientation_deg = angle_deg;
    }

    /**
     * @brief Evaluates cursor_world relative to anchor origin and projects onto closest guide ray within threshold.
     */
    [[nodiscard]] PolarGuideResult evaluate(
        const Point2D& anchor,
        const Point2D& cursor_world
    ) const;

    /**
     * @brief Generates infinite/extended visual guide ray line segment for viewport rendering.
     */
    [[nodiscard]] static LineSegment2D generate_visual_guide_ray(
        const PolarGuideResult& result,
        double ray_length = 2000.0
    );

private:
    void rebuild_guide_angles();
};

} // namespace shunya::cad2d
