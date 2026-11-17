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

struct RegressionSuiteResult {
    bool passed{true};
    size_t total_benchmarks{0};
    size_t regressed_count{0};
    std::vector<PerfGateEvaluation> evaluations;
    std::vector<std::string> failure_reasons;
    std::string summary_report;
};

/**
 * @brief Statistical Welch Student's t-test performance gate blocking throughput regressions.
 */
class PerfRegressionGate {
public:
    void record_baseline(const std::string& name, std::vector<double> durations_ms);

    [[nodiscard]] PerfGateEvaluation evaluate_candidate(
        const std::string& name,
        const std::vector<double>& candidate_ms,
        double max_allowed_slowdown = 0.05) const;

    [[nodiscard]] const std::unordered_map<std::string, std::vector<double>>& baselines() const noexcept {
        return baselines_;
    }

private:
    std::unordered_map<std::string, std::vector<double>> baselines_;
};

/**
 * @brief Automated performance regression test suite failing any build where frame times or solve times exceed baselines by >10%.
 * (Roadmap task PH4-02.04.003)
 */
class PerformanceRegressionTestSuite {
public:
    explicit PerformanceRegressionTestSuite(double max_allowed_slowdown = 0.10);

    void set_threshold(double max_allowed_slowdown) noexcept;
    [[nodiscard]] double threshold() const noexcept;

    void register_baseline(const std::string& metric_name, std::vector<double> samples_ms);
    void record_candidate(const std::string& metric_name, std::vector<double> samples_ms);
    void record_single_candidate(const std::string& metric_name, double sample_ms);

    [[nodiscard]] RegressionSuiteResult run_evaluation() const;
    [[nodiscard]] bool check_and_gate_build(bool throw_on_failure = false) const;

    [[nodiscard]] std::string serialize_baselines_json() const;
    bool deserialize_baselines_json(const std::string& json_str);

    void clear_candidates() noexcept;

private:
    double max_allowed_slowdown_{0.10}; // Default: 10% maximum allowable regression
    PerfRegressionGate gate_;
    std::unordered_map<std::string, std::vector<double>> candidates_;
};

} // namespace shunya::bench
