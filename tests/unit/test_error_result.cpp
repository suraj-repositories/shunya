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

TEST_CASE("PH4-01.02.001: Result Monadic Chaining (and_then, or_else, transform)") {
    // 1. transform on Ok
    auto r1 = safe_divide(20.0, 4.0).transform([](double v) { return v * 2.0; });
    REQUIRE(r1.is_ok());
    CHECK(r1.value() == 10.0);

    // 2. transform on Err
    auto r2 = safe_divide(20.0, 0.0).transform([](double v) { return v * 2.0; });
    CHECK(r2.is_err());
    CHECK(r2.error().code == 101);

    // 3. and_then on Ok -> Ok
    auto r3 = safe_divide(100.0, 2.0).and_then([](double v) { return safe_divide(v, 5.0); });
    REQUIRE(r3.is_ok());
    CHECK(r3.value() == 10.0);

    // 4. and_then on Ok -> Err
    auto r4 = safe_divide(100.0, 2.0).and_then([](double v) { return safe_divide(v, 0.0); });
    CHECK(r4.is_err());
    CHECK(r4.error().code == 101);

    // 5. and_then on Err preserves initial error
    auto r5 = safe_divide(100.0, 0.0).and_then([](double v) { return safe_divide(v, 2.0); });
    CHECK(r5.is_err());
    CHECK(r5.error().code == 101);

    // 6. or_else recovery on Err
    auto r6 = safe_divide(100.0, 0.0).or_else([](const Error&) { return Result<double>::ok(-1.0); });
    REQUIRE(r6.is_ok());
    CHECK(r6.value() == -1.0);

    // 7. or_else bypass on Ok
    auto r7 = safe_divide(100.0, 5.0).or_else([](const Error&) { return Result<double>::ok(-1.0); });
    REQUIRE(r7.is_ok());
    CHECK(r7.value() == 20.0);

    // 8. Result<void> transform and and_then
    bool side_effect = false;
    auto v1 = validate_dimension(2).transform([&]() { side_effect = true; });
    CHECK(v1.is_ok());
    CHECK(side_effect);

    auto v2 = validate_dimension(2).and_then([]() { return Result<int>::ok(42); });
    REQUIRE(v2.is_ok());
    CHECK(v2.value() == 42);

    auto v3 = validate_dimension(99).and_then([]() { return Result<int>::ok(42); });
    CHECK(v3.is_err());
}
