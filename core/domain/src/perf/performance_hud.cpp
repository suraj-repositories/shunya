#include "shunya/perf/performance_hud.hpp"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace shunya::perf {

PerformanceHudModel::PerformanceHudModel(const PerformanceHudConfig& config)
    : config_(config) {
    current_sample_.fps = config_.target_fps;
    current_sample_.cpu_frame_time_ms = 1000.0 / std::max(1.0, config_.target_fps);
    current_sample_.gpu_render_time_ms = 0.0;
    current_sample_.ram_usage_mb = query_system_ram_mb();
    current_sample_.vram_usage_mb = query_system_vram_mb();
    ema_fps_ = config_.target_fps;
}

double PerformanceHudModel::query_system_ram_mb() noexcept {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 145.0; // Baseline fallback
}

double PerformanceHudModel::query_system_vram_mb() noexcept {
    // Simulated RHI target + buffer VRAM baseline (64 MB base)
    return 64.0 + 8.5;
}

void PerformanceHudModel::record_frame(
    double cpu_time_ms,
    double gpu_time_ms,
    uint32_t draw_calls,
    uint32_t triangles,
    uint32_t compute_dispatches,
    double override_ram_mb,
    double override_vram_mb
) {
    const double net_frame_time_ms = std::max(0.01, cpu_time_ms + gpu_time_ms);
    const double instantaneous_fps = 1000.0 / net_frame_time_ms;

    // Exponential moving average for smooth display
    ema_fps_ = 0.85 * ema_fps_ + 0.15 * instantaneous_fps;
    ++total_frames_;

    current_sample_.timestamp_s = static_cast<double>(total_frames_) / std::max(1.0, ema_fps_);
    current_sample_.cpu_frame_time_ms = cpu_time_ms;
    current_sample_.gpu_render_time_ms = gpu_time_ms;
    current_sample_.fps = instantaneous_fps;
    current_sample_.draw_calls = draw_calls;
    current_sample_.triangles_submitted = triangles;
    current_sample_.compute_dispatches = compute_dispatches;

    current_sample_.ram_usage_mb = (override_ram_mb >= 0.0) ? override_ram_mb : query_system_ram_mb();
    current_sample_.vram_usage_mb = (override_vram_mb >= 0.0) ? override_vram_mb : query_system_vram_mb();

    history_.push_back(current_sample_);
    while (history_.size() > config_.history_capacity) {
        history_.pop_front();
    }
}

double PerformanceHudModel::average_fps() const noexcept {
    if (history_.empty()) return ema_fps_;
    double sum = 0.0;
    for (const auto& sample : history_) {
        sum += sample.fps;
    }
    return sum / static_cast<double>(history_.size());
}

std::string PerformanceHudModel::formatted_fps_text() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "FPS: " << ema_fps_ << " (" << (current_sample_.cpu_frame_time_ms + current_sample_.gpu_render_time_ms) << " ms)";
    return oss.str();
}

std::string PerformanceHudModel::formatted_cpu_time_text() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "CPU: " << current_sample_.cpu_frame_time_ms << " ms";
    return oss.str();
}

std::string PerformanceHudModel::formatted_gpu_time_text() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "GPU: " << current_sample_.gpu_render_time_ms << " ms";
    return oss.str();
}

std::string PerformanceHudModel::formatted_memory_text() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "RAM: " << current_sample_.ram_usage_mb << " MB | VRAM: " << current_sample_.vram_usage_mb << " MB";
    return oss.str();
}

std::string PerformanceHudModel::formatted_draw_calls_text() const {
    std::ostringstream oss;
    oss << "Draw Calls: " << current_sample_.draw_calls;
    if (current_sample_.triangles_submitted > 1000) {
        oss << " | Tris: " << std::fixed << std::setprecision(1) << (static_cast<double>(current_sample_.triangles_submitted) / 1000.0) << "k";
    } else {
        oss << " | Tris: " << current_sample_.triangles_submitted;
    }
    return oss.str();
}

void PerformanceHudModel::reset() noexcept {
    history_.clear();
    total_frames_ = 0;
    ema_fps_ = config_.target_fps;
    current_sample_ = PerformanceTelemetrySample{};
}

} // namespace shunya::perf
