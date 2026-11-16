#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <string_view>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <atomic>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace shunya::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

struct AutoRegister {
    AutoRegister(std::string name, std::function<void()> func) {
        get_registry().push_back({std::move(name), std::move(func)});
    }
};

inline std::atomic<int> g_failed_checks{0};
inline std::atomic<int> g_total_checks{0};
inline std::mutex g_test_check_mutex;

inline void check_impl(bool cond, std::string_view expr, std::string_view file, int line) {
    g_total_checks.fetch_add(1, std::memory_order_relaxed);
    if (!cond) {
        g_failed_checks.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(g_test_check_mutex);
        std::cerr << "[FAIL] Check failed: (" << expr << ") at " << file << ":" << line << "\n";
    }
}

// ============================================================================
// Benchmark Timers & Metrics
// ============================================================================

struct BenchmarkResult {
    std::string name;
    std::size_t iterations{0};
    double total_ms{0.0};
    double avg_us{0.0};
    double min_us{0.0};
    double max_us{0.0};
    double ops_per_sec{0.0};
};

template <typename Func>
inline BenchmarkResult run_benchmark(std::string_view name, std::size_t iterations, Func&& func) {
    if (iterations == 0) iterations = 1;
    // Warm-up iteration
    func();

    std::vector<double> run_times_us;
    run_times_us.reserve(iterations);

    auto global_t0 = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        func();
        auto t1 = std::chrono::high_resolution_clock::now();
        run_times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    auto global_t1 = std::chrono::high_resolution_clock::now();

    double total_us = std::chrono::duration<double, std::micro>(global_t1 - global_t0).count();
    double min_us = run_times_us.empty() ? 0.0 : *std::min_element(run_times_us.begin(), run_times_us.end());
    double max_us = run_times_us.empty() ? 0.0 : *std::max_element(run_times_us.begin(), run_times_us.end());
    double avg_us = total_us / static_cast<double>(iterations);
    double ops = total_us > 0.0 ? (static_cast<double>(iterations) / (total_us / 1e6)) : 0.0;

    BenchmarkResult res{
        .name = std::string(name),
        .iterations = iterations,
        .total_ms = total_us / 1000.0,
        .avg_us = avg_us,
        .min_us = min_us,
        .max_us = max_us,
        .ops_per_sec = ops
    };

    std::cout << "[BENCHMARK] " << res.name << " (" << res.iterations << " iters): "
              << "total " << res.total_ms << " ms | "
              << "avg " << res.avg_us << " us | "
              << "min " << res.min_us << " us | "
              << "max " << res.max_us << " us | "
              << static_cast<uint64_t>(res.ops_per_sec) << " ops/sec" << std::endl;

    return res;
}

class ScopedTimer {
public:
    explicit ScopedTimer(std::string name)
        : name_(std::move(name)), start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        if (!stopped_) {
            stop();
        }
    }

    double stop() {
        if (stopped_) return elapsed_us_;
        stopped_ = true;
        auto end = std::chrono::high_resolution_clock::now();
        elapsed_us_ = std::chrono::duration<double, std::micro>(end - start_).count();
        std::cout << "[TIMER] " << name_ << ": " << elapsed_us_ / 1000.0 << " ms" << std::endl;
        return elapsed_us_;
    }

    [[nodiscard]] double elapsed_ms() const noexcept {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

private:
    std::string name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
    double elapsed_us_{0.0};
    bool stopped_{false};
};

// ============================================================================
// Process Memory Inspection & Leak Detection Fixtures
// ============================================================================

struct ProcessMemoryStats {
    std::size_t working_set_bytes{0};
    std::size_t peak_working_set_bytes{0};
    std::size_t private_bytes{0};
};

inline ProcessMemoryStats get_process_memory_stats() {
    ProcessMemoryStats stats{};
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        stats.working_set_bytes = static_cast<std::size_t>(pmc.WorkingSetSize);
        stats.peak_working_set_bytes = static_cast<std::size_t>(pmc.PeakWorkingSetSize);
        stats.private_bytes = static_cast<std::size_t>(pmc.PrivateUsage);
    }
#elif defined(__linux__)
    std::ifstream f("/proc/self/statm");
    if (f.is_open()) {
        unsigned long size = 0, resident = 0;
        if (f >> size >> resident) {
            long page_size = sysconf(_SC_PAGESIZE);
            stats.working_set_bytes = static_cast<std::size_t>(resident * page_size);
            stats.private_bytes = static_cast<std::size_t>(size * page_size);
        }
    }
#endif
    return stats;
}

class ScopedMemoryLeakDetector {
public:
    explicit ScopedMemoryLeakDetector(std::string tag = "Scope", std::size_t max_allowed_growth_bytes = 10 * 1024 * 1024)
        : tag_(std::move(tag)), max_allowed_growth_(max_allowed_growth_bytes), initial_(get_process_memory_stats()) {}

    [[nodiscard]] std::int64_t working_set_growth_bytes() const noexcept {
        auto current = get_process_memory_stats();
        return static_cast<std::int64_t>(current.working_set_bytes) - static_cast<std::int64_t>(initial_.working_set_bytes);
    }

