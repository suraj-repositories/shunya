#pragma once

#include <vector>
#include <queue>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <type_traits>
#include <cstdint>
#include <optional>
#include <stdexcept>

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
public:
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

    // Cooperative cancellable task submission
    template <typename F, typename... Args>
    auto submit_cancellable(JobPriority priority, CancellationToken token, F&& f, Args&&... args) {
        using RawReturnType = typename std::invoke_result_t<F, Args...>;

        if constexpr (std::is_void_v<RawReturnType>) {
            auto promise = std::make_shared<std::promise<bool>>();
            std::future<bool> future = promise->get_future();

            auto work = [func = std::forward<F>(f), ...captured_args = std::forward<Args>(args),
                         token, promise]() mutable {
                if (token.is_cancelled()) {
                    promise->set_value(false); // skipped due to cancellation
                    return;
                }
                try {
                    std::invoke(func, std::move(captured_args)...);
                    promise->set_value(true);
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            };

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (shutdown_.load(std::memory_order_relaxed)) {
                    throw std::runtime_error("Cannot submit task to stopped ThreadPool");
                }

                tasks_.push(PrioritizedTask{
                    .priority = priority,
                    .sequence = next_sequence_++,
                    .work = std::move(work)
                });
            }

            cv_.notify_one();
            return future;
        } else {
            auto promise = std::make_shared<std::promise<std::optional<RawReturnType>>>();
            std::future<std::optional<RawReturnType>> future = promise->get_future();

            auto work = [func = std::forward<F>(f), ...captured_args = std::forward<Args>(args),
                         token, promise]() mutable {
                if (token.is_cancelled()) {
                    promise->set_value(std::nullopt); // skipped due to cancellation
                    return;
                }
                try {
                    promise->set_value(std::invoke(func, std::move(captured_args)...));
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            };

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (shutdown_.load(std::memory_order_relaxed)) {
                    throw std::runtime_error("Cannot submit task to stopped ThreadPool");
                }

                tasks_.push(PrioritizedTask{
                    .priority = priority,
                    .sequence = next_sequence_++,
                    .work = std::move(work)
                });
            }

            cv_.notify_one();
            return future;
        }
    }

    template <typename F, typename... Args>
    auto submit_cancellable(CancellationToken token, F&& f, Args&&... args) {
        return submit_cancellable(JobPriority::Normal, token, std::forward<F>(f), std::forward<Args>(args)...);
    }

    // Submit work targeted to a specific worker's local queue (facilitates work-stealing topology)
    template <typename F, typename... Args>
    auto submit_to_worker(std::size_t worker_id, JobPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        using ReturnType = typename std::invoke_result_t<F, Args...>;

        if (workers_.empty()) {
            throw std::runtime_error("No workers in ThreadPool");
        }
        worker_id %= workers_.size();

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

            local_queues_[worker_id]->tasks.push_back(PrioritizedTask{
                .priority = priority,
                .sequence = next_sequence_++,
                .work = [task]() { (*task)(); }
            });
        }

        cv_.notify_all();
        return future;
    }

    template <typename F, typename... Args>
    auto submit_to_worker(std::size_t worker_id, F&& f, Args&&... args) {
        return submit_to_worker(worker_id, JobPriority::Normal, std::forward<F>(f), std::forward<Args>(args)...);
    }

    void shutdown(bool wait_for_all = true);
    void wait_idle();

    [[nodiscard]] std::size_t thread_count() const noexcept { return workers_.size(); }
    [[nodiscard]] std::size_t pending_tasks() const;
    [[nodiscard]] bool is_shutdown() const noexcept { return shutdown_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::size_t stolen_tasks_count() const noexcept { return stolen_tasks_count_.load(std::memory_order_relaxed); }

private:
    struct WorkerLocalQueue {
        std::deque<PrioritizedTask> tasks;
    };

    void worker_loop(std::size_t worker_index);

    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<WorkerLocalQueue>> local_queues_;
    std::priority_queue<PrioritizedTask> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::condition_variable cv_idle_;
    std::atomic<bool> shutdown_{false};
    std::size_t active_tasks_{0};
    std::atomic<size_t> stolen_tasks_count_{0};
    uint64_t next_sequence_{0};
};

} // namespace shunya::core::base
