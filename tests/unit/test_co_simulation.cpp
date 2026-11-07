#include "tests/test_framework.hpp"
#include "shunya/core/domain/co_simulation.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::core::domain::cosm;

// =============================================================================
// EVT-01: Typed Event Channels & Super-Dense Timestamp Semantics
// =============================================================================

TEST_CASE("EVT-01: SuperDenseTime Ordering and Event Channels") {
    SuperDenseTime t1{0.5, 0};
    SuperDenseTime t2{0.5, 1};
    SuperDenseTime t3{0.6, 0};

    CHECK(t1 < t2);
    CHECK(t2 < t3);
    CHECK(t1 == (SuperDenseTime{0.5, 0}));

    EventChannel chan("fault_events", ChannelType::Discrete);
    CHECK(chan.name() == "fault_events");
    CHECK(chan.type() == ChannelType::Discrete);

    int event_count = 0;
    chan.subscribe([&](const TimedEvent& evt) {
        if (std::holds_alternative<std::string>(evt.payload)) {
            event_count++;
        }
    });

    TimedEvent e1{SuperDenseTime{0.1, 0}, "fault_events", std::string("OverCurrent"), 10};
    auto res1 = chan.publish(e1);
    CHECK(res1.is_ok());
    CHECK(event_count == 1);

    // Monotonicity test: posting event in the past should fail
    TimedEvent e_past{SuperDenseTime{0.05, 0}, "fault_events", std::string("EarlyFault"), 10};
    auto res_past = chan.publish(e_past);
    CHECK(res_past.is_err());
}

// =============================================================================
// EVT-02: Signal Sampling, Interpolation, Buffering and Rate Conversion
// =============================================================================

TEST_CASE("EVT-02: Signal Sampling, Interpolation, and Resampling") {
    SampleBuffer buf(100);
    // Linear ramp y = 2 * t
    buf.append(0.0, 0.0, 2.0);
    buf.append(1.0, 2.0, 2.0);
    buf.append(2.0, 4.0, 2.0);

    // Test linear interpolation at mid-point
    auto val_lin = buf.sample(0.5, InterpolationMethod::Linear);
    REQUIRE(val_lin.has_value());
    CHECK(std::abs(*val_lin - 1.0) < 1e-6);

    // Test zero-order hold
    auto val_zoh = buf.sample(0.5, InterpolationMethod::ZeroOrderHold);
    REQUIRE(val_zoh.has_value());
    CHECK(std::abs(*val_zoh - 0.0) < 1e-6);

    // Test cubic Hermite interpolation
    auto val_hermite = buf.sample(0.5, InterpolationMethod::CubicHermite);
    REQUIRE(val_hermite.has_value());
    CHECK(std::abs(*val_hermite - 1.0) < 1e-4);

    // Resample test (rate conversion from 1.0s interval to 0.25s)
    std::vector<SamplePoint> input_pts{
        {0.0, 0.0, 1.0},
        {1.0, 10.0, 1.0}
    };
    auto resampled = SignalRateConverter::resample(input_pts, 0.25, InterpolationMethod::Linear);
    CHECK(resampled.size() == 5u); // 0.0, 0.25, 0.50, 0.75, 1.00
    CHECK(std::abs(resampled[2].value - 5.0) < 1e-6); // at t = 0.5
}

// =============================================================================
// EVT-03: Domain Coupling Schedule & Communication Points
// =============================================================================

TEST_CASE("EVT-03: Domain Coupling Schedule and Communication Points") {
    CouplingScheduleConfig cfg;
    cfg.start_time = 0.0;
    cfg.stop_time = 0.1;
    cfg.communication_step = 0.02; // 20 ms
    cfg.mode = CouplingMode::GaussSeidelSerial;

    DomainCouplingSchedule schedule(cfg);
    auto comm_points = schedule.generate_communication_points();
    REQUIRE(comm_points.size() == 6u); // 0.00, 0.02, 0.04, 0.06, 0.08, 0.10
    CHECK(std::abs(comm_points[0] - 0.00) < 1e-9);
    CHECK(std::abs(comm_points[5] - 0.10) < 1e-9);
}

