#pragma once

#include <cmath>
#include <sstream>
#include <string>

namespace shunya::bench {

struct ToleranceCheckResult {
    std::string check_name;
    double expected{0.0};
    double actual{0.0};
    double relative_error{0.0};
    double max_relative_tolerance{1e-5};
    bool passed{true};
    std::string diagnostic;
};

[[nodiscard]] inline ToleranceCheckResult check_relative_tolerance(
    std::string name,
    double expected,
    double actual,
    double rel_tol = 1e-5) {
    const double abs_err = std::abs(actual - expected);
    const double denom = std::max(1e-12, std::abs(expected));
    const double rel = abs_err / denom;
    const bool ok = std::isfinite(actual) && (rel <= rel_tol);
    std::ostringstream msg;
    if (!ok) {
        msg << name << " diverged: expected=" << expected
            << ", actual=" << actual << ", rel_err=" << rel
            << " > tol=" << rel_tol;
    }
    return ToleranceCheckResult{std::move(name), expected, actual, rel, rel_tol, ok, msg.str()};
}

} // namespace shunya::bench
