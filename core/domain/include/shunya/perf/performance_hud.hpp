#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace shunya::perf {

enum class HudDockPosition {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct PerformanceTelemetrySample {
    double timestamp_s{0.0};
    double cpu_frame_time_ms{0.0};
    double gpu_render_time_ms{0.0};
    double fps{60.0};
    double ram_usage_mb{0.0};
    double vram_usage_mb{0.0};
    uint32_t draw_calls{0};
    uint32_t triangles_submitted{0};
    uint32_t compute_dispatches{0};
};

struct PerformanceHudConfig {
    bool visible{true};
    HudDockPosition dock_position{HudDockPosition::TopRight};
    size_t history_capacity{120}; // 120 frames (~2 seconds at 60 FPS)
    double target_fps{60.0};
    double low_fps_threshold{30.0};
    double ram_warning_mb{800.0};
    double vram_warning_mb{1024.0};
};

/**
 * @brief High-precision real-time performance HUD data model & controller.
 * Tracks and visualizes FPS, CPU frame time, GPU render time, RAM consumption, and VRAM utilization.
 */
class PerformanceHudModel {
public:
    explicit PerformanceHudModel(const PerformanceHudConfig& config = PerformanceHudConfig{});

    void record_frame(
        double cpu_time_ms,
        double gpu_time_ms,
        uint32_t draw_calls = 0,
        uint32_t triangles = 0,
        uint32_t compute_dispatches = 0,
        double override_ram_mb = -1.0,
        double override_vram_mb = -1.0
    );

    [[nodiscard]] double current_fps() const noexcept { return current_sample_.fps; }
    [[nodiscard]] double average_fps() const noexcept;
    [[nodiscard]] double cpu_frame_time_ms() const noexcept { return current_sample_.cpu_frame_time_ms; }
    [[nodiscard]] double gpu_render_time_ms() const noexcept { return current_sample_.gpu_render_time_ms; }
    [[nodiscard]] double ram_usage_mb() const noexcept { return current_sample_.ram_usage_mb; }
    [[nodiscard]] double vram_usage_mb() const noexcept { return current_sample_.vram_usage_mb; }
    [[nodiscard]] uint32_t draw_calls() const noexcept { return current_sample_.draw_calls; }
    [[nodiscard]] uint32_t triangles_submitted() const noexcept { return current_sample_.triangles_submitted; }
    [[nodiscard]] uint32_t compute_dispatches() const noexcept { return current_sample_.compute_dispatches; }

    [[nodiscard]] bool is_low_fps() const noexcept { return current_sample_.fps < config_.low_fps_threshold; }
    [[nodiscard]] bool is_ram_warning() const noexcept { return current_sample_.ram_usage_mb > config_.ram_warning_mb; }
    [[nodiscard]] bool is_vram_warning() const noexcept { return current_sample_.vram_usage_mb > config_.vram_warning_mb; }

    [[nodiscard]] const PerformanceTelemetrySample& latest_sample() const noexcept { return current_sample_; }
    [[nodiscard]] const std::deque<PerformanceTelemetrySample>& history() const noexcept { return history_; }

    [[nodiscard]] std::string formatted_fps_text() const;
    [[nodiscard]] std::string formatted_cpu_time_text() const;
    [[nodiscard]] std::string formatted_gpu_time_text() const;
    [[nodiscard]] std::string formatted_memory_text() const;
    [[nodiscard]] std::string formatted_draw_calls_text() const;

    [[nodiscard]] bool is_visible() const noexcept { return config_.visible; }
    void set_visible(bool visible) noexcept { config_.visible = visible; }
    void toggle_visible() noexcept { config_.visible = !config_.visible; }

    [[nodiscard]] HudDockPosition dock_position() const noexcept { return config_.dock_position; }
    void set_dock_position(HudDockPosition pos) noexcept { config_.dock_position = pos; }

    static double query_system_ram_mb() noexcept;
    static double query_system_vram_mb() noexcept;

    void reset() noexcept;

private:
    PerformanceHudConfig config_;
    PerformanceTelemetrySample current_sample_{};
    std::deque<PerformanceTelemetrySample> history_;
    double ema_fps_{60.0};
    uint64_t total_frames_{0};
};

} // namespace shunya::perf
