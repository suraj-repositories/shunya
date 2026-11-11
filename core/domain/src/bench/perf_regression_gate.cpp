#include "shunya/bench/perf_regression_gate.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace shunya::bench {

namespace {

std::pair<double, double> mean_and_variance(const std::vector<double>& v) noexcept {
    if (v.empty()) return {0.0, 0.0};
    const double n = static_cast<double>(v.size());
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / n;
    if (v.size() < 2) return {mean, 0.0};
    double sq = 0.0;
    for (double x : v) {
        const double d = x - mean;
        sq += d * d;
    }
    return {mean, sq / (n - 1.0)};
}

} // namespace

void PerfRegressionGate::record_baseline(const std::string& name, std::vector<double> durations_ms) {
    baselines_[name] = std::move(durations_ms);
}

PerfGateEvaluation PerfRegressionGate::evaluate_candidate(
    const std::string& name,
    const std::vector<double>& candidate_ms,
    double max_allowed_slowdown) const {
    PerfGateEvaluation eval;
    eval.benchmark_name = name;

    const auto it = baselines_.find(name);
    if (it == baselines_.end() || it->second.empty() || candidate_ms.empty()) {
        return eval;
    }

    const auto [mean_b, var_b] = mean_and_variance(it->second);
    const auto [mean_c, var_c] = mean_and_variance(candidate_ms);

    eval.baseline_mean_ms = mean_b;
    eval.current_mean_ms = mean_c;
    eval.slowdown_fraction = (mean_b > 1e-12) ? ((mean_c - mean_b) / mean_b) : 0.0;

    const double se = std::sqrt(
        (var_b / static_cast<double>(it->second.size())) +
        (var_c / static_cast<double>(candidate_ms.size())));
    eval.welch_t_stat = (se > 1e-12) ? ((mean_c - mean_b) / se) : ((mean_c > mean_b) ? 100.0 : 0.0);

    // Flag statistical regression if slowdown exceeds budget (5%) AND Welch t-stat > 1.7 (one-sided 95% confidence)
    eval.regression_detected = (eval.slowdown_fraction > max_allowed_slowdown) &&
                               (eval.welch_t_stat > 1.7);
    return eval;
}

} // namespace shunya::bench
