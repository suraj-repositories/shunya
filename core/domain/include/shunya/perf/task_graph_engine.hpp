#pragma once

#include <shunya/core/base/result.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace shunya::perf {

class WorkStealingPool;

enum class TaskDomain : uint8_t {
    General = 0,
    PhysicsBody,
    CircuitMatrix,
    MeshTessellation,
    ThermalField,
    RayTracing
};

[[nodiscard]] constexpr const char* to_string(TaskDomain domain) noexcept {
    switch (domain) {
        case TaskDomain::PhysicsBody: return "PhysicsBody";
        case TaskDomain::CircuitMatrix: return "CircuitMatrix";
        case TaskDomain::MeshTessellation: return "MeshTessellation";
        case TaskDomain::ThermalField: return "ThermalField";
        case TaskDomain::RayTracing: return "RayTracing";
        case TaskDomain::General: default: return "General";
    }
}

enum class TaskStatus : uint8_t {
    Pending = 0,
    Running,
    Completed,
    Failed
};

struct TaskNodeSpec {
    size_t id{0};
    std::string name;
    TaskDomain domain{TaskDomain::General};
    std::function<void()> work;
    std::vector<size_t> dependencies;
    int priority{0};
    double execution_time_us{0.0};
    TaskStatus status{TaskStatus::Pending};
};

struct TaskGraphMetrics {
    size_t total_tasks{0};
    double total_wall_time_us{0.0};
    double critical_path_time_us{0.0};
    double domain_times_us[6]{0.0}; // Indexed by TaskDomain
    size_t completed_tasks{0};
    size_t failed_tasks{0};
    double parallelism_speedup{1.0};
};

/**
 * @brief High-performance directed-acyclic task graph execution engine.
 * 
 * Capable of parallelizing decoupled physics bodies, circuit matrix solves, and
 * mesh tessellation across work-stealing worker threads with strict topological ordering,
 * cycle validation, and microsecond-level domain telemetry.
 */
class TaskGraphExecutionEngine {
public:
    TaskGraphExecutionEngine();
    ~TaskGraphExecutionEngine();

    /**
     * @brief Add a computational task node to the graph.
     * @param name Descriptive label for profiling and diagnostics.
     * @param domain Domain classification (PhysicsBody, CircuitMatrix, MeshTessellation, etc.)
     * @param work Function to execute.
     * @param dependencies Indices of predecessor tasks that must complete first.
     * @param priority Priority hint.
     * @return Assigned unique task node index.
     */
    size_t add_task(std::string name, TaskDomain domain,
                    std::function<void()> work,
                    std::vector<size_t> dependencies = {},
                    int priority = 0);

    [[nodiscard]] size_t task_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] const TaskNodeSpec& task(size_t id) const { return nodes_.at(id); }

    /**
     * @brief Check whether the graph contains any cyclic dependencies.
     */
    [[nodiscard]] bool has_cycle() const;

    /**
     * @brief Return the topologically sorted execution order.
     */
    [[nodiscard]] shunya::core::base::Result<std::vector<size_t>> topological_sort() const;

    /**
     * @brief Execute the task graph concurrently using the provided WorkStealingPool.
     * Ensures all dependencies are strictly satisfied, tracks per-domain latency,
     * and reports detailed execution metrics.
     */
    shunya::core::base::Result<TaskGraphMetrics> execute(WorkStealingPool& pool);

    /**
     * @brief Reset task execution states, allowing reusable graph instances across time steps.
     */
    void reset();

    /**
     * @brief Clear all nodes and dependencies.
     */
    void clear();

    /**
     * @brief Query total tasks associated with a particular domain.
     */
    [[nodiscard]] size_t domain_task_count(TaskDomain domain) const noexcept;

    /**
     * @brief Retrieve metrics from the most recent graph execution.
     */
    [[nodiscard]] const TaskGraphMetrics& last_metrics() const noexcept { return last_metrics_; }

private:
    std::vector<TaskNodeSpec> nodes_;
    TaskGraphMetrics last_metrics_{};
};

} // namespace shunya::perf
