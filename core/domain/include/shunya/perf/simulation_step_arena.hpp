#pragma once

#include "shunya/perf/linear_arena.hpp"
#include "shunya/perf/pmr_arena_adapter.hpp"

#include <memory_resource>
#include <cstddef>
#include <cstdint>

namespace shunya::perf {

class SimulationStepArena;

/**
 * @brief RAII step scope guard that automatically resets the step arena in O(1) when destroyed.
 */
class ScopedStepAllocation {
public:
    explicit ScopedStepAllocation(SimulationStepArena& arena) noexcept;
    ~ScopedStepAllocation();

    ScopedStepAllocation(const ScopedStepAllocation&) = delete;
    ScopedStepAllocation& operator=(const ScopedStepAllocation&) = delete;
    ScopedStepAllocation(ScopedStepAllocation&& other) noexcept;
    ScopedStepAllocation& operator=(ScopedStepAllocation&& other) noexcept;

    [[nodiscard]] std::pmr::memory_resource* resource() noexcept;

private:
    SimulationStepArena* arena_{nullptr};
    bool active_{true};
};

/**
 * @brief Polymorphic memory resource (PMR) linear arena tailored for high-frequency
 *        transient simulation time-step allocations (MNA vectors, RK-substages, state vectors).
 */
class SimulationStepArena {
public:
    explicit SimulationStepArena(size_t capacity_bytes = 8 * 1024 * 1024);

    [[nodiscard]] ScopedStepAllocation scoped_step() noexcept {
        return ScopedStepAllocation(*this);
    }

    [[nodiscard]] std::pmr::memory_resource* resource() noexcept {
        return &pmr_resource_;
    }

    void reset_step() noexcept;

    [[nodiscard]] size_t current_step_bytes() const noexcept { return arena_.bytes_used(); }
    [[nodiscard]] size_t peak_step_bytes() const noexcept { return arena_.peak_bytes_used(); }
    [[nodiscard]] size_t capacity_bytes() const noexcept { return arena_.capacity_bytes(); }
    [[nodiscard]] uint64_t total_step_resets() const noexcept { return total_resets_; }
    [[nodiscard]] uint64_t total_allocations() const noexcept { return arena_.allocation_count(); }

private:
    LinearArenaAllocator arena_;
    PmrArenaResource pmr_resource_;
    uint64_t total_resets_{0};
};

} // namespace shunya::perf
