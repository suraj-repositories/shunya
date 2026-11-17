#include "shunya/cad2d/sketch_viewport_2d.hpp"
#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace shunya::cad2d {

namespace {

using shunya::core::base::math::PI;

} // namespace

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
    auto dyn = compute_dynamic_grid(min_minor_px);
    AdaptiveGridResult res;
    res.minor_step = dyn.minor_step;
    res.major_step = dyn.major_step;
    res.vertical_lines = std::move(dyn.vertical_lines);
    res.horizontal_lines = std::move(dyn.horizontal_lines);
    return res;
}

DynamicGridResult SketchViewport2D::compute_dynamic_grid(double min_minor_px) const {
    DynamicGridResult res;
    res.mode = grid_mode;
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

    double actual_px = res.minor_step * pixels_per_unit;
    res.fade_alpha = std::clamp((actual_px - min_minor_px) / (min_minor_px * 1.5), 0.2, 1.0);

    BBox2D vis = visible_world_bounds();

    if (grid_mode == GridMode::Cartesian) {
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
    } else if (grid_mode == GridMode::Isometric) {
        // Isometric 30-degree grid
        double iso_rad = isometric_angle_deg * (PI / 180.0);
        double cos_iso = std::cos(iso_rad);
        double sin_iso = std::sin(iso_rad);
        double tan_iso = sin_iso / cos_iso;

        // 1. Vertical grid lines
        double vert_step = res.minor_step * cos_iso;
        if (vert_step > 1e-6) {
            int64_t iv_start = static_cast<int64_t>(std::floor(vis.min.x / vert_step));
            int64_t iv_end = static_cast<int64_t>(std::ceil(vis.max.x / vert_step));
            if (iv_end - iv_start < 300) {
                for (int64_t i = iv_start; i <= iv_end; ++i) {
                    double wx = static_cast<double>(i) * vert_step;
                    Point2D s0 = world_to_screen({wx, vis.min.y});
                    Point2D s1 = world_to_screen({wx, vis.max.y});
                    res.isometric_lines.push_back({s0, s1, (i % 5 == 0)});
                }
            }
        }

        // 2. Diagonal +30 deg and -30 deg lines
        double diag_step = res.minor_step * sin_iso;
        if (diag_step > 1e-6) {
            double span = (vis.max.y - vis.min.y) + (vis.max.x - vis.min.x) * tan_iso;
            int64_t id_count = static_cast<int64_t>(std::ceil(span / diag_step));
            if (id_count < 300) {
                double min_c = vis.min.y - vis.max.x * tan_iso;
                int64_t ic_start = static_cast<int64_t>(std::floor(min_c / diag_step));
                for (int64_t i = ic_start; i <= ic_start + id_count; ++i) {
                    double c = static_cast<double>(i) * diag_step;
                    Point2D w0{vis.min.x, vis.min.x * tan_iso + c};
                    Point2D w1{vis.max.x, vis.max.x * tan_iso + c};
                    res.isometric_lines.push_back({world_to_screen(w0), world_to_screen(w1), (i % 5 == 0)});

                    Point2D wm0{vis.min.x, -vis.min.x * tan_iso + c};
                    Point2D wm1{vis.max.x, -vis.max.x * tan_iso + c};
                    res.isometric_lines.push_back({world_to_screen(wm0), world_to_screen(wm1), (i % 5 == 0)});
                }
            }
        }
    }

    return res;
}

Point2D SketchViewport2D::snap_to_active_grid(const Point2D& cursor_world, double override_step) const {
    double step = (override_step > 0.0) ? override_step : compute_dynamic_grid().minor_step;
    if (grid_mode == GridMode::Isometric) {
        return snap_to_isometric_grid(cursor_world, step, isometric_angle_deg);
    }
    // Cartesian
    double sx = std::round(cursor_world.x / step) * step;
    double sy = std::round(cursor_world.y / step) * step;
    return {sx, sy};
}

Point2D SketchViewport2D::snap_to_isometric_grid(
    const Point2D& cursor_world,
    double step,
    double iso_angle_deg
) noexcept {
    if (step <= 1e-12) return cursor_world;

    double rad = iso_angle_deg * (PI / 180.0);
    double cos_a = std::cos(rad);
    double sin_a = std::sin(rad);

    if (cos_a < 1e-6 || sin_a < 1e-6) return cursor_world;

    // Convert (x, y) to oblique isometric coordinates (c1, c2)
    double c1 = 0.5 * (cursor_world.x / cos_a + cursor_world.y / sin_a);
    double c2 = 0.5 * (-cursor_world.x / cos_a + cursor_world.y / sin_a);

    double c1_snap = std::round(c1 / step) * step;
    double c2_snap = std::round(c2 / step) * step;

    double snap_x = (c1_snap - c2_snap) * cos_a;
    double snap_y = (c1_snap + c2_snap) * sin_a;

    return {snap_x, snap_y};
}

std::string SketchViewport2D::format_world_coordinate(double val_meters) {
    char buf[64];
    double abs_v = std::abs(val_meters);
    if (abs_v < 1e-12) {
        return "0.00 mm";
    } else if (abs_v < 1e-3) {
        std::snprintf(buf, sizeof(buf), "%.2f um", val_meters * 1e6);
    } else if (abs_v < 1.0) {
        std::snprintf(buf, sizeof(buf), "%.2f mm", val_meters * 1e3);
    } else if (abs_v < 1000.0) {
        std::snprintf(buf, sizeof(buf), "%.3f m", val_meters);
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f km", val_meters * 1e-3);
    }
    return std::string(buf);
}

} // namespace shunya::cad2d
