#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::bench {

struct PerfGateEvaluation {
    std::string benchmark_name;
    double baseline_mean_ms{0.0};
    double current_mean_ms{0.0};
    double slowdown_fraction{0.0};
    double welch_t_stat{0.0};
    bool regression_detected{false};
};

/**
 * @brief Statistical Welch Student's t-test performance gate blocking > 5% throughput regressions.
 */
class PerfRegressionGate {
public:
    void record_baseline(const std::string& name, std::vector<double> durations_ms);

    [[nodiscard]] PerfGateEvaluation evaluate_candidate(
        const std::string& name,
        const std::vector<double>& candidate_ms,
        double max_allowed_slowdown = 0.05) const;

private:
    std::unordered_map<std::string, std::vector<double>> baselines_;
};

} // namespace shunya::bench
