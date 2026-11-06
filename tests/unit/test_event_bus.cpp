#include "tests/test_framework.hpp"
#include "shunya/core/base/event_bus.hpp"

#include <string>
#include <vector>

using namespace shunya::core::base;

struct SimulationStartedEvent {
    std::string experiment_name;
    double initial_time = 0.0;
};

struct SimulationStepEvent {
    uint64_t step_index = 0;
    double current_time = 0.0;
    double dt = 1e-3;
};

struct AlertEvent {
    std::string level;
    std::string message;
};

TEST_CASE("FND-06: Synchronous Event Subscription and Dispatch") {
    EventBus bus;
    int start_count = 0;
    std::string received_exp_name;

    SubscriptionId sub1 = bus.subscribe<SimulationStartedEvent>([&](const SimulationStartedEvent& e) {
        ++start_count;
        received_exp_name = e.experiment_name;
    });

    bus.publish_immediate(SimulationStartedEvent{"RC_Transient_Benchmark", 0.0});
    CHECK(start_count == 1);
    CHECK(received_exp_name == "RC_Transient_Benchmark");

    // Unsubscribe
    bool unsub_res = bus.unsubscribe(sub1);
    CHECK(unsub_res);

    bus.publish_immediate(SimulationStartedEvent{"Second_Experiment", 1.0});
    CHECK(start_count == 1); // Unsubscribed, must not receive second event
}

TEST_CASE("FND-06: Distinct Event Types Isolation") {
    EventBus bus;
    int sim_events = 0;
    int alert_events = 0;

    bus.subscribe<SimulationStepEvent>([&](const SimulationStepEvent&) {
        ++sim_events;
    });

    bus.subscribe<AlertEvent>([&](const AlertEvent&) {
        ++alert_events;
    });

    bus.publish_immediate(SimulationStepEvent{1, 0.001, 0.001});
    bus.publish_immediate(SimulationStepEvent{2, 0.002, 0.001});
    bus.publish_immediate(AlertEvent{"WARN", "Voltage spike on node 2"});

    CHECK(sim_events == 2);
    CHECK(alert_events == 1);
}

TEST_CASE("FND-06: FIFO Queued Event Batch Delivery") {
    EventBus bus;
    std::vector<uint64_t> received_steps;

    bus.subscribe<SimulationStepEvent>([&](const SimulationStepEvent& e) {
        received_steps.push_back(e.step_index);
    });

    bus.enqueue(SimulationStepEvent{10, 0.010, 0.001});
    bus.enqueue(SimulationStepEvent{11, 0.011, 0.001});
    bus.enqueue(SimulationStepEvent{12, 0.012, 0.001});

    CHECK(bus.queued_count() == 3);
    CHECK(received_steps.empty()); // Not dispatched yet

    std::size_t flushed = bus.flush_queue();
    CHECK(flushed == 3);
    CHECK(bus.queued_count() == 0);
    REQUIRE(received_steps.size() == 3);
    CHECK(received_steps[0] == 10);
    CHECK(received_steps[1] == 11);
    CHECK(received_steps[2] == 12);
}

TEST_CASE("FND-06: Re-entrant Event Dispatch") {
    EventBus bus;
    int alert_count = 0;

    bus.subscribe<AlertEvent>([&](const AlertEvent&) {
        ++alert_count;
    });

    // Handler that enqueues/publishes another event inside its callback
    bus.subscribe<SimulationStartedEvent>([&](const SimulationStartedEvent&) {
        bus.publish_immediate(AlertEvent{"INFO", "Started nested alert"});
    });

    bus.publish_immediate(SimulationStartedEvent{"TestRun", 0.0});
    CHECK(alert_count == 1);
}
