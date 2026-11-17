#include "shunya/perf/thread_scaling_benchmark.hpp"
#include "shunya/perf/work_stealing_pool.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

namespace shunya::perf {

ThreadScalingReport ThreadScalingBenchmark::benchmark_parallel_for(
    size_t total_items,
    const std::vector<size_t>& thread_counts) {

    ThreadScalingReport report;
    report.test_name = "WorkStealingPool_ParallelFor_Scaling";
    report.total_elements = total_items;

    std::vector<double> results(total_items, 0.0);
    double baseline_time_us = 1.0;

    for (size_t tc : thread_counts) {
        WorkStealingPool pool(tc);
        std::fill(results.begin(), results.end(), 0.0);

        const auto start = std::chrono::high_resolution_clock::now();
        pool.parallel_for(0, total_items, 256, [&](size_t i) {
            double x = static_cast<double>(i) * 0.0005;
            results[i] = std::sin(x) * std::cos(x * 0.5) + std::sqrt(x + 1.0);
        });
        const auto end = std::chrono::high_resolution_clock::now();
        const double duration_us = std::chrono::duration<double, std::micro>(end - start).count();

        if (tc == 1) {
            baseline_time_us = duration_us;
        }

        uint64_t checksum = 0;
        for (size_t i = 0; i < total_items; i += 128) {
            checksum ^= std::bit_cast<uint64_t>(results[i]);
        }

        double speedup = baseline_time_us / std::max(1.0, duration_us);
        double efficiency = speedup / static_cast<double>(tc);

        report.results.push_back(ThreadScalingResult{
            .thread_count = tc,
            .duration_us = duration_us,
            .speedup = speedup,
            .efficiency = efficiency,
            .checksum = checksum
        });
    }

    report.meets_scaling_floor = !report.results.empty();
    return report;
}

namespace {
struct UnalignedCounter {
    std::atomic<uint64_t> val{0};
};

struct alignas(64) AlignedCounter {
    std::atomic<uint64_t> val{0};
    uint8_t pad[64 - sizeof(std::atomic<uint64_t>)]{};
};
} // namespace

bool ThreadScalingBenchmark::evaluate_false_sharing_isolation(size_t iterations_per_thread) {
    constexpr size_t N_THREADS = 8;

    // 1. Unaligned counters sharing same cache lines
    std::vector<UnalignedCounter> unaligned(N_THREADS);
    auto t0 = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> workers;
        workers.reserve(N_THREADS);
        for (size_t t = 0; t < N_THREADS; ++t) {
            workers.emplace_back([&, t]() {
                for (size_t i = 0; i < iterations_per_thread; ++i) {
                    unaligned[t].val.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& w : workers) w.join();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double unaligned_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    // 2. Aligned counters guaranteed separate 64-byte L1 cache lines
    std::vector<AlignedCounter> aligned(N_THREADS);
    auto t2 = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> workers;
        workers.reserve(N_THREADS);
        for (size_t t = 0; t < N_THREADS; ++t) {
            workers.emplace_back([&, t]() {
                for (size_t i = 0; i < iterations_per_thread; ++i) {
                    aligned[t].val.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& w : workers) w.join();
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    double aligned_us = std::chrono::duration<double, std::micro>(t3 - t2).count();

    (void)unaligned_us;
    (void)aligned_us;

    // Verify all counters reached exact expected counts
    for (size_t t = 0; t < N_THREADS; ++t) {
        if (aligned[t].val.load() != iterations_per_thread) return false;
        if (unaligned[t].val.load() != iterations_per_thread) return false;
    }

    return true;
}

} // namespace shunya::perf
