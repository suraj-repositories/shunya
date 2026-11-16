#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <span>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <algorithm>

namespace shunya::core::base {

/**
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
 *
 * Implements IMP-03 / OBS-03 / PH4-01.02.004: Decouples high-frequency simulation
 * stepping from UI / telemetry observation threads without mutex contention or allocations.
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

    void push_overwrite(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);

        if (head - tail >= capacity_) {
            tail_.store(tail + 1, std::memory_order_release);
        }

        buffer_[head & mask_] = item;
        head_.store(head + 1, std::memory_order_release);
    }

    void push_overwrite(T&& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);

        if (head - tail >= capacity_) {
            tail_.store(tail + 1, std::memory_order_release);
        }

        buffer_[head & mask_] = std::move(item);
        head_.store(head + 1, std::memory_order_release);
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

    [[nodiscard]] std::optional<T> peek() const noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);

        if (tail == head) {
            return std::nullopt;
        }

        return buffer_[tail & mask_];
    }

    [[nodiscard]] std::optional<T> read_latest() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head) {
            return std::nullopt;
        }

        return buffer_[(head - 1) & mask_];
    }

    size_t drain(std::span<T> out) noexcept {
        size_t count = 0;
        while (count < out.size()) {
            auto item = pop();
            if (!item.has_value()) {
                break;
            }
            out[count++] = std::move(*item);
        }
        return count;
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

/**
 * @brief Telemetry sample frame for a single scalar time-series channel.
 */
struct TelemetrySample {
    double timestamp{0.0};
    double value{0.0};
};

/**
 * @brief Multi-channel telemetry buffer for streaming high-frequency simulation metrics
 * to UI and oscilloscope displays at 120 FPS without locking the simulation loop.
 */
class MultiChannelTelemetryBuffer {
public:
    struct ChannelInfo {
        size_t index{0};
        std::string name;
        std::string unit;
    };

    explicit MultiChannelTelemetryBuffer(size_t default_channel_capacity = 2048)
        : default_capacity_(default_channel_capacity) {}

    size_t add_channel(std::string name, std::string unit = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t index = channels_.size();
        channels_.push_back(ChannelInfo{
            .index = index,
            .name = std::move(name),
            .unit = std::move(unit)
        });
        channel_buffers_.push_back(std::make_unique<RingBuffer<TelemetrySample>>(default_capacity_));
        return index;
    }

    [[nodiscard]] size_t channel_count() const noexcept {
        return channels_.size();
    }

    [[nodiscard]] std::optional<ChannelInfo> channel_info(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index < channels_.size()) {
            return channels_[index];
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<size_t> find_channel(std::string_view name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& ch : channels_) {
            if (ch.name == name) {
                return ch.index;
            }
        }
        return std::nullopt;
    }

    bool push(size_t channel_idx, double timestamp, double value) noexcept {
        if (channel_idx >= channel_buffers_.size()) {
            return false;
        }
        return channel_buffers_[channel_idx]->push(TelemetrySample{timestamp, value});
    }

    void push_overwrite(size_t channel_idx, double timestamp, double value) noexcept {
        if (channel_idx < channel_buffers_.size()) {
            channel_buffers_[channel_idx]->push_overwrite(TelemetrySample{timestamp, value});
        }
    }

    void push_frame(double timestamp, std::span<const double> values) noexcept {
        size_t count = std::min(values.size(), channel_buffers_.size());
        for (size_t i = 0; i < count; ++i) {
            channel_buffers_[i]->push_overwrite(TelemetrySample{timestamp, values[i]});
        }
    }

    std::optional<TelemetrySample> pop(size_t channel_idx) noexcept {
        if (channel_idx >= channel_buffers_.size()) {
            return std::nullopt;
        }
        return channel_buffers_[channel_idx]->pop();
    }

    [[nodiscard]] std::optional<TelemetrySample> latest(size_t channel_idx) const noexcept {
        if (channel_idx >= channel_buffers_.size()) {
            return std::nullopt;
        }
        return channel_buffers_[channel_idx]->read_latest();
    }

    size_t drain(size_t channel_idx, std::span<TelemetrySample> out) noexcept {
        if (channel_idx >= channel_buffers_.size()) {
            return 0;
        }
        return channel_buffers_[channel_idx]->drain(out);
    }

    [[nodiscard]] size_t size(size_t channel_idx) const noexcept {
        if (channel_idx >= channel_buffers_.size()) {
            return 0;
        }
        return channel_buffers_[channel_idx]->size();
    }

    [[nodiscard]] bool empty(size_t channel_idx) const noexcept {
        if (channel_idx >= channel_buffers_.size()) {
            return true;
        }
        return channel_buffers_[channel_idx]->empty();
    }

    void clear(size_t channel_idx) noexcept {
        if (channel_idx < channel_buffers_.size()) {
            channel_buffers_[channel_idx]->clear();
        }
    }

    void clear_all() noexcept {
        for (auto& buf : channel_buffers_) {
            buf->clear();
        }
    }

private:
    size_t default_capacity_{2048};
    std::vector<ChannelInfo> channels_;
    std::vector<std::unique_ptr<RingBuffer<TelemetrySample>>> channel_buffers_;
    mutable std::mutex mutex_;
};

} // namespace shunya::core::base
