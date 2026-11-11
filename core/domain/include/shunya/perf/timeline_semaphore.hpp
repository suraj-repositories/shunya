#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace shunya::perf {

/**
 * @brief Vulkan-style monotonic uint64 timeline semaphore synchronizing CPU/transfer/compute queues.
 */
class TimelineSemaphore {
public:
    explicit TimelineSemaphore(uint64_t initial_value = 0) noexcept : value_(initial_value) {}

    void signal(uint64_t new_value) noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        if (new_value > value_) {
            value_ = new_value;
            cv_.notify_all();
        }
    }

    void wait(uint64_t target_value) const noexcept {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [&]() { return value_ >= target_value; });
    }

    [[nodiscard]] bool poll(uint64_t target_value) const noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        return value_ >= target_value;
    }

    [[nodiscard]] uint64_t completed_value() const noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        return value_;
    }

private:
    mutable std::mutex mu_;
    mutable std::condition_variable cv_;
    uint64_t value_{0};
};

} // namespace shunya::perf
