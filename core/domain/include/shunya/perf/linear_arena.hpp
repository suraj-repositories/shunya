#pragma once

#include <cstddef>
#include <cstdint>

namespace shunya::perf {

/**
 * @brief Pre-allocated monotonic bump-pointer arena resetting in O(1) at simulation step boundaries.
 */
class LinearArenaAllocator {
public:
    explicit LinearArenaAllocator(size_t capacity_bytes = 4 * 1024 * 1024);
    ~LinearArenaAllocator();

    LinearArenaAllocator(const LinearArenaAllocator&) = delete;
    LinearArenaAllocator& operator=(const LinearArenaAllocator&) = delete;

    [[nodiscard]] void* allocate(size_t bytes, size_t alignment = 16) noexcept;
    void reset() noexcept;

    [[nodiscard]] size_t bytes_used() const noexcept { return offset_; }
    [[nodiscard]] size_t peak_bytes_used() const noexcept { return peak_offset_; }
    [[nodiscard]] size_t capacity_bytes() const noexcept { return capacity_; }
    [[nodiscard]] uint64_t allocation_count() const noexcept { return alloc_count_; }

private:
    uint8_t* buffer_{nullptr};
    size_t capacity_{0};
    size_t offset_{0};
    size_t peak_offset_{0};
    uint64_t alloc_count_{0};
};

} // namespace shunya::perf
