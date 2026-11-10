#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/integ/state_checkpoint.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <vector>

namespace shunya::integ {

struct DomainStepFeedback {
    bool converged{true};
    double lte_ratio{0.0};
    double max_safe_dt_s{1e9};
};

struct RegisteredDomain {
    std::string id;
    double frequency_hz{1000.0};
    uint8_t priority{0};
    std::function<core::base::Result<DomainStepFeedback>(double t_s, double dt_s)> step_fn;
    std::function<std::vector<double>()> capture_state_fn;
    std::function<void(const std::vector<double>&)> restore_state_fn;

    uint64_t tick_count{0};
    double last_step_time_s{0.0};
};

struct ScheduledTickEvent {
    double target_time_s{0.0};
    uint8_t priority{0};
    size_t domain_index{0};

    [[nodiscard]] bool operator>(const ScheduledTickEvent& other) const noexcept {
        if (std::abs(target_time_s - other.target_time_s) > 1e-14) {
            return target_time_s > other.target_time_s;
        }
        return priority > other.priority;
    }
};

/**
 * @brief Prioritized multi-rate sub-clock scheduler with exact integer-tick rational timestamps (zero drift).
 */
class DomainScheduler {
public:
    [[nodiscard]] core::base::Result<size_t> register_domain(RegisteredDomain domain);
    [[nodiscard]] size_t domain_count() const noexcept { return domains_.size(); }
    [[nodiscard]] const RegisteredDomain& domain(size_t idx) const { return domains_.at(idx); }
    [[nodiscard]] RegisteredDomain& domain_mut(size_t idx) { return domains_.at(idx); }

    void reset_clocks(double start_time_s = 0.0);

    [[nodiscard]] bool empty() const noexcept { return pq_.empty(); }
    [[nodiscard]] const ScheduledTickEvent& peek() const { return pq_.top(); }
    [[nodiscard]] ScheduledTickEvent pop_and_reschedule(uint64_t next_tick);
    void schedule_after_rollback(double restored_time_s);

private:
    std::vector<RegisteredDomain> domains_;
    std::priority_queue<ScheduledTickEvent, std::vector<ScheduledTickEvent>, std::greater<>> pq_;
};

} // namespace shunya::integ
