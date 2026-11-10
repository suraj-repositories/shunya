#include "shunya/cad2d/sketch_viewport_2d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace shunya::cad2d {

Point2D SketchViewport2D::world_to_screen(const Point2D& world_pt) const noexcept {
    double cx = viewport_width * 0.5;
    double cy = viewport_height * 0.5;
    double sx = cx + (world_pt.x - pan_world.x) * pixels_per_unit;
    double sy = cy - (world_pt.y - pan_world.y) * pixels_per_unit;
    return {sx, sy};
}

Point2D SketchViewport2D::screen_to_world(const Point2D& screen_pt) const noexcept {
    double cx = viewport_width * 0.5;
    double cy = viewport_height * 0.5;
    double inv = (pixels_per_unit > 1e-18) ? (1.0 / pixels_per_unit) : 1.0;
    double wx = pan_world.x + (screen_pt.x - cx) * inv;
    double wy = pan_world.y - (screen_pt.y - cy) * inv;
    return {wx, wy};
}

BBox2D SketchViewport2D::visible_world_bounds() const noexcept {
    double inv = (pixels_per_unit > 1e-18) ? (1.0 / pixels_per_unit) : 1.0;
    double half_w = viewport_width * 0.5 * inv;
    double half_h = viewport_height * 0.5 * inv;
    return {{pan_world.x - half_w, pan_world.y - half_h}, {pan_world.x + half_w, pan_world.y + half_h}};
}

void SketchViewport2D::zoom_at_screen(const Point2D& screen_pt, double factor) noexcept {
    if (factor <= 0.0 || !std::isfinite(factor)) return;
    Point2D anchor_world = screen_to_world(screen_pt);
    pixels_per_unit = std::clamp(pixels_per_unit * factor, 1e-4, 1e11);

    // Recompute pan_world so anchor_world stays pinned under screen_pt
    double cx = viewport_width * 0.5;
    double cy = viewport_height * 0.5;
    double inv = 1.0 / pixels_per_unit;
    pan_world.x = anchor_world.x - (screen_pt.x - cx) * inv;
    pan_world.y = anchor_world.y + (screen_pt.y - cy) * inv;
}

void SketchViewport2D::pan_by_screen(double dx_px, double dy_px) noexcept {
    double inv = (pixels_per_unit > 1e-18) ? (1.0 / pixels_per_unit) : 1.0;
    pan_world.x -= dx_px * inv;
    pan_world.y += dy_px * inv;
}

AdaptiveGridResult SketchViewport2D::compute_adaptive_grid(double min_minor_px) const {
    AdaptiveGridResult res;
    if (viewport_width <= 0.0 || viewport_height <= 0.0 || pixels_per_unit <= 1e-18) {
        return res;
    }

    double min_world_step = std::max(1.0, min_minor_px) / pixels_per_unit;
    double exp_v = std::floor(std::log10(min_world_step));
    double base = std::pow(10.0, exp_v);
    double normalized = min_world_step / base;

    double chosen_mult = 1.0;
    if (normalized <= 1.0) chosen_mult = 1.0;
    else if (normalized <= 2.0) chosen_mult = 2.0;
    else if (normalized <= 5.0) chosen_mult = 5.0;
    else chosen_mult = 10.0;

    res.minor_step = chosen_mult * base;
    res.major_step = res.minor_step * 5.0;

    BBox2D vis = visible_world_bounds();

    int64_t ix_start = static_cast<int64_t>(std::floor(vis.min.x / res.minor_step));
    int64_t ix_end = static_cast<int64_t>(std::ceil(vis.max.x / res.minor_step));
    if (ix_end - ix_start < 500) {
        for (int64_t i = ix_start; i <= ix_end; ++i) {
            double wx = static_cast<double>(i) * res.minor_step;
            double sx = world_to_screen({wx, pan_world.y}).x;
            bool major = (i % 5 == 0);
            res.vertical_lines.push_back({wx, sx, major});
        }
    }

    int64_t iy_start = static_cast<int64_t>(std::floor(vis.min.y / res.minor_step));
    int64_t iy_end = static_cast<int64_t>(std::ceil(vis.max.y / res.minor_step));
    if (iy_end - iy_start < 500) {
        for (int64_t i = iy_start; i <= iy_end; ++i) {
            double wy = static_cast<double>(i) * res.minor_step;
            double sy = world_to_screen({pan_world.x, wy}).y;
            bool major = (i % 5 == 0);
            res.horizontal_lines.push_back({wy, sy, major});
        }
    }

    return res;
}

std::string SketchViewport2D::format_world_coordinate(double val_meters) {
    char buf[64];
    double abs_v = std::abs(val_meters);
    if (abs_v < 1e-12) {
        return "0.00 mm";
    } else if (abs_v < 1e-3) {
        std::snprintf(buf, sizeof(buf), "%.2f um", val_meters * 1e6);
    } else if (abs_v < 1.0) {
        std::snprintf(buf, sizeof(buf), "%.3f mm", val_meters * 1e3);
    } else if (abs_v < 1000.0) {
        std::snprintf(buf, sizeof(buf), "%.4f m", val_meters);
    } else {
        std::snprintf(buf, sizeof(buf), "%.4f km", val_meters * 1e-3);
    }
    return std::string(buf);
}

} // namespace shunya::cad2d
