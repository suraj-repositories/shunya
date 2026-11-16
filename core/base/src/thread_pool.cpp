#include "shunya/core/base/thread_pool.hpp"

namespace shunya::core::base {

ThreadPool::ThreadPool(std::size_t thread_count) {
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 2;
    }

    local_queues_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        local_queues_.push_back(std::make_unique<WorkerLocalQueue>());
    }

    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this, i);
    }
}

ThreadPool::~ThreadPool() {
    shutdown(true);
}

void ThreadPool::worker_loop(std::size_t worker_index) {
    while (true) {
        PrioritizedTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this, worker_index]() {
                if (shutdown_.load(std::memory_order_relaxed)) return true;
                if (!tasks_.empty()) return true;
                if (worker_index < local_queues_.size() && !local_queues_[worker_index]->tasks.empty()) return true;
                for (std::size_t i = 0; i < local_queues_.size(); ++i) {
                    if (i != worker_index && !local_queues_[i]->tasks.empty()) return true;
                }
                return false;
            });

            if (shutdown_.load(std::memory_order_relaxed) && tasks_.empty()) {
                bool any_local = false;
                for (const auto& lq : local_queues_) {
                    if (!lq->tasks.empty()) {
                        any_local = true;
                        break;
                    }
                }
                if (!any_local) return;
            }

            // 1. Try local queue
            if (worker_index < local_queues_.size() && !local_queues_[worker_index]->tasks.empty()) {
                task = std::move(local_queues_[worker_index]->tasks.front());
                local_queues_[worker_index]->tasks.pop_front();
            }
            // 2. Try global priority queue
            else if (!tasks_.empty()) {
                task = std::move(const_cast<PrioritizedTask&>(tasks_.top()));
                tasks_.pop();
            }
            // 3. Try work-stealing from peers
            else {
                for (std::size_t offset = 1; offset < local_queues_.size(); ++offset) {
                    std::size_t victim_idx = (worker_index + offset) % local_queues_.size();
                    if (!local_queues_[victim_idx]->tasks.empty()) {
                        task = std::move(local_queues_[victim_idx]->tasks.back());
                        local_queues_[victim_idx]->tasks.pop_back();
                        stolen_tasks_count_.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }

            if (!task.work && shutdown_.load(std::memory_order_relaxed)) {
                return;
            }

            if (task.work) {
                ++active_tasks_;
            }
        }

        if (task.work) {
            try {
                task.work();
            } catch (...) {
                // Prevent uncaught worker exceptions from terminating the thread pool
            }

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                --active_tasks_;
                if (tasks_.empty() && active_tasks_ == 0) {
                    bool any_local = false;
                    for (const auto& lq : local_queues_) {
                        if (!lq->tasks.empty()) {
                            any_local = true;
                            break;
                        }
                    }
                    if (!any_local) {
                        cv_idle_.notify_all();
                    }
                }
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
            while (!tasks_.empty()) {
                tasks_.pop();
            }
            for (auto& lq : local_queues_) {
                lq->tasks.clear();
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
        if (active_tasks_ != 0 || !tasks_.empty()) {
            return false;
        }
        for (const auto& lq : local_queues_) {
            if (!lq->tasks.empty()) {
                return false;
            }
        }
        return true;
    });
}

std::size_t ThreadPool::pending_tasks() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    std::size_t count = tasks_.size();
    for (const auto& lq : local_queues_) {
        count += lq->tasks.size();
    }
    return count;
}

} // namespace shunya::core::base
