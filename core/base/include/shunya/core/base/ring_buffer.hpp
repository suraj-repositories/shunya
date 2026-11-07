#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <span>
#include <vector>

namespace shunya::core::base {

/**
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
 *
 * Implements IMP-03 / OBS-03: Decouples high-frequency simulation stepping
 * from UI / telemetry observation threads without mutex contention or allocations.
 */
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity = 1024)
        : capacity_(round_up_pow2(capacity < 2 ? 2 : capacity)),
          mask_(capacity_ - 1),
          buffer_(capacity_) {}

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return head >= tail ? (head - tail) : 0;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return size() >= capacity_;
    }

    bool push(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);

        if (head - tail >= capacity_) {
            return false; // Buffer full
        }

        buffer_[head & mask_] = item;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool push(T&& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);

        if (head - tail >= capacity_) {
            return false;
        }

        buffer_[head & mask_] = std::move(item);
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);

        if (tail == head) {
            return std::nullopt; // Buffer empty
        }

        T item = std::move(buffer_[tail & mask_]);
        tail_.store(tail + 1, std::memory_order_release);
        return item;
    }

    void clear() noexcept {
        tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

private:
    static size_t round_up_pow2(size_t v) noexcept {
        --v;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        if constexpr (sizeof(size_t) > 4) {
            v |= v >> 32;
        }
        return v + 1;
    }

    const size_t capacity_;
    const size_t mask_;
    std::vector<T> buffer_;

    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

} // namespace shunya::core::base
