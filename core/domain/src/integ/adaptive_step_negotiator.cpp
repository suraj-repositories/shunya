#include "shunya/integ/adaptive_step_negotiator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::integ {

AdaptiveStepNegotiator::AdaptiveStepNegotiator() = default;

AdaptiveStepNegotiator::AdaptiveStepNegotiator(Config cfg) : cfg_(cfg) {}

double AdaptiveStepNegotiator::negotiate_dt(
    double current_dt_s,
    const std::vector<DomainStepProposal>& proposals) const noexcept {
    const double dt_safe = std::clamp(current_dt_s, cfg_.min_dt_s, cfg_.max_dt_s);
    double target_dt = std::min(cfg_.max_dt_s, dt_safe * cfg_.max_growth_factor);

    for (const auto& p : proposals) {
        if (std::isfinite(p.cfl_dt_s) && p.cfl_dt_s > 0.0) {
            target_dt = std::min(target_dt, cfg_.safety_factor * p.cfl_dt_s);
        }
        if (std::isfinite(p.desired_dt_s) && p.desired_dt_s > 0.0) {
            target_dt = std::min(target_dt, p.desired_dt_s);
        }
        if (p.lte_ratio > 1e-12 && std::isfinite(p.lte_ratio)) {
            // Classic embedded RK / local truncation error step controller (order ~2)
            const double scale = cfg_.safety_factor * std::pow(1.0 / p.lte_ratio, 0.5);
            const double clamped_scale = std::clamp(scale, cfg_.min_shrink_factor, cfg_.max_growth_factor);
            target_dt = std::min(target_dt, dt_safe * clamped_scale);
        }
    }

    return std::clamp(target_dt, cfg_.min_dt_s, cfg_.max_dt_s);
}

} // namespace shunya::integ
