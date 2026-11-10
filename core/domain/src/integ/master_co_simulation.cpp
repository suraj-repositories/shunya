#include "shunya/integ/master_co_simulation.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::integ {

core::base::Result<size_t> DomainScheduler::register_domain(RegisteredDomain domain) {
    if (domain.id.empty() || domain.frequency_hz <= 0.0 || !domain.step_fn) {
        return core::base::Error{
            411, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid co-simulation domain registration", ""
        };
    }
    const size_t idx = domains_.size();
    domains_.push_back(std::move(domain));
    pq_.push(ScheduledTickEvent{
        1.0 / domains_.back().frequency_hz,
        domains_.back().priority,
        idx
    });
    return idx;
}

void DomainScheduler::reset_clocks(double start_time_s) {
    while (!pq_.empty()) pq_.pop();
    for (size_t i = 0; i < domains_.size(); ++i) {
        domains_[i].tick_count = 0;
        domains_[i].last_step_time_s = start_time_s;
        pq_.push(ScheduledTickEvent{
            start_time_s + (1.0 / domains_[i].frequency_hz),
            domains_[i].priority,
            i
        });
    }
}

ScheduledTickEvent DomainScheduler::pop_and_reschedule(uint64_t next_tick) {
    const ScheduledTickEvent top = pq_.top();
    pq_.pop();
    auto& d = domains_[top.domain_index];
    d.tick_count = next_tick;
    d.last_step_time_s = top.target_time_s;
    const double following_t = static_cast<double>(next_tick + 1) / d.frequency_hz;
    pq_.push(ScheduledTickEvent{following_t, d.priority, top.domain_index});
    return top;
}

void DomainScheduler::schedule_after_rollback(double restored_time_s) {
    while (!pq_.empty()) pq_.pop();
    for (size_t i = 0; i < domains_.size(); ++i) {
        domains_[i].last_step_time_s = restored_time_s;
        const double next_t = restored_time_s + (1.0 / domains_[i].frequency_hz);
        pq_.push(ScheduledTickEvent{next_t, domains_[i].priority, i});
    }
}

core::base::Result<size_t> MasterCoSimulationExecutive::add_domain(RegisteredDomain domain) {
    return scheduler_.register_domain(std::move(domain));
}

core::base::Result<CoSimExecutionReport> MasterCoSimulationExecutive::run_multi_rate(
    double total_duration_s,
    double sync_barrier_dt_s) {
    if (total_duration_s <= 0.0 || sync_barrier_dt_s <= 0.0 || scheduler_.domain_count() == 0) {
        return core::base::Error{
            412, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid co-simulation duration, barrier, or zero registered domains", ""
        };
    }

    scheduler_.reset_clocks(0.0);
    clock_.reset(0.0);

    CoSimExecutionReport report;
    const size_t n_barriers = static_cast<size_t>(std::ceil(total_duration_s / sync_barrier_dt_s));

    for (size_t b = 1; b <= n_barriers; ++b) {
        const double t_barrier = std::min(total_duration_s, static_cast<double>(b) * sync_barrier_dt_s);
        while (!scheduler_.empty() && scheduler_.peek().target_time_s <= t_barrier + 1e-12) {
            const ScheduledTickEvent ev = scheduler_.peek();
            const size_t d_idx = ev.domain_index;
            auto& d = scheduler_.domain_mut(d_idx);
            const double dt_domain = 1.0 / d.frequency_hz;
            report.min_dt_observed_s = std::min(report.min_dt_observed_s, dt_domain);
            report.max_dt_observed_s = std::max(report.max_dt_observed_s, dt_domain);

            auto res = d.step_fn(ev.target_time_s, dt_domain);
            if (!res.is_ok()) {
                return res.error();
            }
            (void)scheduler_.pop_and_reschedule(d.tick_count + 1);
        }
        clock_.set_physical_time(t_barrier);
        ++report.macro_steps_executed;
    }

    report.final_time_s = clock_.physical_time();
    report.domain_step_counts.reserve(scheduler_.domain_count());
    for (size_t i = 0; i < scheduler_.domain_count(); ++i) {
        report.domain_step_counts.push_back(scheduler_.domain(i).tick_count);
    }
    return report;
}