    [[nodiscard]] std::int64_t private_bytes_growth() const noexcept {
        auto current = get_process_memory_stats();
        return static_cast<std::int64_t>(current.private_bytes) - static_cast<std::int64_t>(initial_.private_bytes);
    }

    [[nodiscard]] bool has_leak() const noexcept {
        return private_bytes_growth() > static_cast<std::int64_t>(max_allowed_growth_);
    }

    void verify_no_leak(std::string_view file, int line) const {
        std::int64_t growth = private_bytes_growth();
        bool ok = growth <= static_cast<std::int64_t>(max_allowed_growth_);
        check_impl(ok, "Memory leak check: growth <= " + std::to_string(max_allowed_growth_) + " bytes (actual: " + std::to_string(growth) + " bytes)", file, line);
    }

private:
    std::string tag_;
    std::size_t max_allowed_growth_;
    ProcessMemoryStats initial_;
};

#define SHUNYA_CONCAT_IMPL(a, b) a##b
#define SHUNYA_CONCAT(a, b) SHUNYA_CONCAT_IMPL(a, b)

#define TEST_CASE(name) \
    static void SHUNYA_CONCAT(test_func_, __LINE__)(); \
    static ::shunya::test::AutoRegister SHUNYA_CONCAT(reg_, __LINE__)(name, SHUNYA_CONCAT(test_func_, __LINE__)); \
    static void SHUNYA_CONCAT(test_func_, __LINE__)()

#define CHECK(expr) ::shunya::test::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define CHECK_FALSE(expr) ::shunya::test::check_impl(!static_cast<bool>(expr), "!(" #expr ")", __FILE__, __LINE__)
#define REQUIRE(expr) do { \
    bool req_cond = static_cast<bool>(expr); \
    ::shunya::test::check_impl(req_cond, #expr, __FILE__, __LINE__); \
    if (!req_cond) { \
        std::cerr << "[FATAL] Requirement (" #expr ") failed. Aborting test case.\n"; \
        return; \
    } \
} while(false)
#define REQUIRE_FALSE(expr) do { \
    bool req_cond = !static_cast<bool>(expr); \
    ::shunya::test::check_impl(req_cond, "!(" #expr ")", __FILE__, __LINE__); \
    if (!req_cond) { \
        std::cerr << "[FATAL] Requirement (!(" #expr ")) failed. Aborting test case.\n"; \
        return; \
    } \
} while(false)

#define BENCHMARK(name, iters, ...) \
    ::shunya::test::run_benchmark(name, iters, [&]() { __VA_ARGS__; })

#define TIMED_SECTION(name) \
    ::shunya::test::ScopedTimer SHUNYA_CONCAT(_timer_, __LINE__)(name)

#define CHECK_NO_MEMORY_GROWTH(max_growth_bytes, ...) do { \
    ::shunya::test::ScopedMemoryLeakDetector SHUNYA_CONCAT(_detector_, __LINE__)("Check", max_growth_bytes); \
    do { __VA_ARGS__; } while(false); \
    SHUNYA_CONCAT(_detector_, __LINE__).verify_no_leak(__FILE__, __LINE__); \
} while(false)

inline int run_all_tests(int argc = 0, char** argv = nullptr) {
    auto& tests = get_registry();
    std::string filter;
    if (argc > 1 && argv && argv[1]) {
        filter = argv[1];
    }

    std::cout << "========================================\n";
    std::cout << "Running " << tests.size() << " test cases";
    if (!filter.empty()) {
        std::cout << " (filter: \"" << filter << "\")";
    }
    std::cout << "...\n========================================\n" << std::flush;

    int failed_cases = 0;
    int ran_cases = 0;
    for (const auto& t : tests) {
        if (!filter.empty() && t.name.find(filter) == std::string::npos) {
            continue;
        }
        ++ran_cases;
        int before_failures = g_failed_checks.load(std::memory_order_relaxed);
        std::cout << "[RUN ] " << t.name << std::endl;
        try {
            t.func();
        } catch (const std::exception& ex) {
            g_failed_checks.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "[EXCEPTION] Caught unhandled exception in test case: " << ex.what() << std::endl;
        } catch (...) {
            g_failed_checks.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "[EXCEPTION] Caught unknown unhandled exception in test case." << std::endl;
        }

        if (g_failed_checks.load(std::memory_order_relaxed) == before_failures) {
            std::cout << "[PASS] " << t.name << std::endl;
        } else {
            ++failed_cases;
            std::cerr << "[FAIL] " << t.name << std::endl;
        }
    }

    std::cout << "========================================\n";
    std::cout << "Test Summary: " << (tests.size() - failed_cases) << " passed, "
              << failed_cases << " failed of " << tests.size() << " cases. ("
              << (g_total_checks - g_failed_checks) << "/" << g_total_checks << " checks passed)\n";
    std::cout << "========================================\n";

    return failed_cases == 0 ? 0 : 1;
}

} // namespace shunya::test