// =============================================================================
// EVT-04: Zero-Crossing Root-Finding and Discrete Event Re-anchoring
// =============================================================================

TEST_CASE("EVT-04: State Event Zero-Crossing Detection") {
    // Function: g(t, v) = t^2 - 4.0; Root is at t = 2.0
    auto indicator = [](double t, double /*state*/) {
        return t * t - 4.0;
    };

    auto root_res = ZeroCrossingDetector::find_zero_crossing(
        indicator,
        1.0, 0.0, // t0 = 1.0 (g = -3.0)
        3.0, 0.0, // t1 = 3.0 (g = +5.0)
        1e-8
    );

    REQUIRE(root_res.is_ok());
    CHECK(std::abs(root_res.value() - 2.0) < 1e-7);

    // Failure case: no zero-crossing in interval [3.0, 4.0]
    auto no_root = ZeroCrossingDetector::find_zero_crossing(
        indicator,
        3.0, 0.0,
        4.0, 0.0
    );
    CHECK(no_root.is_err());
}

// =============================================================================
// EVT-05: Co-Simulation Slave Adapters
// =============================================================================

TEST_CASE("EVT-05: Co-Simulation Slave Adapter Interface and Stepping") {
    MockCoSimSlave slave("SubsystemA", 0.1);
    CHECK(slave.name() == "SubsystemA");

    auto init_res = slave.initialize(0.0, 1.0);
    CHECK(init_res.is_ok());

    auto set_res = slave.set_real("u", 10.0);
    CHECK(set_res.is_ok());

    // With feedthrough gain = 0.1, y = 0.0 + 0.1 * 10 = 1.0 immediately
    auto y_pre = slave.get_real("y");
    REQUIRE(y_pre.is_ok());
    CHECK(std::abs(y_pre.value() - 1.0) < 1e-6);

    // Step by dt = 0.1s -> internal state += 10.0 * 0.1 = 1.0
    auto step_res = slave.do_step(0.0, 0.1);
    CHECK(step_res.is_ok());

    // y = state(1.0) + 0.1 * 10.0 = 2.0
    auto y_post = slave.get_real("y");
    REQUIRE(y_post.is_ok());
    CHECK(std::abs(y_post.value() - 2.0) < 1e-6);
}

// =============================================================================
// EVT-06: Co-Simulation Feedback Loop Resolution and Deterministic Replay
// =============================================================================

TEST_CASE("EVT-06: Algebraic Loop Resolution and Deterministic Replay") {
    CouplingScheduleConfig cfg;
    cfg.start_time = 0.0;
    cfg.stop_time = 0.05;
    cfg.communication_step = 0.01;

    DomainCouplingSchedule schedule(cfg);
    CoSimulationOrchestrator orchestrator(schedule);

    auto slave_a = std::make_shared<MockCoSimSlave>("SlaveA", 0.2);
    auto slave_b = std::make_shared<MockCoSimSlave>("SlaveB", 0.1);

    orchestrator.add_slave(slave_a);
    orchestrator.add_slave(slave_b);

    // Connect SlaveA.y -> SlaveB.u
    auto c1 = orchestrator.connect_ports(PortConnection{"SlaveA", "y", "SlaveB", "u"});
    CHECK(c1.is_ok());

    auto init_res = orchestrator.initialize();
    CHECK(init_res.is_ok());

    // Set initial input to SlaveA
    auto set_in = slave_a->set_real("u", 5.0);
    CHECK(set_in.is_ok());

    auto run_res = orchestrator.run_simulation();
    CHECK(run_res.is_ok());

    const auto& log1 = orchestrator.replay_log();
    REQUIRE(!log1.empty());

    // Replay determinism verification
    CHECK(orchestrator.verify_deterministic_replay(log1));
}
