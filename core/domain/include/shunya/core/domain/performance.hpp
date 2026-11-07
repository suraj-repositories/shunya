#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain::perf {

// =============================================================================
// PERF-01: Performance Budgets
// =============================================================================

struct PerformanceBudget {
    double max_startup_time_ms{1000.0};
    double max_frame_latency_ms{16.67}; // 60 FPS UI
    double min_simulation_throughput_steps_per_sec{10000.0};
    size_t max_memory_bytes{512 * 1024 * 1024}; // 512 MB
    double min_compression_ratio{2.0};
};

class BudgetMonitor {
public:
    explicit BudgetMonitor(PerformanceBudget budget = {}) : budget_(budget) {}

    [[nodiscard]] const PerformanceBudget& budget() const noexcept { return budget_; }
    void update_budget(PerformanceBudget b) noexcept { budget_ = b; }

    [[nodiscard]] bool check_startup_time(double startup_ms) const noexcept {
        return startup_ms <= budget_.max_startup_time_ms;
    }
    [[nodiscard]] bool check_frame_latency(double frame_ms) const noexcept {
        return frame_ms <= budget_.max_frame_latency_ms;
    }
    [[nodiscard]] bool check_throughput(double steps_per_sec) const noexcept {
        return steps_per_sec >= budget_.min_simulation_throughput_steps_per_sec;
    }
    [[nodiscard]] bool check_memory(size_t bytes_allocated) const noexcept {
        return bytes_allocated <= budget_.max_memory_bytes;
    }

private:
    PerformanceBudget budget_;
};

// =============================================================================
// PERF-02 & PERF-03: Workloads, Profiler & Timers
// =============================================================================

struct ProfileMetrics {
    double cpu_duration_ms{0.0};
    size_t allocations_count{0};
    size_t memory_peak_bytes{0};
    size_t io_bytes_read{0};
    size_t io_bytes_written{0};
};

class ScopedTimer {
public:
    using Clock = std::chrono::steady_clock;

    ScopedTimer() : start_(Clock::now()) {}

    [[nodiscard]] double elapsed_ms() const {
        auto now = Clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    [[nodiscard]] double elapsed_microseconds() const {
        auto now = Clock::now();
        return std::chrono::duration<double, std::micro>(now - start_).count();
    }

private:
    Clock::time_point start_;
};

// =============================================================================
// PERF-04: Data-Oriented Layout (Structure of Arrays - SoA)
// =============================================================================

class ParticleSystemSoA {
public:
    void resize(size_t count);
    [[nodiscard]] size_t size() const noexcept { return x_.size(); }

    void set_particle(size_t idx, double x, double y, double vx, double vy, double mass);

    // Vectorized / cache-friendly numerical Euler integration
    void integrate(double dt, double ax = 0.0, double ay = -9.81);

    [[nodiscard]] double x(size_t idx) const { return x_[idx]; }
    [[nodiscard]] double y(size_t idx) const { return y_[idx]; }
    [[nodiscard]] double vx(size_t idx) const { return vx_[idx]; }
    [[nodiscard]] double vy(size_t idx) const { return vy_[idx]; }

    // Memory footprint in bytes
    [[nodiscard]] size_t memory_bytes() const noexcept {
        return 5 * x_.size() * sizeof(double);
    }

private:
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> vx_;
    std::vector<double> vy_;
    std::vector<double> mass_;
};

// =============================================================================
// PERF-05: Parallel Deterministic Reduction
// =============================================================================

class DeterministicParallelReducer {
public:
    // Parallel sum with chunking and deterministic reduction tree
    static double parallel_sum(const std::vector<double>& values, size_t chunk_size = 256);
};

// =============================================================================
// PERF-07: Adaptive Deadband Data Compression
// =============================================================================

struct TimeSeriesPoint {
    double time{0.0};
    double value{0.0};
};

class AdaptiveDeadbandCompressor {
public:
    explicit AdaptiveDeadbandCompressor(double tolerance = 0.01) : tolerance_(tolerance) {}

    // Compresses a time-series by skipping points within tolerance envelope
    [[nodiscard]] std::vector<TimeSeriesPoint> compress(const std::vector<TimeSeriesPoint>& input) const;

    // Calculates compression ratio = uncompressed_size / compressed_size
    static double compression_ratio(size_t uncompressed_pts, size_t compressed_pts) noexcept {
        return (compressed_pts > 0) ? (static_cast<double>(uncompressed_pts) / static_cast<double>(compressed_pts)) : 1.0;
    }

private:
    double tolerance_{0.01};
};

// =============================================================================
// PERF-08 & PERF-09: Snapshot Buffer, Memory Budgets & Backpressure
// =============================================================================

struct UIStateSnapshot {
    uint64_t step_index{0};
    double simulation_time{0.0};
    std::vector<double> probe_values;
};

class LocklessSnapshotBuffer {
public:
    explicit LocklessSnapshotBuffer(size_t capacity = 16) : capacity_(capacity) {}

    void push(UIStateSnapshot snapshot);
    [[nodiscard]] std::optional<UIStateSnapshot> latest() const;

    [[nodiscard]] size_t size() const noexcept;
    void clear() noexcept;

private:
    size_t capacity_;
    std::vector<UIStateSnapshot> buffer_;
    mutable size_t head_{0};
    mutable bool has_data_{false};
};

class BackpressureController {
public:
    explicit BackpressureController(size_t max_queue_depth = 100, size_t memory_ceiling_bytes = 256 * 1024 * 1024)
        : max_queue_(max_queue_depth), memory_ceiling_(memory_ceiling_bytes) {}

    enum class Action {
        Proceed,
        Throttle,
        Abort
    };

    [[nodiscard]] Action evaluate(size_t current_queue_depth, size_t current_memory_bytes) const noexcept {
        if (current_memory_bytes > memory_ceiling_) {
            return Action::Abort;
        }
        if (current_queue_depth >= max_queue_) {
            return Action::Throttle;
        }
        return Action::Proceed;
    }

private:
    size_t max_queue_;
    size_t memory_ceiling_;
};

// =============================================================================
// PERF-10: Regression Detector
// =============================================================================

struct RegressionReport {
    std::string test_name;
    double baseline_ms{0.0};
    double measured_ms{0.0};
    double percentage_delta{0.0};
    bool is_regression{false};
};

class PerformanceRegressionDetector {
public:
    static RegressionReport evaluate(
        const std::string& test_name,
        double baseline_ms,
        double measured_ms,
        double allowed_regression_pct = 15.0) noexcept;
};

} // namespace shunya::core::domain::perf
