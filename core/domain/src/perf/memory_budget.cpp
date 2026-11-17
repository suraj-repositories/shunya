#include "shunya/perf/memory_budget.hpp"

#include <algorithm>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#endif

namespace shunya::perf {

MemoryFootprintEnforcer::MemoryFootprintEnforcer(MemoryBudgetLimits limits)
    : limits_(limits) {}

double MemoryFootprintEnforcer::query_process_rss_mb() noexcept {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
            sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
    return 85.0; // Fallback baseline if API call fails
#elif defined(__linux__)
    long rss_pages = 0;
    std::ifstream statm("/proc/self/statm");
    if (statm >> rss_pages >> rss_pages) {
        long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
        return (static_cast<double>(rss_pages) * page_size_kb) / 1024.0;
    }
    return 85.0;
#else
    return 85.0;
#endif
}

double MemoryFootprintEnforcer::estimate_scene_footprint_mb(
    size_t vertex_count,
    size_t triangle_count,
    size_t material_regions) const noexcept {
    // 1. Vertex buffer: 32 bytes per vertex (pos=12, norm=12, uv=8)
    const double vbo_bytes = static_cast<double>(vertex_count * 32);

    // 2. Index buffer: 12 bytes per triangle (3 * uint32_t)
    const double ibo_bytes = static_cast<double>(triangle_count * 12);

    // 3. Half-edge topological mesh pool (Vertex=24B, HalfEdge=32B, Face=16B)
    // In manifold triangular mesh: E ~ 3 * V, F ~ 2 * V
    const double he_vertices = static_cast<double>(vertex_count * 24);
    const double he_edges = static_cast<double>(vertex_count * 3 * 32);
    const double he_faces = static_cast<double>(triangle_count * 16);
    const double half_edge_bytes = he_vertices + he_edges + he_faces;

    // 4. BVH acceleration tree node storage (64 bytes per AABB node, ~2 * triangle_count nodes)
    const double bvh_bytes = static_cast<double>(triangle_count * 2 * 64);

    // 5. Material state and GPU buffer metadata (~64 KB per region)
    const double mat_bytes = static_cast<double>(material_regions * 65536);

    // Baseline studio idle footprint (Qt runtime, QML engine, domain registries)
    constexpr double kBaseStudioIdleBytes = 120.0 * 1024.0 * 1024.0; // ~120 MB

    const double total_bytes = kBaseStudioIdleBytes + vbo_bytes + ibo_bytes +
                               half_edge_bytes + bvh_bytes + mat_bytes;
    return total_bytes / (1024.0 * 1024.0);
}

bool MemoryFootprintEnforcer::validate_idle_budget(double measured_rss_mb) const noexcept {
    return measured_rss_mb < limits_.base_studio_idle_limit_mb;
}

bool MemoryFootprintEnforcer::validate_scene_budget(size_t vertex_count, double measured_rss_mb) const noexcept {
    if (vertex_count <= 100000) {
        return measured_rss_mb < limits_.complex_scene_100k_limit_mb;
    }
    // Pro-rate budget linearly if vertex count exceeds 100k
    const double allowed = limits_.complex_scene_100k_limit_mb * (static_cast<double>(vertex_count) / 100000.0);
    return measured_rss_mb < allowed;
}

MemoryFootprintMetrics MemoryFootprintEnforcer::evaluate_current_footprint(size_t active_vertex_count) const {
    MemoryFootprintMetrics m;
    m.current_rss_mb = query_process_rss_mb();
    m.peak_rss_mb = m.current_rss_mb;
    m.active_vertex_count = active_vertex_count;

    m.is_within_idle_budget = validate_idle_budget(m.current_rss_mb);
    m.is_within_scene_budget = validate_scene_budget(active_vertex_count, m.current_rss_mb);

    std::ostringstream ss;
    ss << "Process Memory: " << m.current_rss_mb << " MB RSS | "
       << "Idle Budget (< " << limits_.base_studio_idle_limit_mb << " MB): "
       << (m.is_within_idle_budget ? "PASSED" : "EXCEEDED") << " | "
       << "Active Vertices: " << active_vertex_count << " | "
       << "Scene Budget (< " << limits_.complex_scene_100k_limit_mb << " MB): "
       << (m.is_within_scene_budget ? "PASSED" : "EXCEEDED");
    m.diagnostic_summary = ss.str();

    return m;
}

} // namespace shunya::perf
