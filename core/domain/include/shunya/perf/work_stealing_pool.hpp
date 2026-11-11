#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace shunya::perf {

/**
 * @brief Multi-core work-stealing task scheduler with per-worker LIFO/steal-FIFO deques and cooperative nested waits.
 */
class WorkStealingPool {
public:
    explicit WorkStealingPool(size_t num_threads = 0);
    ~WorkStealingPool();

    WorkStealingPool(const WorkStealingPool&) = delete;
    WorkStealingPool& operator=(const WorkStealingPool&) = delete;

    void enqueue(std::function<void()> task);

    template <typename Fn>
    void parallel_for(size_t begin, size_t end, size_t grain_size, Fn&& fn);

    void wait_idle();
    [[nodiscard]] size_t worker_count() const noexcept { return workers_.size(); }
    [[nodiscard]] uint64_t steals_count() const noexcept { return steals_.load(std::memory_order_relaxed); }

private:
    struct alignas(64) WorkerQueue {
        std::mutex mu;
        std::deque<std::function<void()>> deque;
    };

    bool try_pop_or_steal(size_t self_idx, std::function<void()>& out_task);
    void worker_loop(size_t self_idx);
    void run_task(std::function<void()>& task);

    std::vector<std::unique_ptr<WorkerQueue>> queues_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_tasks_{0};
    std::atomic<size_t> next_queue_{0};
    std::atomic<uint64_t> steals_{0};
    std::mutex wake_mu_;
    std::condition_variable wake_cv_;
    std::condition_variable idle_cv_;
};

template <typename Fn>
void WorkStealingPool::parallel_for(size_t begin, size_t end, size_t grain_size, Fn&& fn) {
    if (begin >= end) return;
    const size_t grain = (grain_size == 0) ? 1 : grain_size;
    const size_t total = end - begin;
    const size_t chunks = (total + grain - 1) / grain;
    if (chunks <= 1 || workers_.empty()) {
        for (size_t i = begin; i < end; ++i) fn(i);
        return;
    }

    std::atomic<size_t> remaining{chunks};
    std::mutex done_mu;
    std::condition_variable done_cv;

    for (size_t c = 0; c < chunks; ++c) {
        const size_t c_begin = begin + c * grain;
        const size_t c_end = std::min(end, c_begin + grain);
        enqueue([&, c_begin, c_end]() {
            for (size_t i = c_begin; i < c_end; ++i) {
                fn(i);
            }
            if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> lock(done_mu);
                done_cv.notify_all();
            }
        });
    }

    // Cooperative waiting: help execute queued work if called recursively inside a task
    while (remaining.load(std::memory_order_acquire) > 0) {
        std::function<void()> stolen;
        if (try_pop_or_steal(0, stolen)) {
            run_task(stolen);
        } else {
            std::unique_lock<std::mutex> lock(done_mu);
            done_cv.wait_for(lock, std::chrono::microseconds(100), [&]() {
                return remaining.load(std::memory_order_acquire) == 0;
            });
        }
    }
}

} // namespace shunya::perf
