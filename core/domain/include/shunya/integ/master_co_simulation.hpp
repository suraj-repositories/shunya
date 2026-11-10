#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/simulation_clock.hpp"
#include "shunya/integ/adaptive_step_negotiator.hpp"
#include "shunya/integ/domain_scheduler.hpp"
#include "shunya/integ/rollback_manager.hpp"
#include "shunya/integ/state_checkpoint.hpp"

#include <string>
#include <vector>

namespace shunya::integ {

struct CoSimExecutionReport {
    double final_time_s{0.0};
    size_t macro_steps_executed{0};
    size_t rollbacks_triggered{0};
    double min_dt_observed_s{1e9};
    double max_dt_observed_s{0.0};
    std::vector<uint64_t> domain_step_counts;
};

/**
 * @brief Master multi-rate co-simulation executive coordinating asynchronous domain clocks, LTE/CFL adaptation, and rollback recovery.
 */
class MasterCoSimulationExecutive {
public:
    MasterCoSimulationExecutive() = default;

    [[nodiscard]] core::base::Result<size_t> add_domain(RegisteredDomain domain);

    /**
     * @brief Advances all multi-rate sub-clocks deterministically across fixed macro-steps with zero drift.
     */
    [[nodiscard]] core::base::Result<CoSimExecutionReport> run_multi_rate(
        double total_duration_s,
        double sync_barrier_dt_s);

    /**
     * @brief Advances synchronously with LTE/CFL adaptive step negotiation and automatic checkpoint rollback on divergence.
     */
    [[nodiscard]] core::base::Result<CoSimExecutionReport> run_adaptive_with_rollback(
        double total_duration_s,
        double initial_dt_s,
        const AdaptiveStepNegotiator& negotiator = AdaptiveStepNegotiator{});

    [[nodiscard]] const core::domain::SimulationClock& clock() const noexcept { return clock_; }

private:
    DomainScheduler scheduler_;
    StateCheckpointManager checkpoints_;
    RollbackManager rollback_;
    core::domain::SimulationClock clock_;
};

} // namespace shunya::integ
