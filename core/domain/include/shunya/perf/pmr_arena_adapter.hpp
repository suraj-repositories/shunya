#pragma once

#include "shunya/perf/linear_arena.hpp"

#include <memory_resource>

namespace shunya::perf {

/**
 * @brief C++20 std::pmr::memory_resource adapter backed by a LinearArenaAllocator.
 */
class PmrArenaResource final : public std::pmr::memory_resource {
public:
    explicit PmrArenaResource(LinearArenaAllocator& arena) noexcept : arena_(arena) {}

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        void* p = arena_.allocate(bytes, alignment);
        if (!p) {
            throw std::bad_alloc();
        }
        return p;
    }

    void do_deallocate(void* /*p*/, size_t /*bytes*/, size_t /*alignment*/) noexcept override {
        // Linear arena frees all allocations in O(1) via LinearArenaAllocator::reset()
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        const auto* rhs = dynamic_cast<const PmrArenaResource*>(&other);
        return rhs != nullptr && (&rhs->arena_ == &arena_);
    }

private:
    LinearArenaAllocator& arena_;
};

} // namespace shunya::perf
