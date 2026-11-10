#pragma once

#include "shunya/cad2d/curve_2d.hpp"

#include <string>
#include <vector>

namespace shunya::cad2d {

struct GridLine2D {
    double world_coord{0.0};
    double screen_coord{0.0};
    bool is_major{false};
};

struct AdaptiveGridResult {
    double minor_step{0.1};
    double major_step{1.0};
    std::vector<GridLine2D> vertical_lines;
    std::vector<GridLine2D> horizontal_lines;
};

/**
 * @brief Double-precision 2D sketch camera supporting sub-micron (1e-6) to kilometer (1e6) zooming without overflow.
 */
class SketchViewport2D {
public:
    Point2D pan_world{0.0, 0.0};       // World coordinate at screen center
    double pixels_per_unit{100.0};      // Zoom factor (e.g. 100 px per 1.0 world unit)
    double viewport_width{800.0};
    double viewport_height{600.0};

    [[nodiscard]] Point2D world_to_screen(const Point2D& world_pt) const noexcept;
    [[nodiscard]] Point2D screen_to_world(const Point2D& screen_pt) const noexcept;
    [[nodiscard]] BBox2D visible_world_bounds() const noexcept;

    void zoom_at_screen(const Point2D& screen_pt, double factor) noexcept;
    void pan_by_screen(double dx_px, double dy_px) noexcept;

    [[nodiscard]] AdaptiveGridResult compute_adaptive_grid(double min_minor_px = 14.0) const;
    [[nodiscard]] static std::string format_world_coordinate(double val_meters);
};

} // namespace shunya::cad2d
