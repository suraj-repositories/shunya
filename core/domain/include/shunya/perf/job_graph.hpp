#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <vector>

namespace shunya::perf {

class WorkStealingPool;

struct JobNodeSpec {
    std::function<void()> task;
    std::vector<size_t> dependencies;
};

/**
 * @brief Lightweight directed-acyclic task graph executing ready jobs concurrently onto WorkStealingPool.
 */
class JobGraph {
public:
    size_t add_node(std::function<void()> task, std::vector<size_t> dependencies = {});
    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }

    void execute(WorkStealingPool& pool);

private:
    std::vector<JobNodeSpec> nodes_;
};

} // namespace shunya::perf