core::base::Result<CoSimExecutionReport> MasterCoSimulationExecutive::run_adaptive_with_rollback(
    double total_duration_s,
    double initial_dt_s,
    const AdaptiveStepNegotiator& negotiator) {
    if (total_duration_s <= 0.0 || initial_dt_s <= 0.0 || scheduler_.domain_count() == 0) {
        return core::base::Error{
            413, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid adaptive co-simulation duration, initial dt, or zero domains", ""
        };
    }

    scheduler_.reset_clocks(0.0);
    clock_.reset(0.0);
    checkpoints_.clear();

    double t_curr = 0.0;
    double dt_curr = std::clamp(initial_dt_s, negotiator.config().min_dt_s, negotiator.config().max_dt_s);
    CoSimExecutionReport report;
    report.domain_step_counts.assign(scheduler_.domain_count(), 0);

    size_t consecutive_failures = 0;
    constexpr size_t kMaxSteps = 500000;

    for (size_t step_iter = 0; step_iter < kMaxSteps && t_curr < total_duration_s - 1e-12; ++step_iter) {
        // 1) Snapshot domain states before trying dt_try
        std::vector<DomainSnapshot> snaps;
        snaps.reserve(scheduler_.domain_count());
        for (size_t i = 0; i < scheduler_.domain_count(); ++i) {
            const auto& d = scheduler_.domain(i);
            DomainSnapshot snap;
            snap.domain_id = d.id;
            snap.timestamp_s = t_curr;
            snap.tick_count = d.tick_count;
            if (d.capture_state_fn) {
                snap.state_values = d.capture_state_fn();
            }
            snaps.push_back(std::move(snap));
        }
        checkpoints_.save_checkpoint(t_curr, dt_curr, std::move(snaps));

        const double dt_try = std::min(dt_curr, total_duration_s - t_curr);
        bool step_failed = false;
        std::vector<DomainStepProposal> proposals;
        proposals.reserve(scheduler_.domain_count());

        for (size_t i = 0; i < scheduler_.domain_count(); ++i) {
            auto& d = scheduler_.domain_mut(i);
            auto res = d.step_fn(t_curr + dt_try, dt_try);
            if (!res.is_ok() || !res.value().converged) {
                step_failed = true;
                break;
            }
            if (d.capture_state_fn && !RollbackManager::is_state_valid(d.capture_state_fn())) {
                step_failed = true;
                break;
            }
            proposals.push_back(DomainStepProposal{
                d.id,
                res.value().lte_ratio,
                res.value().max_safe_dt_s,
                negotiator.config().max_dt_s
            });
        }

        if (step_failed) {
            ++consecutive_failures;
            auto rb = rollback_.attempt_rollback(
                checkpoints_, dt_try, negotiator.config().min_dt_s, consecutive_failures);
            if (!rb.is_ok()) {
                return rb.error();
            }
            auto latest_cp = checkpoints_.latest_checkpoint();
            if (!latest_cp.is_ok()) {
                return latest_cp.error();
            }
            for (size_t i = 0; i < scheduler_.domain_count(); ++i) {
                auto& d = scheduler_.domain_mut(i);
                if (d.restore_state_fn) {
                    d.restore_state_fn(latest_cp.value().domain_snapshots[i].state_values);
                }
            }
            dt_curr = rb.value().halved_dt_s;
            ++report.rollbacks_triggered;
            continue;
        }

        consecutive_failures = 0;
        t_curr += dt_try;
        clock_.advance(dt_try);
        ++report.macro_steps_executed;
        report.min_dt_observed_s = std::min(report.min_dt_observed_s, dt_try);
        report.max_dt_observed_s = std::max(report.max_dt_observed_s, dt_try);
        for (size_t i = 0; i < scheduler_.domain_count(); ++i) {
            ++report.domain_step_counts[i];
        }

        // Negotiate next dt if not finishing at boundary
        if (t_curr < total_duration_s - 1e-12) {
            dt_curr = negotiator.negotiate_dt(dt_curr, proposals);
        }
    }

    report.final_time_s = t_curr;
    return report;
}

} // namespace shunya::integ
