#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::perf {

struct ThreadScalingResult {
    size_t thread_count{0};
    double duration_us{0.0};
    double speedup{1.0};
    double efficiency{1.0};
    uint64_t checksum{0};
};

struct ThreadScalingReport {
    std::string test_name;
    size_t total_elements{0};
    std::vector<ThreadScalingResult> results;
    bool meets_scaling_floor{true};
    bool false_sharing_eliminated{true};
};

/**
 * @brief Benchmark suite evaluating multi-threaded scaling from 1 to 32 threads
 * and verifying hardware cache line isolation to eliminate false sharing.
 */
class ThreadScalingBenchmark {
public:
    static ThreadScalingReport benchmark_parallel_for(
        size_t total_items = 100000,
        const std::vector<size_t>& thread_counts = {1, 2, 4, 8, 16, 32}
    );

    static bool evaluate_false_sharing_isolation(size_t iterations_per_thread = 200000);
};

} // namespace shunya::perf
