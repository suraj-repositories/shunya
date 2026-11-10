#pragma once

#include <limits>
#include <string>
#include <vector>

namespace shunya::integ {

struct DomainStepProposal {
    std::string domain_id;
    double lte_ratio{0.0}; // estimated_error / tolerance (<= 1.0 means acceptable)
    double cfl_dt_s{std::numeric_limits<double>::infinity()};
    double desired_dt_s{std::numeric_limits<double>::infinity()};
};

/**
 * @brief Negotiates unified master step dt across coupled physics domains via LTE & CFL limits.
 */
class AdaptiveStepNegotiator {
public:
    struct Config {
        double safety_factor{0.85};
        double max_growth_factor{1.25}; // Smooth upward ramp prevents step chattering
        double min_shrink_factor{1e-4}; // Allows >3 orders of magnitude instant contraction
        double min_dt_s{1e-9};
        double max_dt_s{1.0};
    };

    AdaptiveStepNegotiator();
    explicit AdaptiveStepNegotiator(Config cfg);

    [[nodiscard]] double negotiate_dt(
        double current_dt_s,
        const std::vector<DomainStepProposal>& proposals) const noexcept;

    [[nodiscard]] const Config& config() const noexcept { return cfg_; }

private:
    Config cfg_;
};

} // namespace shunya::integ
