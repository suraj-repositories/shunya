#include "shunya/bench/perf_regression_gate.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

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

    // Flag statistical regression if slowdown exceeds budget AND Welch t-stat > 1.7 (one-sided 95% confidence)
    eval.regression_detected = (eval.slowdown_fraction > max_allowed_slowdown) &&
                               (eval.welch_t_stat > 1.7);
    return eval;
}

// --- PerformanceRegressionTestSuite (PH4-02.04.003) ---

PerformanceRegressionTestSuite::PerformanceRegressionTestSuite(double max_allowed_slowdown)
    : max_allowed_slowdown_(max_allowed_slowdown) {}

void PerformanceRegressionTestSuite::set_threshold(double max_allowed_slowdown) noexcept {
    max_allowed_slowdown_ = max_allowed_slowdown;
}

double PerformanceRegressionTestSuite::threshold() const noexcept {
    return max_allowed_slowdown_;
}

void PerformanceRegressionTestSuite::register_baseline(const std::string& metric_name, std::vector<double> samples_ms) {
    gate_.record_baseline(metric_name, std::move(samples_ms));
}

void PerformanceRegressionTestSuite::record_candidate(const std::string& metric_name, std::vector<double> samples_ms) {
    candidates_[metric_name] = std::move(samples_ms);
}

void PerformanceRegressionTestSuite::record_single_candidate(const std::string& metric_name, double sample_ms) {
    candidates_[metric_name].push_back(sample_ms);
}

void PerformanceRegressionTestSuite::clear_candidates() noexcept {
    candidates_.clear();
}

RegressionSuiteResult PerformanceRegressionTestSuite::run_evaluation() const {
    RegressionSuiteResult result;
    result.total_benchmarks = gate_.baselines().size();

    for (const auto& [name, baseline_samples] : gate_.baselines()) {
        const auto it = candidates_.find(name);
        if (it == candidates_.end() || it->second.empty()) {
            continue;
        }

        PerfGateEvaluation eval = gate_.evaluate_candidate(name, it->second, max_allowed_slowdown_);
        result.evaluations.push_back(eval);

        if (eval.regression_detected) {
            result.passed = false;
            result.regressed_count++;
            std::string reason = "Benchmark '" + name + "' regressed by " +
                std::to_string(eval.slowdown_fraction * 100.0) + "% (baseline: " +
                std::to_string(eval.baseline_mean_ms) + " ms, candidate: " +
                std::to_string(eval.current_mean_ms) + " ms, threshold: " +
                std::to_string(max_allowed_slowdown_ * 100.0) + "%)";
            result.failure_reasons.push_back(std::move(reason));
        }
    }

    result.summary_report = "Evaluated " + std::to_string(result.evaluations.size()) +
        " metrics: " + (result.passed ? "PASSED (No regressions > " + std::to_string(max_allowed_slowdown_ * 100.0) + "%)"
                                      : "FAILED (" + std::to_string(result.regressed_count) + " regressions detected)");

    return result;
}

bool PerformanceRegressionTestSuite::check_and_gate_build(bool throw_on_failure) const {
    const auto result = run_evaluation();
    if (!result.passed && throw_on_failure) {
        std::string err = "Performance regression gate failed:\n";
        for (const auto& f : result.failure_reasons) {
            err += "  - " + f + "\n";
        }
        throw std::runtime_error(err);
    }
    return result.passed;
}

std::string PerformanceRegressionTestSuite::serialize_baselines_json() const {
    std::string json = "{\n  \"baselines\": {\n";
    bool first = true;
    for (const auto& [name, samples] : gate_.baselines()) {
        if (!first) {
            json += ",\n";
        }
        first = false;
        json += "    \"" + name + "\": [";
        for (size_t i = 0; i < samples.size(); ++i) {
            if (i > 0) json += ", ";
            json += std::to_string(samples[i]);
        }
        json += "]";
    }
    json += "\n  }\n}";
    return json;
}

bool PerformanceRegressionTestSuite::deserialize_baselines_json(const std::string& json_str) {
    // Basic parser for key-value float arrays in "baselines": { ... }
    size_t pos = json_str.find("\"baselines\"");
    if (pos == std::string::npos) return false;

    size_t open_brace = json_str.find('{', pos);
    if (open_brace == std::string::npos) return false;

    size_t idx = open_brace + 1;
    while (idx < json_str.size()) {
        size_t key_start = json_str.find('"', idx);
        if (key_start == std::string::npos) break;
        size_t key_end = json_str.find('"', key_start + 1);
        if (key_end == std::string::npos) break;

        std::string key = json_str.substr(key_start + 1, key_end - key_start - 1);
        size_t bracket_open = json_str.find('[', key_end);
        if (bracket_open == std::string::npos) break;
        size_t bracket_close = json_str.find(']', bracket_open);
        if (bracket_close == std::string::npos) break;

        std::string array_str = json_str.substr(bracket_open + 1, bracket_close - bracket_open - 1);
        std::vector<double> vals;
        size_t num_start = 0;
        while (num_start < array_str.size()) {
            while (num_start < array_str.size() && (array_str[num_start] == ' ' || array_str[num_start] == ',' || array_str[num_start] == '\n' || array_str[num_start] == '\r')) {
                num_start++;
            }
            if (num_start >= array_str.size()) break;
            size_t num_end = array_str.find_first_of(",]", num_start);
            if (num_end == std::string::npos) num_end = array_str.size();
            std::string token = array_str.substr(num_start, num_end - num_start);
            try {
                vals.push_back(std::stod(token));
            } catch (...) {}
            num_start = num_end + 1;
        }

        if (!vals.empty()) {
            register_baseline(key, std::move(vals));
        }

        idx = bracket_close + 1;
    }
    return true;
}

} // namespace shunya::bench
