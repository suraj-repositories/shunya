#include "shunya/core/domain/scientific_plot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace shunya::core::domain {

void MinMaxDecimator::decimate(
    std::span<const PlotPoint2D> data,
    double x_min,
    double x_max,
    size_t pixel_width,
    std::vector<PlotPoint2D>& out_points
) {
    out_points.clear();
    if (data.empty() || x_max <= x_min || pixel_width == 0) {
        return;
    }

    auto it_start = std::lower_bound(
        data.begin(), data.end(), x_min,
        [](const PlotPoint2D& p, double val) { return p.x < val; }
    );
    if (it_start != data.begin()) {
        --it_start;
    }

    auto it_end = std::upper_bound(
        data.begin(), data.end(), x_max,
        [](double val, const PlotPoint2D& p) { return val < p.x; }
    );
    if (it_end != data.end()) {
        ++it_end;
    }

    if (it_start >= it_end) {
        return;
    }

    size_t idx_start = static_cast<size_t>(it_start - data.begin());
    size_t idx_end = static_cast<size_t>(it_end - data.begin());
    size_t vis_count = idx_end - idx_start;

    if (vis_count <= pixel_width * 2) {
        out_points.assign(it_start, it_end);
        return;
    }

    out_points.reserve(pixel_width * 2);
    double x_span = x_max - x_min;
    double inv_bucket_width = static_cast<double>(pixel_width) / x_span;

    size_t curr_bucket = 0;
    bool bucket_initialized = false;
    size_t min_idx = idx_start;
    size_t max_idx = idx_start;

    auto flush_bucket = [&]() {
        if (!bucket_initialized) return;
        if (min_idx == max_idx) {
            out_points.push_back(data[min_idx]);
        } else if (min_idx < max_idx) {
            out_points.push_back(data[min_idx]);
            out_points.push_back(data[max_idx]);
        } else {
            out_points.push_back(data[max_idx]);
            out_points.push_back(data[min_idx]);
        }
        bucket_initialized = false;
    };

    for (size_t i = idx_start; i < idx_end; ++i) {
        const auto& pt = data[i];
        double raw_b = (pt.x - x_min) * inv_bucket_width;
        size_t b = raw_b <= 0.0 ? 0 : static_cast<size_t>(raw_b);
        if (b >= pixel_width) {
            b = pixel_width - 1;
        }

        if (!bucket_initialized) {
            curr_bucket = b;
            min_idx = i;
            max_idx = i;
            bucket_initialized = true;
        } else if (b != curr_bucket) {
            flush_bucket();
            curr_bucket = b;
            min_idx = i;
            max_idx = i;
            bucket_initialized = true;
        } else {
            if (pt.y < data[min_idx].y) min_idx = i;
            if (pt.y > data[max_idx].y) max_idx = i;
        }
    }

    flush_bucket();
}

PlotRange PlotAxisCalculator::compute_auto_range(
    std::span<const PlotPoint2D> data,
    double y_padding_fraction
) {
    PlotRange r;
    if (data.empty()) {
        return r;
    }

    double x_lo = data.front().x;
    double x_hi = data.back().x;
    if (x_hi < x_lo) {
        std::swap(x_lo, x_hi);
    }
    if (std::abs(x_hi - x_lo) < 1e-12) {
        x_lo -= 0.5;
        x_hi += 0.5;
    }

    double y_lo = std::numeric_limits<double>::infinity();
    double y_hi = -std::numeric_limits<double>::infinity();
    for (const auto& p : data) {
        if (std::isfinite(p.y)) {
            if (p.y < y_lo) y_lo = p.y;
            if (p.y > y_hi) y_hi = p.y;
        }
    }

    if (!std::isfinite(y_lo) || !std::isfinite(y_hi)) {
        y_lo = -1.0;
        y_hi = 1.0;
    } else if (std::abs(y_hi - y_lo) < 1e-12) {
        y_lo -= 1.0;
        y_hi += 1.0;
    } else {
        double pad = (y_hi - y_lo) * std::max(0.0, y_padding_fraction);
        y_lo -= pad;
        y_hi += pad;
    }

    r.x_min = x_lo;
    r.x_max = x_hi;
    r.y_min = y_lo;
    r.y_max = y_hi;
    return r;
}

