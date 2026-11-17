#include "shunya/perf/task_graph_engine.hpp"
#include "shunya/perf/work_stealing_pool.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace shunya::perf {

TaskGraphExecutionEngine::TaskGraphExecutionEngine() = default;
TaskGraphExecutionEngine::~TaskGraphExecutionEngine() = default;

size_t TaskGraphExecutionEngine::add_task(std::string name, TaskDomain domain,
                                          std::function<void()> work,
                                          std::vector<size_t> dependencies,
                                          int priority) {
    const size_t id = nodes_.size();
    nodes_.push_back(TaskNodeSpec{
        .id = id,
        .name = std::move(name),
        .domain = domain,
        .work = std::move(work),
        .dependencies = std::move(dependencies),
        .priority = priority,
        .execution_time_us = 0.0,
        .status = TaskStatus::Pending
    });
    return id;
}

bool TaskGraphExecutionEngine::has_cycle() const {
    auto sort_res = topological_sort();
    return !sort_res.is_ok();
}

shunya::core::base::Result<std::vector<size_t>> TaskGraphExecutionEngine::topological_sort() const {
    const size_t n = nodes_.size();
    if (n == 0) return std::vector<size_t>{};

    std::vector<size_t> in_degree(n, 0);
    std::vector<std::vector<size_t>> adj(n);

    for (size_t i = 0; i < n; ++i) {
        in_degree[i] = nodes_[i].dependencies.size();
        for (size_t dep : nodes_[i].dependencies) {
            if (dep >= n) {
                return shunya::core::base::Error{
                    .code = -1,
                    .domain = shunya::core::base::ErrorDomain::Simulation,
                    .severity = shunya::core::base::Severity::Error,
                    .message = "Dependency index out of range",
                    .details = ""
                };
            }
            adj[dep].push_back(i);
        }
    }

    std::queue<size_t> q;
    for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            q.push(i);
        }
    }

    std::vector<size_t> order;
    order.reserve(n);

    while (!q.empty()) {
        size_t u = q.front();
        q.pop();
        order.push_back(u);

        for (size_t v : adj[u]) {
            if (--in_degree[v] == 0) {
                q.push(v);
            }
        }
    }

    if (order.size() != n) {
        return shunya::core::base::Error{
            .code = -2,
            .domain = shunya::core::base::ErrorDomain::Simulation,
            .severity = shunya::core::base::Severity::Error,
            .message = "Cycle detected in task graph dependencies",
            .details = ""
        };
    }

    return order;
}

