#include "shunya/perf/simulation_step_arena.hpp"

namespace shunya::perf {

ScopedStepAllocation::ScopedStepAllocation(SimulationStepArena& arena) noexcept
    : arena_(&arena), active_(true) {}

ScopedStepAllocation::~ScopedStepAllocation() {
    if (active_ && arena_) {
        arena_->reset_step();
    }
}

ScopedStepAllocation::ScopedStepAllocation(ScopedStepAllocation&& other) noexcept
    : arena_(other.arena_), active_(other.active_) {
    other.active_ = false;
    other.arena_ = nullptr;
}

ScopedStepAllocation& ScopedStepAllocation::operator=(ScopedStepAllocation&& other) noexcept {
    if (this != &other) {
        if (active_ && arena_) {
            arena_->reset_step();
        }
        arena_ = other.arena_;
        active_ = other.active_;
        other.active_ = false;
        other.arena_ = nullptr;
    }
    return *this;
}

std::pmr::memory_resource* ScopedStepAllocation::resource() noexcept {
    return arena_ ? arena_->resource() : std::pmr::get_default_resource();
}

SimulationStepArena::SimulationStepArena(size_t capacity_bytes)
    : arena_(capacity_bytes), pmr_resource_(arena_) {}

void SimulationStepArena::reset_step() noexcept {
    arena_.reset();
    ++total_resets_;
}

} // namespace shunya::perf
