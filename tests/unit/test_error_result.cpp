#include "tests/test_framework.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

using namespace shunya::core::base;

Result<double> safe_divide(double numerator, double denominator) {
    if (denominator == 0.0) {
        return Result<double>::err({
            .code = 101,
            .domain = ErrorDomain::Simulation,
            .severity = Severity::Error,
            .message = "Division by zero",
            .details = "Denominator value evaluated to exactly 0.0"
        });
    }
    return Result<double>::ok(numerator / denominator);
}

Result<void> validate_dimension(int dim) {
    if (dim < 1 || dim > 3) {
        return Result<void>::err({
            .code = 202,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Unsupported spatial dimension",
            .details = "Supported spatial dimensions are 1, 2, or 3"
        });
    }
    return Result<void>::ok();
}

TEST_CASE("FND-04: Result Value and Error Semantics") {
    auto res_ok = safe_divide(10.0, 2.0);
    CHECK(res_ok.is_ok());
    CHECK(!res_ok.is_err());
    CHECK(res_ok.value() == 5.0);
    CHECK(res_ok.value_or(0.0) == 5.0);

    auto res_err = safe_divide(10.0, 0.0);
    CHECK(!res_err.is_ok());
    CHECK(res_err.is_err());
    CHECK(res_err.value_or(99.0) == 99.0);
    CHECK(res_err.error().code == 101);
    CHECK(res_err.error().domain == ErrorDomain::Simulation);
    CHECK(res_err.error().severity == Severity::Error);
    CHECK(res_err.error().format().find("Division by zero") != std::string::npos);
}

TEST_CASE("FND-04: Void Result Handling") {
    auto v_ok = validate_dimension(2);
    CHECK(v_ok.is_ok());

    auto v_err = validate_dimension(5);
    CHECK(v_err.is_err());
    CHECK(v_err.error().code == 202);
    CHECK(v_err.error().domain == ErrorDomain::Validation);
}

TEST_CASE("FND-04: Validation Report Accumulation") {
    ValidationReport report;
    CHECK(report.is_valid());
    CHECK(!report.has_warnings());

    report.add_info("sim.timestep", "Using default timestep 1e-4 s");
    CHECK(report.is_valid());
    CHECK(!report.has_warnings());

    report.add_warning("material.copper", "Thermal conductivity operating range close to limit");
    CHECK(report.is_valid());
    CHECK(report.has_warnings());

    report.add_error("circuit.node3", "Floating electrical node detected", 301);
    CHECK(!report.is_valid());
    CHECK(report.issues().size() == 3);

    report.clear();
    CHECK(report.is_valid());
    CHECK(!report.has_warnings());
    CHECK(report.issues().empty());
}