shunya::core::base::Result<TaskGraphMetrics> TaskGraphExecutionEngine::execute(WorkStealingPool& pool) {
    const size_t n = nodes_.size();
    if (n == 0) {
        last_metrics_ = TaskGraphMetrics{};
        return last_metrics_;
    }

    // Verify acyclic precondition
    auto sort_res = topological_sort();
    if (!sort_res.is_ok()) {
        return sort_res.error();
    }

    reset();

    struct ExecutionContext : std::enable_shared_from_this<ExecutionContext> {
        ExecutionContext(size_t count, std::vector<TaskNodeSpec>& nodes_ref)
            : n(count), nodes(&nodes_ref), dependents(count), unresolved(count), remaining(count) {
            for (auto& dt : domain_times_us) {
                dt.store(0.0, std::memory_order_relaxed);
            }
        }

        size_t n;
        std::vector<TaskNodeSpec>* nodes;
        std::vector<std::vector<size_t>> dependents;
        std::vector<std::atomic<int>> unresolved;
        std::atomic<size_t> remaining;
        std::atomic<size_t> completed{0};
        std::atomic<size_t> failed{0};
        std::atomic<double> domain_times_us[6];
        std::atomic<double> sum_task_duration_us{0.0};

        std::mutex mu;
        std::condition_variable cv;

        void submit(size_t idx, WorkStealingPool& pool) {
            auto self = shared_from_this();
            pool.enqueue([self, idx, &pool]() {
                auto& task_spec = (*self->nodes)[idx];
                task_spec.status = TaskStatus::Running;

                auto t0 = std::chrono::high_resolution_clock::now();
                try {
                    if (task_spec.work) {
                        task_spec.work();
                    }
                    task_spec.status = TaskStatus::Completed;
                    self->completed.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    task_spec.status = TaskStatus::Failed;
                    self->failed.fetch_add(1, std::memory_order_relaxed);
                }
                auto t1 = std::chrono::high_resolution_clock::now();
                double dur_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                task_spec.execution_time_us = dur_us;

                self->sum_task_duration_us.fetch_add(dur_us, std::memory_order_relaxed);
                size_t d_idx = static_cast<size_t>(task_spec.domain);
                if (d_idx < 6) {
                    double current = self->domain_times_us[d_idx].load(std::memory_order_relaxed);
                    while (!self->domain_times_us[d_idx].compare_exchange_weak(
                        current, current + dur_us, std::memory_order_relaxed)) {}
                }

                // Notify dependent child tasks
                for (size_t child : self->dependents[idx]) {
                    if (self->unresolved[child].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        self->submit(child, pool);
                    }
                }

                if (self->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    std::lock_guard<std::mutex> lock(self->mu);
                    self->cv.notify_all();
                }
            });
        }
    };

    auto ctx = std::make_shared<ExecutionContext>(n, nodes_);

    // Set up dependencies and dependents
    for (size_t i = 0; i < n; ++i) {
        ctx->unresolved[i].store(static_cast<int>(nodes_[i].dependencies.size()), std::memory_order_relaxed);
        for (size_t dep : nodes_[i].dependencies) {
            if (dep < n) {
                ctx->dependents[dep].push_back(i);
            }
        }
    }

    auto wall_t0 = std::chrono::high_resolution_clock::now();

    // Collect initial roots before submitting to avoid double-submission races
    std::vector<size_t> roots;
    roots.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (nodes_[i].dependencies.empty()) {
            roots.push_back(i);
        }
    }

    for (size_t root_idx : roots) {
        ctx->submit(root_idx, pool);
    }

    // Wait for all tasks to complete
    {
        std::unique_lock<std::mutex> lock(ctx->mu);
        ctx->cv.wait(lock, [&]() {
            return ctx->remaining.load(std::memory_order_acquire) == 0;
        });
    }

    auto wall_t1 = std::chrono::high_resolution_clock::now();
    double total_wall_us = std::chrono::duration<double, std::micro>(wall_t1 - wall_t0).count();
    if (total_wall_us <= 0.0) total_wall_us = 1.0;

    TaskGraphMetrics metrics{};
    metrics.total_tasks = n;
    metrics.total_wall_time_us = total_wall_us;
    metrics.completed_tasks = ctx->completed.load(std::memory_order_relaxed);
    metrics.failed_tasks = ctx->failed.load(std::memory_order_relaxed);

    double sum_task_time = ctx->sum_task_duration_us.load(std::memory_order_relaxed);
    metrics.parallelism_speedup = sum_task_time > 0.0 ? (sum_task_time / total_wall_us) : 1.0;

    for (size_t d = 0; d < 6; ++d) {
        metrics.domain_times_us[d] = ctx->domain_times_us[d].load(std::memory_order_relaxed);
    }

    last_metrics_ = metrics;
    return metrics;
}

void TaskGraphExecutionEngine::reset() {
    for (auto& node : nodes_) {
        node.status = TaskStatus::Pending;
        node.execution_time_us = 0.0;
    }
}

void TaskGraphExecutionEngine::clear() {
    nodes_.clear();
    last_metrics_ = TaskGraphMetrics{};
}

size_t TaskGraphExecutionEngine::domain_task_count(TaskDomain domain) const noexcept {
    size_t count = 0;
    for (const auto& node : nodes_) {
        if (node.domain == domain) {
            ++count;
        }
    }
    return count;
}

} // namespace shunya::perf