namespace {

double nice_number(double range, bool round_val) {
    if (range <= 0.0) return 1.0;
    double exp_v = std::floor(std::log10(range));
    double f = range / std::pow(10.0, exp_v);
    double nf = 1.0;

    if (round_val) {
        if (f < 1.5) nf = 1.0;
        else if (f < 3.0) nf = 2.0;
        else if (f < 7.0) nf = 5.0;
        else nf = 10.0;
    } else {
        if (f <= 1.0) nf = 1.0;
        else if (f <= 2.0) nf = 2.0;
        else if (f <= 5.0) nf = 5.0;
        else nf = 10.0;
    }
    return nf * std::pow(10.0, exp_v);
}

std::string format_tick_value(double v) {
    if (std::abs(v) < 1e-12) return "0";
    char buf[64];
    double abs_v = std::abs(v);
    if (abs_v >= 1e4 || abs_v < 1e-2) {
        std::snprintf(buf, sizeof(buf), "%.2e", v);
    } else {
        std::snprintf(buf, sizeof(buf), "%.4g", v);
    }
    return std::string(buf);
}

} // namespace

std::vector<PlotTick> PlotAxisCalculator::generate_ticks(
    double min_val,
    double max_val,
    size_t max_ticks
) {
    std::vector<PlotTick> ticks;
    if (max_val <= min_val || max_ticks < 2) {
        return ticks;
    }

    double range = nice_number(max_val - min_val, false);
    double spacing = nice_number(range / static_cast<double>(max_ticks - 1), true);
    if (spacing <= 0.0) return ticks;

    double start = std::ceil(min_val / spacing) * spacing;
    double inv_span = 1.0 / (max_val - min_val);

    for (double val = start; val <= max_val + spacing * 1e-6; val += spacing) {
        if (ticks.size() > max_ticks * 2) break;
        double norm = (val - min_val) * inv_span;
        if (norm >= -1e-6 && norm <= 1.0 + 1e-6) {
            double clamped_norm = std::clamp(norm, 0.0, 1.0);
            double clean_val = std::abs(val) < spacing * 1e-9 ? 0.0 : val;
            ticks.push_back(PlotTick{clean_val, clamped_norm, format_tick_value(clean_val)});
        }
    }

    return ticks;
}

PlotRange PlotAxisCalculator::zoom_around(
    const PlotRange& current,
    double norm_x,
    double norm_y,
    double zoom_factor
) {
    if (!current.is_valid() || zoom_factor <= 0.0) return current;
    norm_x = std::clamp(norm_x, 0.0, 1.0);
    norm_y = std::clamp(norm_y, 0.0, 1.0);

    double cx = current.x_min + norm_x * current.width();
    double cy = current.y_min + norm_y * current.height();

    double new_w = current.width() * zoom_factor;
    double new_h = current.height() * zoom_factor;

    // Prevent collapse to floating-point noise
    if (new_w < 1e-12 || new_h < 1e-12) return current;

    PlotRange next;
    next.x_min = cx - norm_x * new_w;
    next.x_max = next.x_min + new_w;
    next.y_min = cy - norm_y * new_h;
    next.y_max = next.y_min + new_h;
    return next;
}

PlotRange PlotAxisCalculator::pan_by_normalized(
    const PlotRange& current,
    double delta_norm_x,
    double delta_norm_y
) {
    if (!current.is_valid()) return current;
    double dx = delta_norm_x * current.width();
    double dy = delta_norm_y * current.height();
    return PlotRange{
        current.x_min + dx,
        current.x_max + dx,
        current.y_min + dy,
        current.y_max + dy
    };
}

bool PlotAxisCalculator::find_nearest_x(
    std::span<const PlotPoint2D> data,
    double target_x,
    PlotPoint2D& out_point
) {
    if (data.empty()) return false;

    auto it = std::lower_bound(
        data.begin(), data.end(), target_x,
        [](const PlotPoint2D& p, double val) { return p.x < val; }
    );

    if (it == data.begin()) {
        out_point = *it;
        return true;
    }
    if (it == data.end()) {
        out_point = data.back();
        return true;
    }

    auto prev = it - 1;
    if (std::abs(target_x - prev->x) <= std::abs(it->x - target_x)) {
        out_point = *prev;
    } else {
        out_point = *it;
    }
    return true;
}

} // namespace shunya::core::domain
