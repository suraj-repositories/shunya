#include "tests/test_framework.hpp"
#include "shunya/core/domain/performance.hpp"

#include <cmath>
#include <vector>

using namespace shunya::core::domain::perf;

// =============================================================================
// PERF-01: Performance Budgets
// =============================================================================

TEST_CASE("PERF-01: Performance Budget Verification") {
    PerformanceBudget budget;
    budget.max_startup_time_ms = 800.0;
    budget.max_frame_latency_ms = 16.67; // 60 FPS
    budget.min_simulation_throughput_steps_per_sec = 5000.0;
    budget.max_memory_bytes = 256 * 1024 * 1024; // 256 MB

    BudgetMonitor monitor(budget);

    CHECK(monitor.check_startup_time(650.0));
    CHECK_FALSE(monitor.check_startup_time(950.0));

    CHECK(monitor.check_frame_latency(12.0));
    CHECK_FALSE(monitor.check_frame_latency(20.0));

    CHECK(monitor.check_throughput(8000.0));
    CHECK_FALSE(monitor.check_throughput(3000.0));

    CHECK(monitor.check_memory(128 * 1024 * 1024));
    CHECK_FALSE(monitor.check_memory(300 * 1024 * 1024));
}

// =============================================================================
// PERF-02 & PERF-03: Scoped Profiling Timers
// =============================================================================

TEST_CASE("PERF-02 & PERF-03: Scoped Performance Timers") {
    ScopedTimer timer;
    volatile double dummy = 0.0;
    for (int i = 0; i < 10000; ++i) {
        dummy += std::sin(static_cast<double>(i));
    }
    (void)dummy;

    double elapsed_ms = timer.elapsed_ms();
    CHECK(elapsed_ms >= 0.0);
    double elapsed_us = timer.elapsed_microseconds();
    CHECK(elapsed_us >= 0.0);
}

// =============================================================================
// PERF-04: Data-Oriented Layout (Structure of Arrays - SoA)
// =============================================================================

TEST_CASE("PERF-04: Data-Oriented Structure-of-Arrays Particle System") {
    ParticleSystemSoA soa;
    soa.resize(100);
    CHECK(soa.size() == 100u);
    CHECK(soa.memory_bytes() == 100 * 5 * sizeof(double));

    for (size_t i = 0; i < 100; ++i) {
        soa.set_particle(i, 0.0, 10.0, 1.0, 0.0, 1.0);
    }

    // Integrate with gravity ay = -9.81 for dt = 0.1s
    soa.integrate(0.1, 0.0, -9.81);

    // Particle 0: vx = 1.0, vy = -0.981, x = 0.1, y = 10.0 - 0.0981
    CHECK(std::abs(soa.vx(0) - 1.0) < 1e-6);
    CHECK(std::abs(soa.vy(0) - (-0.981)) < 1e-6);
    CHECK(std::abs(soa.x(0) - 0.1) < 1e-6);
    CHECK(soa.y(0) < 10.0);
}

// =============================================================================
// PERF-05: Parallel Deterministic Reduction Tree
// =============================================================================

TEST_CASE("PERF-05: Deterministic Parallel Reduction Tree") {
    std::vector<double> vals(1024, 1.5); // Sum should be 1024 * 1.5 = 1536.0
    double sum = DeterministicParallelReducer::parallel_sum(vals, 128);
    CHECK(std::abs(sum - 1536.0) < 1e-9);

    std::vector<double> empty_vals;
    CHECK(DeterministicParallelReducer::parallel_sum(empty_vals) == 0.0);
}

// =============================================================================
// PERF-07: Adaptive Deadband Time-Series Data Compression
// =============================================================================

TEST_CASE("PERF-07: Adaptive Deadband Output Sampling and Compression") {
    AdaptiveDeadbandCompressor compressor(0.1); // 0.1 tolerance

    // Input signal with plateau: [0.0, 0.01, 0.02, 0.01, 1.0, 1.02, 2.0]
    std::vector<TimeSeriesPoint> pts{
        {0.0, 0.0},
        {0.1, 0.02}, // delta < 0.1, skipped
        {0.2, 0.03}, // delta < 0.1, skipped
        {0.3, 0.01}, // delta < 0.1, skipped
        {0.4, 1.0},  // delta >= 0.1, recorded!
        {0.5, 1.02}, // delta < 0.1, skipped
        {0.6, 2.0}   // delta >= 0.1, recorded!
    };

    auto compressed = compressor.compress(pts);
    CHECK(compressed.size() < pts.size());
    CHECK(compressed.size() == 3u); // Start (0.0), Middle step (1.0), End (2.0)

    double ratio = AdaptiveDeadbandCompressor::compression_ratio(pts.size(), compressed.size());
    CHECK(ratio > 2.0); // >2x compression achieved
}

// =============================================================================
// PERF-08 & PERF-09: Lockless Snapshots & Backpressure Regulation
// =============================================================================

TEST_CASE("PERF-08 & PERF-09: UI Snapshot Buffering and Backpressure Regulation") {
    LocklessSnapshotBuffer buf(4);
    CHECK(buf.size() == 0u);

    UIStateSnapshot s1{1, 0.01, {1.2, 3.4}};
    UIStateSnapshot s2{2, 0.02, {2.2, 4.4}};
    buf.push(s1);
    buf.push(s2);

    auto latest = buf.latest();
    REQUIRE(latest.has_value());
    CHECK(latest->step_index == 2u);
    CHECK(latest->simulation_time == 0.02);

    // Backpressure Controller: max queue = 50, memory = 100 MB
    BackpressureController ctrl(50, 100 * 1024 * 1024);
    CHECK(ctrl.evaluate(10, 50 * 1024 * 1024) == BackpressureController::Action::Proceed);
    CHECK(ctrl.evaluate(60, 50 * 1024 * 1024) == BackpressureController::Action::Throttle);
    CHECK(ctrl.evaluate(10, 150 * 1024 * 1024) == BackpressureController::Action::Abort);
}

// =============================================================================
// PERF-10: Automated Performance Regression Detection
// =============================================================================

TEST_CASE("PERF-10: Automated Performance Regression Detection") {
    // Normal case: 10 ms vs 10.5 ms (+5%, below 15% threshold)
    auto rep_ok = PerformanceRegressionDetector::evaluate("SolverStep", 10.0, 10.5, 15.0);
    CHECK_FALSE(rep_ok.is_regression);
    CHECK(std::abs(rep_ok.percentage_delta - 5.0) < 1e-3);

    // Regression case: 10 ms vs 13.0 ms (+30%, above 15% threshold)
    auto rep_reg = PerformanceRegressionDetector::evaluate("SolverStep", 10.0, 13.0, 15.0);
    CHECK(rep_reg.is_regression);
    CHECK(std::abs(rep_reg.percentage_delta - 30.0) < 1e-3);
}
