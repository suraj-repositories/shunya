#pragma once

#include "shunya/bench/tolerance_macros.hpp"

#include <functional>
#include <string>
#include <vector>

namespace shunya::bench {

struct CiValidationReport {
    int exit_code{0};
    size_t total_checks{0};
    size_t passed_checks{0};
    size_t failed_checks{0};
    double elapsed_ms{0.0};
    std::vector<ToleranceCheckResult> failures;
    std::string json_report;
};

/**
 * @brief Automated headless CI numerical accuracy harness emitting structured JSON reports and CI exit codes.
 */
class ScientificCiRunner {
public:
    void add_check(std::string name, double expected, std::function<double()> evaluator, double rel_tol = 1e-5);
    [[nodiscard]] CiValidationReport execute_all() const;
    void clear() noexcept { checks_.clear(); }

private:
    struct RegisteredCheck {
        std::string name;
        double expected{0.0};
        std::function<double()> evaluator;
        double rel_tol{1e-5};
    };

    std::vector<RegisteredCheck> checks_;
};

} // namespace shunya::bench
