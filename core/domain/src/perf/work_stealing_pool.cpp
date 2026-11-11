#include "shunya/perf/work_stealing_pool.hpp"
#include "shunya/perf/job_graph.hpp"

#include <algorithm>
#include <atomic>

namespace shunya::perf {

WorkStealingPool::WorkStealingPool(size_t num_threads) {
    const size_t n = (num_threads == 0)
                         ? std::max<size_t>(2, std::thread::hardware_concurrency())
                         : std::max<size_t>(1, num_threads);
    queues_.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        queues_.push_back(std::make_unique<WorkerQueue>());
    }
    workers_.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        workers_.emplace_back([this, i]() { worker_loop(i); });
    }
}

WorkStealingPool::~WorkStealingPool() {
    wait_idle();
    stop_.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(wake_mu_);
        wake_cv_.notify_all();
    }
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void WorkStealingPool::enqueue(std::function<void()> task) {
    if (!task) return;
    active_tasks_.fetch_add(1, std::memory_order_acq_rel);
    const size_t q_idx = next_queue_.fetch_add(1, std::memory_order_relaxed) % queues_.size();
    {
        std::lock_guard<std::mutex> lock(queues_[q_idx]->mu);
        queues_[q_idx]->deque.push_back(std::move(task));
    }
    std::lock_guard<std::mutex> wake_lock(wake_mu_);
    wake_cv_.notify_one();
}

bool WorkStealingPool::try_pop_or_steal(size_t self_idx, std::function<void()>& out_task) {
    const size_t n = queues_.size();
    // 1) Try local worker queue (LIFO for cache locality)
    {
        std::lock_guard<std::mutex> lock(queues_[self_idx % n]->mu);
        auto& dq = queues_[self_idx % n]->deque;
        if (!dq.empty()) {
            out_task = std::move(dq.back());
            dq.pop_back();
            return true;
        }
    }
    // 2) Try stealing from victim queues (FIFO from front)
    for (size_t offset = 1; offset < n; ++offset) {
        const size_t victim = (self_idx + offset) % n;
        std::unique_lock<std::mutex> lock(queues_[victim]->mu, std::try_to_lock);
        if (lock.owns_lock() && !queues_[victim]->deque.empty()) {
            out_task = std::move(queues_[victim]->deque.front());
            queues_[victim]->deque.pop_front();
            steals_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

void WorkStealingPool::run_task(std::function<void()>& task) {
    task();
    if (active_tasks_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        std::lock_guard<std::mutex> lock(wake_mu_);
        idle_cv_.notify_all();
    }
}

void WorkStealingPool::worker_loop(size_t self_idx) {
    while (!stop_.load(std::memory_order_acquire)) {
        std::function<void()> task;
        if (try_pop_or_steal(self_idx, task)) {
            run_task(task);
        } else {
            std::unique_lock<std::mutex> lock(wake_mu_);
            wake_cv_.wait_for(lock, std::chrono::microseconds(200), [&]() {
                return stop_.load(std::memory_order_acquire) ||
                       active_tasks_.load(std::memory_order_acquire) > 0;
            });
        }
    }
}

void WorkStealingPool::wait_idle() {
    while (active_tasks_.load(std::memory_order_acquire) > 0) {
        std::function<void()> task;
        if (try_pop_or_steal(0, task)) {
            run_task(task);
        } else {
            std::unique_lock<std::mutex> lock(wake_mu_);
            idle_cv_.wait_for(lock, std::chrono::microseconds(100), [&]() {
                return active_tasks_.load(std::memory_order_acquire) == 0;
            });
        }
    }
}

size_t JobGraph::add_node(std::function<void()> task, std::vector<size_t> dependencies) {
    const size_t id = nodes_.size();
    nodes_.push_back(JobNodeSpec{std::move(task), std::move(dependencies)});
    return id;
}

void JobGraph::execute(WorkStealingPool& pool) {
    if (nodes_.empty()) return;
    const size_t n = nodes_.size();
    std::vector<std::vector<size_t>> dependents(n);
    std::vector<std::atomic<int>> unresolved(n);
    for (size_t i = 0; i < n; ++i) {
        unresolved[i].store(static_cast<int>(nodes_[i].dependencies.size()), std::memory_order_relaxed);
        for (size_t dep : nodes_[i].dependencies) {
            if (dep < n) dependents[dep].push_back(i);
        }
    }

    std::atomic<size_t> remaining{n};
    std::mutex done_mu;
    std::condition_variable done_cv;

    std::function<void(size_t)> submit_node = [&](size_t idx) {
        pool.enqueue([&, idx]() {
            if (nodes_[idx].task) nodes_[idx].task();
            for (size_t child : dependents[idx]) {
                if (unresolved[child].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    submit_node(child);
                }
            }
            if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> lock(done_mu);
                done_cv.notify_all();
            }
        });
    };

    for (size_t i = 0; i < n; ++i) {
        if (unresolved[i].load(std::memory_order_acquire) == 0) {
            submit_node(i);
        }
    }

    std::unique_lock<std::mutex> lock(done_mu);
    done_cv.wait(lock, [&]() { return remaining.load(std::memory_order_acquire) == 0; });
}

} // namespace shunya::perf
