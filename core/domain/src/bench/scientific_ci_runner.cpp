#include "shunya/bench/scientific_ci_runner.hpp"

#include <chrono>
#include <sstream>

namespace shunya::bench {

void ScientificCiRunner::add_check(
    std::string name,
    double expected,
    std::function<double()> evaluator,
    double rel_tol) {
    checks_.push_back(RegisteredCheck{std::move(name), expected, std::move(evaluator), rel_tol});
}

CiValidationReport ScientificCiRunner::execute_all() const {
    const auto t0 = std::chrono::steady_clock::now();
    CiValidationReport rep;
    rep.total_checks = checks_.size();

    for (const auto& chk : checks_) {
        const double actual = chk.evaluator ? chk.evaluator() : 0.0;
        auto res = check_relative_tolerance(chk.name, chk.expected, actual, chk.rel_tol);
        if (res.passed) {
            ++rep.passed_checks;
        } else {
            ++rep.failed_checks;
            rep.failures.push_back(std::move(res));
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    rep.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    rep.exit_code = (rep.failed_checks == 0) ? 0 : 1;

    std::ostringstream js;
    js << "{"
       << "\"exit_code\":" << rep.exit_code << ","
       << "\"total\":" << rep.total_checks << ","
       << "\"passed\":" << rep.passed_checks << ","
       << "\"failed\":" << rep.failed_checks << ","
       << "\"elapsed_ms\":" << rep.elapsed_ms << ","
       << "\"failures\":[";
    for (size_t i = 0; i < rep.failures.size(); ++i) {
        if (i > 0) js << ",";
        js << "{\"check\":\"" << rep.failures[i].check_name << "\","
           << "\"diagnostic\":\"" << rep.failures[i].diagnostic << "\"}";
    }
    js << "]}";
    rep.json_report = js.str();
    return rep;
}

} // namespace shunya::bench
