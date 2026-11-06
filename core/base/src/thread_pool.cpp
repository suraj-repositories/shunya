#include "shunya/core/base/thread_pool.hpp"

namespace shunya::core::base {

ThreadPool::ThreadPool(std::size_t thread_count) {
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 2;
    }

    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown(true);
}

void ThreadPool::worker_loop() {
    while (true) {
        PrioritizedTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this]() {
                return shutdown_.load(std::memory_order_relaxed) || !tasks_.empty();
            });

            if (shutdown_.load(std::memory_order_relaxed) && tasks_.empty()) {
                return;
            }

            task = std::move(const_cast<PrioritizedTask&>(tasks_.top()));
            tasks_.pop();
            ++active_tasks_;
        }

        if (task.work) {
            try {
                task.work();
            } catch (...) {
                // Prevent uncaught worker exceptions from terminating the thread pool
            }
        }

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            --active_tasks_;
            if (tasks_.empty() && active_tasks_ == 0) {
                cv_idle_.notify_all();
            }
        }
    }
}

void ThreadPool::shutdown(bool wait_for_all) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (shutdown_.load(std::memory_order_relaxed)) {
            return;
        }
        shutdown_.store(true, std::memory_order_relaxed);

        if (!wait_for_all) {
            // Drop remaining unexecuted tasks
            while (!tasks_.empty()) {
                tasks_.pop();
            }
        }
    }

    cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPool::wait_idle() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    cv_idle_.wait(lock, [this]() {
        return tasks_.empty() && active_tasks_ == 0;
    });
}

std::size_t ThreadPool::pending_tasks() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

} // namespace shunya::core::base
