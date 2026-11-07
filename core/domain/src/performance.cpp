#include "shunya/core/domain/performance.hpp"

#include <algorithm>
#include <numeric>

namespace shunya::core::domain::perf {

// =============================================================================
// ParticleSystemSoA Implementation
// =============================================================================

void ParticleSystemSoA::resize(size_t count) {
    x_.resize(count, 0.0);
    y_.resize(count, 0.0);
    vx_.resize(count, 0.0);
    vy_.resize(count, 0.0);
    mass_.resize(count, 1.0);
}

void ParticleSystemSoA::set_particle(size_t idx, double x, double y, double vx, double vy, double mass) {
    if (idx < x_.size()) {
        x_[idx] = x;
        y_[idx] = y;
        vx_[idx] = vx;
        vy_[idx] = vy;
        mass_[idx] = mass;
    }
}

void ParticleSystemSoA::integrate(double dt, double ax, double ay) {
    size_t count = x_.size();
    for (size_t i = 0; i < count; ++i) {
        vx_[i] += ax * dt;
        vy_[i] += ay * dt;
        x_[i] += vx_[i] * dt;
        y_[i] += vy_[i] * dt;
    }
}

// =============================================================================
// DeterministicParallelReducer Implementation
// =============================================================================

double DeterministicParallelReducer::parallel_sum(const std::vector<double>& values, size_t chunk_size) {
    if (values.empty()) {
        return 0.0;
    }
    if (chunk_size == 0) {
        chunk_size = 256;
    }

    size_t n = values.size();
    size_t num_chunks = (n + chunk_size - 1) / chunk_size;
    std::vector<double> partial_sums(num_chunks, 0.0);

    for (size_t c = 0; c < num_chunks; ++c) {
        size_t start = c * chunk_size;
        size_t end = std::min(n, start + chunk_size);
        double s = 0.0;
        for (size_t i = start; i < end; ++i) {
            s += values[i];
        }
        partial_sums[c] = s;
    }

    // Deterministic pairwise reduction tree
    while (partial_sums.size() > 1) {
        std::vector<double> next_level;
        next_level.reserve((partial_sums.size() + 1) / 2);
        for (size_t i = 0; i < partial_sums.size(); i += 2) {
            if (i + 1 < partial_sums.size()) {
                next_level.push_back(partial_sums[i] + partial_sums[i + 1]);
            } else {
                next_level.push_back(partial_sums[i]);
            }
        }
        partial_sums = std::move(next_level);
    }

    return partial_sums.front();
}

// =============================================================================
// AdaptiveDeadbandCompressor Implementation
// =============================================================================

std::vector<TimeSeriesPoint> AdaptiveDeadbandCompressor::compress(
    const std::vector<TimeSeriesPoint>& input) const {
    if (input.size() <= 2) {
        return input;
    }

    std::vector<TimeSeriesPoint> compressed;
    compressed.push_back(input.front());

    double last_recorded_val = input.front().value;

    for (size_t i = 1; i < input.size() - 1; ++i) {
        double delta = std::abs(input[i].value - last_recorded_val);
        if (delta >= tolerance_) {
            compressed.push_back(input[i]);
            last_recorded_val = input[i].value;
        }
    }

    // Always include the last point to preserve time extent
    compressed.push_back(input.back());
    return compressed;
}

// =============================================================================
// LocklessSnapshotBuffer Implementation
// =============================================================================

void LocklessSnapshotBuffer::push(UIStateSnapshot snapshot) {
    if (buffer_.size() < capacity_) {
        buffer_.push_back(std::move(snapshot));
        head_ = buffer_.size() - 1;
    } else {
        head_ = (head_ + 1) % capacity_;
        buffer_[head_] = std::move(snapshot);
    }
    has_data_ = true;
}

std::optional<UIStateSnapshot> LocklessSnapshotBuffer::latest() const {
    if (!has_data_ || buffer_.empty()) {
        return std::nullopt;
    }
    return buffer_[head_];
}

size_t LocklessSnapshotBuffer::size() const noexcept {
    return buffer_.size();
}

void LocklessSnapshotBuffer::clear() noexcept {
    buffer_.clear();
    head_ = 0;
    has_data_ = false;
}

// =============================================================================
// PerformanceRegressionDetector Implementation
// =============================================================================

RegressionReport PerformanceRegressionDetector::evaluate(
    const std::string& test_name,
    double baseline_ms,
    double measured_ms,
    double allowed_regression_pct) noexcept {
    RegressionReport report;
    report.test_name = test_name;
    report.baseline_ms = baseline_ms;
    report.measured_ms = measured_ms;

    if (baseline_ms > 0.0) {
        report.percentage_delta = ((measured_ms - baseline_ms) / baseline_ms) * 100.0;
        report.is_regression = (report.percentage_delta > allowed_regression_pct);
    } else {
        report.percentage_delta = 0.0;
        report.is_regression = false;
    }

    return report;
}

} // namespace shunya::core::domain::perf
