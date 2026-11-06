#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <type_traits>
#include <cstdint>

namespace shunya::core::base {

enum class JobPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

class CancellationToken {
public:
    CancellationToken() : flag_(std::make_shared<std::atomic<bool>>(false)) {}

    void cancel() noexcept {
        flag_->store(true, std::memory_order_relaxed);
    }

    [[nodiscard]] bool is_cancelled() const noexcept {
        return flag_->load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        flag_->store(false, std::memory_order_relaxed);
    }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

class ThreadPool {
private:
    struct PrioritizedTask {
        JobPriority priority = JobPriority::Normal;
        uint64_t sequence = 0;
        std::function<void()> work;

        bool operator<(const PrioritizedTask& other) const noexcept {
            if (priority != other.priority) {
                return priority < other.priority; // higher priority comes first in max-heap
            }
            return sequence > other.sequence; // FIFO within same priority
        }
    };

public:
    explicit ThreadPool(std::size_t thread_count = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto submit(JobPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        using ReturnType = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(f), ...captured_args = std::forward<Args>(args)]() mutable {
                return std::invoke(func, std::move(captured_args)...);
            }
        );

        std::future<ReturnType> future = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (shutdown_.load(std::memory_order_relaxed)) {
                throw std::runtime_error("Cannot submit task to stopped ThreadPool");
            }

            tasks_.push(PrioritizedTask{
                .priority = priority,
                .sequence = next_sequence_++,
                .work = [task]() { (*task)(); }
            });
        }

        cv_.notify_one();
        return future;
    }

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) {
        return submit(JobPriority::Normal, std::forward<F>(f), std::forward<Args>(args)...);
    }

    void shutdown(bool wait_for_all = true);
    void wait_idle();

    [[nodiscard]] std::size_t thread_count() const noexcept { return workers_.size(); }
    [[nodiscard]] std::size_t pending_tasks() const;
    [[nodiscard]] bool is_shutdown() const noexcept { return shutdown_.load(std::memory_order_relaxed); }

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::priority_queue<PrioritizedTask> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::condition_variable cv_idle_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> active_tasks_{0};
    uint64_t next_sequence_{0};
};

} // namespace shunya::core::base
