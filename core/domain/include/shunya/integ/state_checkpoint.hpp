#pragma once

#include "shunya/core/base/result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace shunya::integ {

struct DomainSnapshot {
    std::string domain_id;
    double timestamp_s{0.0};
    uint64_t tick_count{0};
    std::vector<double> state_values;
};

struct SystemCheckpoint {
    uint64_t checkpoint_id{0};
    double physical_time_s{0.0};
    double suggested_dt_s{0.0};
    std::vector<DomainSnapshot> domain_snapshots;
};

/**
 * @brief Captures lightweight multi-domain state snapshots for deterministic rollback recovery.
 */
class StateCheckpointManager {
public:
    uint64_t save_checkpoint(double time_s, double dt_s, std::vector<DomainSnapshot> snapshots);

    [[nodiscard]] core::base::Result<SystemCheckpoint> latest_checkpoint() const;
    [[nodiscard]] bool has_checkpoint() const noexcept { return !checkpoints_.empty(); }
    [[nodiscard]] size_t checkpoint_count() const noexcept { return checkpoints_.size(); }
    void clear() noexcept { checkpoints_.clear(); }

private:
    uint64_t next_id_{1};
    std::vector<SystemCheckpoint> checkpoints_;
    static constexpr size_t kMaxHistory = 32;
};

} // namespace shunya::integ
