#pragma once

#include "shunya/integ/state_checkpoint.hpp"

#include <cstddef>

namespace shunya::integ {

struct RollbackOutcome {
    bool rolled_back{false};
    double restored_time_s{0.0};
    double halved_dt_s{0.0};
    size_t retry_attempt{0};
};

/**
 * @brief Detects numerical divergence / non-convergence and drives checkpoint rollback with binary step halving.
 */
class RollbackManager {
public:
    explicit RollbackManager(size_t max_retries = 12) noexcept : max_retries_(max_retries) {}

    [[nodiscard]] static bool is_state_valid(const std::vector<double>& state, double max_abs_bound = 1e12) noexcept;

    [[nodiscard]] core::base::Result<RollbackOutcome> attempt_rollback(
        const StateCheckpointManager& checkpoints,
        double current_dt_s,
        double min_dt_s,
        size_t consecutive_failures);

    [[nodiscard]] size_t total_rollbacks() const noexcept { return total_rollbacks_; }
    [[nodiscard]] size_t max_retries() const noexcept { return max_retries_; }

private:
    size_t max_retries_{12};
    size_t total_rollbacks_{0};
};

} // namespace shunya::integ
