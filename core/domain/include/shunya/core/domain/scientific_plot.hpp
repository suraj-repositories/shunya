#pragma once

#include "shunya/core/base/ring_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace shunya::core::domain {

struct PlotPoint2D {
    double x{0.0};
    double y{0.0};
};

struct PlotRange {
    double x_min{0.0};
    double x_max{1.0};
    double y_min{-1.0};
    double y_max{1.0};

    [[nodiscard]] double width() const noexcept { return x_max - x_min; }
    [[nodiscard]] double height() const noexcept { return y_max - y_min; }
    [[nodiscard]] bool is_valid() const noexcept { return x_max > x_min && y_max > y_min; }
};

struct PlotTick {
    double value{0.0};
    double normalized_pos{0.0}; // [0, 1] inside PlotRange
    std::string label;
};

/**
 * @brief High-throughput Min-Max pyramid / bucket decimator for 2D scientific time-series data.
 *
 * Preserves local extrema (sharp peaks and transients) while bounding output vertices
 * to at most 2 * pixel_width points for lock-free 60 FPS GPU rendering.
 */
class MinMaxDecimator {
public:
    /**
     * @brief Decimate sorted-in-X points within [x_min, x_max] for a target pixel width.
     *
     * @param data Points ordered non-decreasing by x.
     * @param x_min Visible left bound.
     * @param x_max Visible right bound.
     * @param pixel_width Target horizontal pixel count (buckets).
     * @param out_points Output buffer cleared and populated in O(N_visible) time.
     */
    static void decimate(
        std::span<const PlotPoint2D> data,
        double x_min,
        double x_max,
        size_t pixel_width,
        std::vector<PlotPoint2D>& out_points
    );
};

/**
 * @brief Computes data bounds, "nice number" ticks, coordinate transforms, and nearest-point crosshair probes.
 */
class PlotAxisCalculator {
public:
    [[nodiscard]] static PlotRange compute_auto_range(
        std::span<const PlotPoint2D> data,
        double y_padding_fraction = 0.05
    );

    [[nodiscard]] static std::vector<PlotTick> generate_ticks(
        double min_val,
        double max_val,
        size_t max_ticks = 8
    );

    [[nodiscard]] static PlotRange zoom_around(
        const PlotRange& current,
        double norm_x,
        double norm_y,
        double zoom_factor
    );

    [[nodiscard]] static PlotRange pan_by_normalized(
        const PlotRange& current,
        double delta_norm_x,
        double delta_norm_y
    );

    /**
     * @brief Finds nearest point in X for crosshair readout using binary search on sorted X.
     */
    [[nodiscard]] static bool find_nearest_x(
        std::span<const PlotPoint2D> data,
        double target_x,
        PlotPoint2D& out_point
    );
};

} // namespace shunya::core::domain
