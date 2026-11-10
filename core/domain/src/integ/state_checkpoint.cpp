#include "shunya/integ/state_checkpoint.hpp"
#include "shunya/integ/rollback_manager.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::integ {

uint64_t StateCheckpointManager::save_checkpoint(
    double time_s,
    double dt_s,
    std::vector<DomainSnapshot> snapshots) {
    SystemCheckpoint cp;
    cp.checkpoint_id = next_id_++;
    cp.physical_time_s = time_s;
    cp.suggested_dt_s = dt_s;
    cp.domain_snapshots = std::move(snapshots);
    if (checkpoints_.size() >= kMaxHistory) {
        checkpoints_.erase(checkpoints_.begin());
    }
    checkpoints_.push_back(std::move(cp));
    return checkpoints_.back().checkpoint_id;
}

core::base::Result<SystemCheckpoint> StateCheckpointManager::latest_checkpoint() const {
    if (checkpoints_.empty()) {
        return core::base::Error{
            401, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
            "No valid checkpoint available for rollback", ""
        };
    }
    return checkpoints_.back();
}

bool RollbackManager::is_state_valid(
    const std::vector<double>& state,
    double max_abs_bound) noexcept {
    for (double v : state) {
        if (!std::isfinite(v) || std::abs(v) > max_abs_bound) {
            return false;
        }
    }
    return true;
}

core::base::Result<RollbackOutcome> RollbackManager::attempt_rollback(
    const StateCheckpointManager& checkpoints,
    double current_dt_s,
    double min_dt_s,
    size_t consecutive_failures) {
    if (consecutive_failures > max_retries_) {
        return core::base::Error{
            402, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
            "Exceeded maximum solver rollback retry budget", ""
        };
    }
    auto cp = checkpoints.latest_checkpoint();
    if (!cp.is_ok()) {
        return cp.error();
    }
    const double next_dt = 0.5 * current_dt_s;
    if (next_dt < min_dt_s) {
        return core::base::Error{
            403, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
            "Adaptive step halved below minimum allowed dt_s during rollback", ""
        };
    }

    ++total_rollbacks_;
    RollbackOutcome out;
    out.rolled_back = true;
    out.restored_time_s = cp.value().physical_time_s;
    out.halved_dt_s = next_dt;
    out.retry_attempt = consecutive_failures;
    return out;
}

} // namespace shunya::integ
