#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <vector>

namespace shunya::perf {

/**
 * @brief Chunked intrusive-free-list object pool with O(1) alloc/dealloc and strict cache-line alignment.
 */
template <typename T, size_t Alignment = 64>
class FixedSizePool {
    static_assert((Alignment & (Alignment - 1)) == 0 && Alignment >= alignof(void*),
                  "Alignment must be a power of two >= alignof(void*)");

    struct FreeNode {
        FreeNode* next;
    };

    static constexpr size_t kRawSlotMin = sizeof(T) > sizeof(FreeNode) ? sizeof(T) : sizeof(FreeNode);
    static constexpr size_t kAlign = (Alignment > alignof(T)) ? Alignment : alignof(T);
    static constexpr size_t kSlotStride = ((kRawSlotMin + kAlign - 1) / kAlign) * kAlign;

public:
    explicit FixedSizePool(size_t slots_per_chunk = 1024)
        : slots_per_chunk_(std::max<size_t>(16, slots_per_chunk)) {
        allocate_chunk();
    }

    ~FixedSizePool() {
        for (void* block : chunks_) {
            ::operator delete(block, std::align_val_t{kAlign});
        }
    }

    FixedSizePool(const FixedSizePool&) = delete;
    FixedSizePool& operator=(const FixedSizePool&) = delete;

    [[nodiscard]] void* allocate_raw() {
        if (!free_head_) {
            allocate_chunk();
        }
        FreeNode* node = free_head_;
        free_head_ = node->next;
        ++active_count_;
        return static_cast<void*>(node);
    }

    void deallocate_raw(void* ptr) noexcept {
        if (!ptr) return;
        auto* node = static_cast<FreeNode*>(ptr);
        node->next = free_head_;
        free_head_ = node;
        if (active_count_ > 0) --active_count_;
    }

    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        void* mem = allocate_raw();
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    void destroy(T* obj) noexcept {
        if (!obj) return;
        obj->~T();
        deallocate_raw(obj);
    }

    [[nodiscard]] size_t active_count() const noexcept { return active_count_; }
    [[nodiscard]] size_t chunk_count() const noexcept { return chunks_.size(); }
    [[nodiscard]] size_t total_capacity() const noexcept { return chunks_.size() * slots_per_chunk_; }
    static constexpr size_t slot_alignment() noexcept { return kAlign; }

private:
    void allocate_chunk() {
        const size_t chunk_bytes = slots_per_chunk_ * kSlotStride;
        auto* base = static_cast<uint8_t*>(::operator new(chunk_bytes, std::align_val_t{kAlign}));
        chunks_.push_back(base);

        // Link slots in ascending order into intrusive free-list
        for (size_t i = slots_per_chunk_; i > 0; --i) {
            auto* node = reinterpret_cast<FreeNode*>(base + (i - 1) * kSlotStride);
            node->next = free_head_;
            free_head_ = node;
        }
    }

    size_t slots_per_chunk_;
    std::vector<void*> chunks_;
    FreeNode* free_head_{nullptr};
    size_t active_count_{0};
};

} // namespace shunya::perf
