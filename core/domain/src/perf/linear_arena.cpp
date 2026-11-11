#include "shunya/perf/linear_arena.hpp"

#include <algorithm>
#include <new>

namespace shunya::perf {

LinearArenaAllocator::LinearArenaAllocator(size_t capacity_bytes)
    : capacity_(std::max<size_t>(64, capacity_bytes)) {
    buffer_ = static_cast<uint8_t*>(::operator new(capacity_, std::align_val_t{64}));
}

LinearArenaAllocator::~LinearArenaAllocator() {
    if (buffer_) {
        ::operator delete(buffer_, std::align_val_t{64});
        buffer_ = nullptr;
    }
}

void* LinearArenaAllocator::allocate(size_t bytes, size_t alignment) noexcept {
    if (!buffer_ || bytes == 0) {
        return nullptr;
    }
    const size_t safe_align = (alignment == 0) ? 16 : alignment;
    const uintptr_t base_addr = reinterpret_cast<uintptr_t>(buffer_);
    const uintptr_t curr_addr = base_addr + offset_;
    const uintptr_t aligned_addr = (curr_addr + safe_align - 1) & ~(static_cast<uintptr_t>(safe_align) - 1);
    const size_t new_offset = static_cast<size_t>(aligned_addr - base_addr) + bytes;

    if (new_offset > capacity_) {
        return nullptr;
    }

    offset_ = new_offset;
    peak_offset_ = std::max(peak_offset_, offset_);
    ++alloc_count_;
    return reinterpret_cast<void*>(aligned_addr);
}

void LinearArenaAllocator::reset() noexcept {
    offset_ = 0;
}

} // namespace shunya::perf
