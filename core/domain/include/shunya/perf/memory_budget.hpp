#pragma once

#include <cstddef>
#include <string>

namespace shunya::perf {

struct MemoryBudgetLimits {
    double base_studio_idle_limit_mb{250.0};      // Limit: < 250 MB RAM at studio idle
    double complex_scene_100k_limit_mb{800.0};    // Limit: < 800 MB RAM for 100k-vertex 3D scene
};

struct MemoryFootprintMetrics {
    double current_rss_mb{0.0};
    double peak_rss_mb{0.0};
    size_t active_vertex_count{0};
    bool is_within_idle_budget{true};
    bool is_within_scene_budget{true};
    std::string diagnostic_summary;
};

/**
 * @brief Memory footprint limiter and validator enforcing architectural budget constraints:
 *        - Base studio idle: < 250 MB RAM
 *        - Complex 100k-vertex 3D scene: < 800 MB RAM
 * (Roadmap task PH4-02.04.006)
 */
class MemoryFootprintEnforcer {
public:
    explicit MemoryFootprintEnforcer(MemoryBudgetLimits limits = {});

    [[nodiscard]] const MemoryBudgetLimits& limits() const noexcept { return limits_; }
    void set_limits(MemoryBudgetLimits limits) noexcept { limits_ = limits; }

    /**
     * @brief Measures actual resident set size (RSS) of the current running process in Megabytes.
     */
    [[nodiscard]] static double query_process_rss_mb() noexcept;

    /**
     * @brief Evaluates current process memory footprint against architectural budget limits.
     */
    [[nodiscard]] MemoryFootprintMetrics evaluate_current_footprint(size_t active_vertex_count = 0) const;

    /**
     * @brief Analytically models memory consumption for a CAD 3D scene including vertex buffers,
     *        index buffers, half-edge topological pools, and material regions.
     */
    [[nodiscard]] double estimate_scene_footprint_mb(
        size_t vertex_count,
        size_t triangle_count,
        size_t material_regions = 1) const noexcept;

    /**
     * @brief Validates if the given memory usage satisfies the idle budget (< 250 MB).
     */
    [[nodiscard]] bool validate_idle_budget(double measured_rss_mb) const noexcept;

    /**
     * @brief Validates if the given memory usage satisfies the 100k-vertex scene budget (< 800 MB).
     */
    [[nodiscard]] bool validate_scene_budget(size_t vertex_count, double measured_rss_mb) const noexcept;

private:
    MemoryBudgetLimits limits_;
};

} // namespace shunya::perf
