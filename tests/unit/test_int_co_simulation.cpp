#include "tests/test_framework.hpp"

#include "shunya/integ/cfl_calculator.hpp"
#include "shunya/integ/master_co_simulation.hpp"

#include <cmath>
#include <limits>
#include <vector>

using namespace shunya::integ;
using shunya::core::base::Result;

TEST_CASE("INT-01-001 Multi-rate domain scheduling across 100 kHz, 1 kHz, and 10 Hz clocks") {
    MasterCoSimulationExecutive exec;

    double last_circuit_t = 0.0;
    double last_mech_t = 0.0;
    double last_therm_t = 0.0;

    REQUIRE(exec.add_domain(RegisteredDomain{
        "circuit", 100000.0, 0,
        [&](double t, double /*dt*/) -> Result<DomainStepFeedback> {
            last_circuit_t = t;
            return DomainStepFeedback{true, 0.1, 1e9};
        },
        nullptr, nullptr
    }).is_ok());

    REQUIRE(exec.add_domain(RegisteredDomain{
        "mechanics", 1000.0, 1,
        [&](double t, double /*dt*/) -> Result<DomainStepFeedback> {
            last_mech_t = t;
            return DomainStepFeedback{true, 0.1, 1e9};
        },
        nullptr, nullptr
    }).is_ok());

    REQUIRE(exec.add_domain(RegisteredDomain{
        "thermal", 10.0, 2,
        [&](double t, double /*dt*/) -> Result<DomainStepFeedback> {
            last_therm_t = t;
            return DomainStepFeedback{true, 0.1, 1e9};
        },
        nullptr, nullptr
    }).is_ok());

    // Run 0.1 s with 10 ms barriers -> 10,000 circuit, 100 mechanics, 1 thermal steps with zero jitter
    auto report = exec.run_multi_rate(0.1, 0.01);
    REQUIRE(report.is_ok());
    REQUIRE(report.value().domain_step_counts.size() == 3);
    CHECK(report.value().domain_step_counts[0] == 10000);
    CHECK(report.value().domain_step_counts[1] == 100);
    CHECK(report.value().domain_step_counts[2] == 1);
    CHECK(std::abs(last_circuit_t - 0.1) < 1e-11);
    CHECK(std::abs(last_mech_t - 0.1) < 1e-11);
    CHECK(std::abs(last_therm_t - 0.1) < 1e-11);

    // 100 kHz vs 100 Hz over 10 ms gives exact 1000 : 1 step ratio
    MasterCoSimulationExecutive exec_ratio;
    REQUIRE(exec_ratio.add_domain(RegisteredDomain{
        "spice", 100000.0, 0,
        [](double, double) -> Result<DomainStepFeedback> { return DomainStepFeedback{true, 0.0, 1e9}; },
        nullptr, nullptr
    }).is_ok());
    REQUIRE(exec_ratio.add_domain(RegisteredDomain{
        "thermal_macro", 100.0, 1,
        [](double, double) -> Result<DomainStepFeedback> { return DomainStepFeedback{true, 0.0, 1e9}; },
        nullptr, nullptr
    }).is_ok());
    auto rep2 = exec_ratio.run_multi_rate(0.01, 0.01);
    REQUIRE(rep2.is_ok());
    CHECK(rep2.value().domain_step_counts[0] == 1000);
    CHECK(rep2.value().domain_step_counts[1] == 1);
}

TEST_CASE("INT-01-001 Adaptive step negotiation contracts by >1000x on collision/switching and recovers") {
    AdaptiveStepNegotiator::Config cfg;
    cfg.min_dt_s = 1e-7;
    cfg.max_dt_s = 1e-2;
    AdaptiveStepNegotiator neg(cfg);

    // Normal quiescent flight: LTE small, expands smoothly by <= 1.25x
    const double dt0 = 1e-3;
    const double dt_quiescent = neg.negotiate_dt(dt0, {
        {"mech", 0.05, 1.0, 1e-2},
        {"elec", 0.05, 1.0, 1e-2}
    });
    CHECK(std::abs(dt_quiescent - 1.25e-3) < 1e-9);

    // Abrupt collision impact / hard switch: CFL drops to 5e-7 s -> contracts by > 1000x in 1 call
    const double dt_cfl_impact = CflCalculator::contact_oscillation_dt(0.01, 1.0e9, 20.0);
    const double dt_impact = neg.negotiate_dt(dt_quiescent, {
        {"mech_contact", 2000.0, dt_cfl_impact, 1e-2}
    });
    CHECK(dt_impact < 1e-6);
    CHECK((dt_quiescent / dt_impact) > 1000.0);

    // Smooth recovery after event clears: grows at most 1.25x per step (no chattering)
    const double dt_rebound = neg.negotiate_dt(dt_impact, {
        {"mech", 0.01, 1.0, 1e-2}
    });
    CHECK(dt_rebound <= 1.2501 * dt_impact);
}

TEST_CASE("INT-01-001 Deterministic checkpoint rollback & step halving on stiff singularity divergence") {
    MasterCoSimulationExecutive exec;

    // Stiff ODE y' = -lambda * y with explicit Euler; unstable when lambda * dt > 2
    // Inject deliberate NaN/divergence when dt > 0.004 after t >= 0.01
    double y = 1.0;
    bool singularity_triggered_once = false;

    REQUIRE(exec.add_domain(RegisteredDomain{
        "stiff_reactor", 100.0, 0,
        [&](double t_next, double dt) -> Result<DomainStepFeedback> {
            if (t_next > 0.015 && dt > 0.003) {
                singularity_triggered_once = true;
                y = std::numeric_limits<double>::quiet_NaN(); // Diverges on large step
                return DomainStepFeedback{false, 100.0, dt * 0.5};
            }
            // Stable analytical/implicit step when dt <= 0.003 (keeps proposed safe dt large so t=0.015 hits dt > 0.003)
            y = y / (1.0 + 10.0 * dt);
            return DomainStepFeedback{true, 0.2, 0.05};
        },
        [&]() { return std::vector<double>{y}; },
        [&](const std::vector<double>& s) { y = s.at(0); }
    }).is_ok());

    AdaptiveStepNegotiator::Config cfg;
    cfg.min_dt_s = 1e-6;
    cfg.max_dt_s = 0.01;
    AdaptiveStepNegotiator neg(cfg);

    auto rep = exec.run_adaptive_with_rollback(0.05, 0.01, neg);
    REQUIRE(rep.is_ok());
    CHECK(singularity_triggered_once);
    CHECK(rep.value().rollbacks_triggered >= 1);
    CHECK(std::isfinite(y));
    CHECK(std::abs(rep.value().final_time_s - 0.05) < 1e-9);
    // Expected analytical exp(-10 * 0.05) = 0.6065; backward Euler matches within ~5%
    CHECK(y > 0.55);
    CHECK(y < 0.70);
}
